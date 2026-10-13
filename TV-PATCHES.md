# FAKE-08 with the TV patches

This fork of [FAKE-08](https://github.com/jtothebell/fake-08) carries the
compatibility and speed patches used by
[FAKE-08 for Samsung TVs](https://github.com/giobauermeister/fake08_tv), which
uses the `tv-patches` branch as a git submodule.

FAKE-08 is MIT licensed, see [LICENSE.MD](LICENSE.MD). The patches are by
Giovanni Bauermeister, except that 0006 carries two upstream z8lua commits, and
are under the same licence.

## Where it starts

| Repository | Branch | Starts from |
|---|---|---|
| [giobauermeister/fake-08](https://github.com/giobauermeister/fake-08) | `tv-patches` | [jtothebell/fake-08](https://github.com/jtothebell/fake-08) `814991a2571ad3970e386cef48f3b148aa1c27b9` (`master`, "Merge pull request #267 from ysamlan/fix-vibrato-modulation") |
| [giobauermeister/z8lua](https://github.com/giobauermeister/z8lua), the `libs/z8lua` submodule | `tv-patches` | [jtothebell/z8lua](https://github.com/jtothebell/z8lua) `e6928578d46b61fd5ea30cfcf547e855a30a0553` (`z8lua-3ds-switch`, "Add another sandbox fallback to gettable"), the commit upstream FAKE-08 pins |

Each branch has one commit per patch, in order, with the patch's one-line
summary as its subject. A patch that changes both FAKE-08 and z8lua has a
commit with the same subject in each repository. The last commit on the fake-08
branch points `libs/z8lua` (and `.gitmodules`) at the z8lua fork's `tv-patches`
head. Clone with submodules:

```sh
git clone --recursive -b tv-patches https://github.com/giobauermeister/fake-08.git
```

The TV app compiles only `source/` (without `main.cpp` and
`hostCommonFunctions.cpp`), `libs/z8lua` and `libs/lodepng`, and turns on the
fast paths with `-DPICO8_TLINE_FAST=1 -DPICO8_VM_HOT_CALLS=1
-DPICO8_VM_MEMORY_SUPEROPS=1 -DPICO8_VM_FIELD_CACHE=1 -fwrapv`. The desktop,
console and libretro platforms in `platform/` are untouched and were not
rebuilt against the patches.

## The patches

The notes under each patch are the ones written with it, for the port the
patches were first made for. Paths in them such as `main/pico8_emu.cpp`,
`tools/host_tests/`, `upstream/` or `components/fake08` belong to that port,
which is not public.

### 0001 Add fix32 int constructors and casts

*z8lua*

z8lua's fix32 has a constructor and a cast operator for every int*_t,
plus templates covering `long`/`unsigned long` "when it is a distinct
type from the standard int*_t types, e.g. on Windows". On 32-bit
embedded toolchains where int32_t is `long`, the distinct type is `int`
instead, so `int` is the one with neither.

Both halves matter, and they fail differently:

  - constructors: a plain `int` matches nothing exactly and several
    candidates equally badly, so all ~250 places that pass an int to a
    fix32 fail to compile, fix32(0) included. Loud.

  - cast operators: `(int)someFix32` still compiles, by falling back to
    one of the narrower casts, and silently truncates to 8 or 16 bits.
    Lua's own luaL_checkversion_ catches it at startup and aborts every
    cart with "bad conversion number->int; must recompile Lua with
    proper settings" -- but luai_numshl/numshr cast to int too, so
    PICO-8's shift operators would otherwise quietly return nonsense.

The patch adds the mirrored templates for both.

### 0002 Zero CartRom when loading a cart from memory

*fake-08*

FAKE-08's Cart constructor that takes a filename zeroes CartRom before
parsing; the one that takes bytes in memory does not, and loadCart()
copies the whole struct into PicoRam. A cart whose text omits a section
-- no __map__, no __sfx__ -- therefore inherits whatever was in the
heap. This port only ever uses the from-memory constructor, since carts
arrive as bytes copied into memory.

### 0003 Use single-precision math in the audio synthesiser

*fake-08*

Uses single-precision float rather than double arithmetic in the audio
hot path, for CPUs with a single-precision FPU only. Full measurements
are in the patch.

<details><summary>Notes from the original patch</summary>

```text
Use single-precision math in the audio synthesiser.

FAKE-08 accumulates sfx and music playback position in `double`, and
computes each sample's offsets in `double`. The ESP32-P4 is RV32IMAFC:
it has a single-precision FPU and no D extension, so every one of those
operations is emulated in software.

Measured with P8_AUDIO_BENCH=1 on Celeste's title screen, four channels
sounding, ESP32-P4 at 360 MHz:

    double   12.5 ms per frame
    float     7.2 ms per frame     -42%

Precision is not at risk. These values are PICO-8 note offsets bounded to
0..32 and reset per pattern, advancing ~0.0055 per sample; float resolves
about 4e-6 at that magnitude.
```

</details>

### 0004 Discard horizontal spans that start exactly at the right clip edge

*fake-08*

Corrects an exclusive-edge clipping error that painted PICO-8's last
visible column when a cart cleared just outside the screen.

<details><summary>Notes from the original patch</summary>

```text
Discard horizontal spans that start exactly at the right clip edge.

Graphics::_private_h_line rejects a span lying entirely outside the clip
rectangle before drawing. The left test is `x < clip_xb`, but the right
test was `x > clip_xe` -- and clip_xe is *exclusive*, so a span starting
at exactly clip_xe (128, one past the last visible column) was not
rejected. It fell through to

    minx = clamp(min(x1,x2), clip_xb, clip_xe - 1)
    maxx = clamp(max(x1,x2), clip_xb, clip_xe - 1)

which collapsed the whole off-screen span onto column 127 and filled it.

Celeste Classic ends every frame with four rectfills that clear the strip
just outside the screen, so a screenshake cannot smear stale pixels into
view:

    rectfill(-5,-5,-1,133,0)     -- left,   correctly discarded
    rectfill(128,-5,133,133,0)   -- right,  painted column 127 black

So every frame lost its rightmost pixel column. At the 4x scale this port
uses that is a four-pixel black stripe down the right edge of the
picture, and it is why terrain and objects at the right of a room looked
cut off compared with other PICO-8 players.

Verified by dumping the 128x128 screen over serial: column 127 read all
zero before, and reads tile 50's true last column (12,12,12,12,12,12,7,7)
after, with 71 of 128 rows non-black.
```

</details>

### 0005 Fix an out-of-bounds read at the peak of every sin() and cos()

*z8lua*

Clamps the rounded sin/cos lookup index at the table's final entry,
avoiding a one-sample animation glitch and an out-of-bounds read.

<details><summary>Notes from the original patch</summary>

```text
Fix an out-of-bounds read at the peak of every sin() and cos().

sin_helper() indexes a 4096-entry table (valid 0..4095) with

    a   = ((x.bits() & 0x4000 ? ~x : x).bits() & 0x3fff) + 2;
    idx = a >> 2;

The "+ 2" rounds the low bits, and it pushes idx to 4096 -- one past the
end -- for the two angles closest to the quarter turn, which is exactly
where the sine peaks. The read returned whatever followed the table in
memory, so every sin() and cos() produced one wrong sample at each
extreme, twice per cycle.

In Celeste that is the strawberry at 300 m, whose bob is
y = start + sin(off/40)*2.5. Traced frame by frame on hardware:

    before:  45 45 45 45  43  45 45 45 45      50 50 50 50  52  50 ...
    after:   45 45 45 45  45  45 45 45 45      50 50 50 50  50  50 ...

with the sub-pixel value at the peak going from 43.30 to 45.50. It looked
like a stutter because it is a two-pixel jump in the wrong direction for a
single frame, on an animation that otherwise moves one pixel at a time.

Clamping is exact rather than an approximation: near the peak the
reconstruction (idx << 4) + sintable[idx] already evaluates to exactly
65536 (1.0) for several consecutive indices, so 4095 returns precisely
what 4096 should have.

This affects every cart that uses sin() or cos(), not just Celeste.
```

</details>

### 0006 Restrict the cart-sandbox fallback to actual _ENV lookups

*z8lua*

Carries upstream z8lua's corrected sandbox lookup behavior: missing
object fields stay nil, while globals remain available through real
_ENV lookups. This fixes Celeste Classic 2's first level transition.

<details><summary>Notes from the original patch</summary>

```text
Restrict the cart-sandbox fallback to actual _ENV lookups.

The pinned z8lua revision falls back to the cart sandbox whenever OP_GETTABLE
misses any string field on any table. That makes ordinary missing object fields
resolve to globals with the same name. In Celeste Classic 2, a new berry's
missing `self.collected` field therefore resolves to the global `collected`
table and becomes truthy; the next `self.timer += 1` then raises an error
because timer was never initialized.

This carries the two upstream z8lua fixes that first restrict the fallback and
then preserve it for carts that deliberately shadow _ENV:

    de41ca2b4af2770802b5bbd22bae15459c2e942d
    ba1e3d3a879030ccc639effe018da2219a036a52

Verified with the official Celeste Classic 2 cart in FAKE-08's desktop test
runner: the first level transition fails at objects.lua:1071 before this patch
and passes after it. The complete upstream suite also passes (35 tests, 967
assertions).
```

</details>

### 0007 Sample input once per automatic frame so btnp edges reach the cart

*fake-08*

Removes a redundant input sample from FAKE-08's automatic update/draw
loop. On slow draws it could create and then clear a btnp edge before
the cart update observed it, making Scrap Boy's title screen ignore
almost every action-button press. Explicit cart calls to flip() still
refresh input normally.

<details><summary>Notes from the original patch</summary>

```text
Sample input once per automatic cart frame so btnp edges reach the cart.

FAKE-08's generated update/draw loop calls _update_buttons() immediately before
the cart update, then calls flip() after drawing.  flip() calls
_update_buttons() again before yielding.  If a physical press begins during a
slow _draw(), that second sample creates the btnp edge; the next pre-update
sample clears it before the cart can observe it.

Scrap Boy makes the failure nearly deterministic because its title draw takes
about 39 ms on the ESP32-P4.  Hardware tracing showed every USB action button
being translated correctly, but only FAKE-08's post-resume btnp(13) check saw
the edge.  The cart's btnp(4)/btnp(5) calls never did.

The host presents a frame whenever the cart coroutine yields, so the automatic
loop needs a plain yield after _draw(), not flip().  Explicit flip() calls made
by carts retain their input refresh, which is required by blocking animation
and wait loops.

Verified with the exact Scrap Boy cartridge, the complete FAKE-08 desktop
suite (36 tests, 982 assertions), and on hardware.  After the change the trace
shows B/Y reaching btnp(4) and A/X reaching btnp(5), and Scrap Boy advances
through its title and difficulty screens.
```

</details>

### 0008 Skip map cells that cannot intersect the active clip

*fake-08*

Restricts map() to cells that can intersect the active clip instead of
walking a whole off-screen level and asking spr() to reject each tile.

<details><summary>Notes from the original patch</summary>

```text
Skip map cells whose destination sprites cannot intersect the active clip.

FAKE-08's map() walked the complete requested rectangle and relied on spr()
to clip every cell separately.  Carts commonly request a whole level: Scrap
Boy, for example, makes several 128x48-cell map calls while only about 16x16
cells can intersect the PICO-8 viewport.  The off-screen walk still paid for
mget(), optional fget(), and spr() calls thousands of times per frame.

Derive the half-open cell range that intersects the camera-adjusted clip before
walking the map.  Integer floor/ceil helpers preserve the boundary behaviour
for negative map positions.

Verified against every non-audio FAKE-08 desktop test (37 cases and 5,637
assertions) and with a randomized equivalence test covering camera, clip, map
position, dimensions, and layers.  The old and new implementations produced
bit-identical framebuffers in all cases.  An -O3 host profile improved Scrap
Boy by about 17% and Shelled Shinobi by about 13%; ESP32-P4 measurements are
recorded separately.
```

</details>

### 0009 Allow a platform to replace one exact Lua fragment before a cart starts

*fake-08*

Adds a guarded exact-source replacement hook used by opt-in benchmark
builds to select a cart mode or enter a reproducible scene before its
coroutine starts. Normal release builds do not invoke the hook.

<details><summary>Notes from the original patch</summary>

```text
Allow a platform to replace one exact Lua fragment before a cart starts.

Some carts contain their own compatibility or performance modes but hardcode
one of them in the released cartridge.  Ports that cannot modify the source
cart need a narrow way to select such a mode after Cart has decoded compressed
PNG Lua and before vm_run() creates its coroutine.

ReplaceLoadedCartLua() changes exactly one occurrence in the loaded Cart's Lua
source.  It refuses empty, absent, and ambiguous matches, so a cart revision
whose source no longer matches falls back unchanged instead of receiving a
partial or overly broad patch.

The ESP32-P4 host uses this for Shelled Shinobi's author-provided 30 fps path.
The title check and source strings remain platform policy in pico8_emu.cpp;
FAKE-08 only provides the generic pre-launch operation.
```

</details>

### 0010 Track ownership separately for each dependency supplied to Vm

*fake-08*

Tracks ownership per Vm dependency so a host can lend its own PicoRam
arena without Vm deleting it on shutdown.

<details><summary>Notes from the original patch</summary>

```text
Track ownership separately for each dependency supplied to Vm.

The original single cleanup flag becomes true when any missing dependency is
created.  A caller that supplies only PicoRam therefore has its borrowed memory
deleted together with the dependencies that Vm actually owns.  Platforms that
lend a fixed memory arena need mixed ownership to be safe.
```

</details>

### 0011 Compile z8lua's interpreter hot loop normally instead of at O0

*z8lua*

Removes luaV_execute's forced O0 attribute. The build supplies
fixed-point wrap semantics with -fwrapv, preserving boundary-loop
behavior while allowing the interpreter hot loop to build at O3.

<details><summary>Notes from the original patch</summary>

```text
Compile z8lua's interpreter normally instead of forcing its hot loop to O0.

Upstream disabled all optimization on luaV_execute because aggressive C++
optimization can assume signed overflow never occurs and remove OP_FORLOOP's
fixed-point wrap detection.  The ESP-IDF component now compiles z8lua with
-fwrapv, which supplies the arithmetic contract the wrap check requires, so
the whole interpreter no longer needs to run unoptimized.

On ESP32-P4 v1.3 at 360 MHz, an automatically started Scrap Boy stage 1 fell
from about 47.0 ms to 23.4 ms of Lua work per frame and rose from 21 to 42 fps.
The optimized Scrap Boy cart holds 59-60 fps at roughly 11-14.5 ms of Lua work.
A host z8lua regression also verified that one-iteration positive and negative
loops at 0x7fff.ffff and 0x8000.0000 still terminate correctly.
```

</details>

### 0012 Fast-path Lua table reads when the table has no metatable

*z8lua*

Reads plain tables directly in the GETTABLE, GETTABUP and SELF opcodes,
retaining the generic path for metatables, strings and non-tables.

<details><summary>Notes from the original patch</summary>

```text
Fast-path Lua table reads when the table has no metatable.

PICO-8 carts represent objects as plain Lua tables and the cart sandbox itself
is also a plain table.  GETTABLE, GETTABUP, and SELF nevertheless sent every
read through luaV_gettable(), including string indexing and the full __index
tag-method loop.  Read directly with luaH_get() only when the value is a table
whose metatable pointer is null; all strings, non-tables, and tables with
metatables still use the existing generic path.

On the automatically started original Scrap Boy stage 1 this reduced stable
Lua/API windows from roughly 23.3-23.6 ms to 22.1-22.2 ms and raised delivered
performance from 42 to 44 fps.  Host regressions cover present and missing
plain-table keys, functional __index, PICO-8 string indexing, and fixed-point
boundary loops.
```

</details>

### 0013 Accelerate the dominant z8lua hot paths

*z8lua*

Adds exact integral fixed-point division, strictly guarded direct calls
for the hottest numeric PICO-8 APIs, and a mutation-safe GETTABUP hash-
node cache. Together with scoped LTO this brings untouched Scrap Boy
stage 1 to about 17.6-17.8 ms in steady gameplay windows.

<details><summary>Notes from the original patch</summary>

```text
Accelerate the remaining dominant z8lua hot paths without changing cart code.

Integral fixed-point division uses the RV32 hardware divider instead of a
64-bit helper. Strictly guarded calls to pget, rectfill, rnd, sin and cos bypass
Lua's generic C-call frame only for their exact numeric forms. GETTABUP caches
the hash-node index for stable short-string globals while validating the live
table, key and node array on every hit, so writes, deletion and resize remain
immediately visible.

Untouched Scrap Boy stage 1 improved from about 22.1 ms after the O3/table work
to 17.6-17.8 ms in steady windows on the ESP32-P4. The pinned desktop suite
passes 34 cases / 932 assertions, including explicit environment mutation and
resize, division boundaries, and the Celeste Classic 2 transition canary.
```

</details>

### 0014 Expose allocation-free numeric adapters for z8lua's fast calls

*fake-08*

Adds the allocation-free FAKE-08 API adapters called by 0013 after its
function identity, arity, type, result-count and hook guards pass.

<details><summary>Notes from the original patch</summary>

```text
Expose allocation-free numeric adapters used by z8lua's guarded native-call
fast path. The VM checks exact function identity, argument/result counts,
numeric types and disabled debug hooks before calling these; all observable
special cases continue through the normal Lua C API wrappers.

This patch is the FAKE-08 source half of 0013-z8lua-hot-paths.patch and must be
applied with it.
```

</details>

### 0015 Accelerate the horizontal tline form used by Scrap Boy's lights

*z8lua and fake-08*

Adds a guarded direct numeric tline adapter and a pixel-identical row
traversal for horizontal calls using PICO-8's default texture step.
It preserves the generic path for all other tline forms and exposes
per-frame coverage counters to a host profiler.

<details><summary>Notes from the original patch</summary>

```text
Accelerate the exact tline form used by Scrap Boy's radial lights without
changing carts or the output image.

The VM fast-call remains strictly guarded by function identity, numeric
arguments, result count and disabled hooks. Graphics uses the row traversal
only for horizontal lines with PICO-8's default 1/8 texture step; every other
form retains the original general walker. Map wrapping/offsets, camera and
clip state, sprite-zero suppression, palette transparency/remapping and the
hardware colour bitmask are preserved. Counters let the ESP host attribute a
slow frame to tline traffic and verify fast-path coverage.

On ESP32-P4, a controlled 400-call / 43,200-pixel frame fell from 16.3 ms to
7.1 ms with the renderer switch alone. A direct framebuffer comparison corpus
covering reverse lines and all state above produced an identical hash.
```

</details>

### 0016 Format numbers without the C library's soft-float engine

*z8lua*

Formats fix32 as decimal with exact integer arithmetic instead of
sprintf("%1.4f") plus a trailing-zero strip, which costs 12.1 us a
call on a CPU without double-precision floating point. tostr(),
tostring(), print() of a number and every number concatenation go
through it. Verified against the old expression over all 2^32 inputs.

<details><summary>Notes from the original patch</summary>

```text
Format numbers without the C library's soft-float engine.

z8lua's lua_number2str asked printf for "%1.4f" and then stripped trailing
zeros and a trailing point. This target has no double-precision unit, so that
one line reached picolibc's general float-to-decimal path through soft-float
multiply and divide: 12.1 us per conversion, measured on ESP32-P4 at 360 MHz.
Any cart that formats numbers in bulk pays it, and tostr(), tostring(),
print() of a number and every "text"..number concatenation all arrive there.

A 16.16 value is exactly bits/65536, so four decimals need neither float nor
64-bit arithmetic: 10000/65536 == 625/4096, and 65535 * 625 < 2^31. The only
subtlety is the tie, because printf rounds the exact value half to even; the
tie cases are the sixteen fractions 2048*k with k odd, and to_decimal()
reproduces the same rule. The sign is emitted whenever the value is negative
even when every digit rounds away, so the smallest negative value still
prints as "-0", as it did before.

Pico Crate exposed it. That cart rebuilds a room on every death and every room
transition, and its room builder calls tostr() 1,536 times to match tile
characters against class ids. The rebuild ran in one 36.7 ms frame; the change
alone takes it to 19.2 ms, and its residual tostr cost -- the Lua C-call frame
and string interning, with the conversion no longer in it -- is 1.4 ms. In
gameplay the cart's worst frame went from 38.2 ms to 20.3 ms while its steady
state stayed at 2.4 ms of 16.7.

Verified equal to the expression it replaces over the complete input space:
all 2^32 bit patterns against glibc on the host, 0 mismatches, and 1,048,576
values against picolibc on the device itself -- every fraction against twelve
integer parts plus a 2^18 pseudorandom sweep -- also 0 mismatches. The device
check is retained behind PICO8_FIX32_TOSTR_SELFTEST, because the rounding rule
is the C library's and a toolchain change could move it. The pinned desktop
suite passes 34 cases / 926 assertions, and tools/host_tests/p4PortTests.cpp
adds a host regression that recomputes the old expression and compares.
```

</details>

### 0017 Add guarded VM specializations and fast paths for common cart idioms

*z8lua and fake-08*

Records work that went into this tree together with 0011-0015 but was
not exported as a patch at the time: two runtime-specialized GETTABUP
opcodes, memory super-instructions for poke/peek span loops, a
field-read hash-node cache, guarded direct calls for more PICO-8 APIs
with their FAKE-08 adapters, a native all() iterator, cheaper fix32
division and power-of-two modulo/rnd, an srand() state cache, and the
PICO8_VM_PROFILE / PICO8_PERF_LOG counters. Added after the fact as the
exact difference between 0001-0016 and the tree 0018-0020 apply to.

<details><summary>Notes from the original patch</summary>

```text
Add guarded z8lua VM specializations and fast paths for common cart idioms.

This work went into upstream/ with commit 2a05edc ("Optimize PICO-8 runtime"),
alongside 0011-0015, but was not exported as a patch at the time, and 0018-0020
were made on top of it. It is recorded here after the fact: the exact
difference between pinned upstream + 0001-0016 and the tree 0018-0020 apply
to. No separate measurements were kept for it.

Everything that changes interpretation is guarded the same way as 0013: the
function or global must still be the original, debug hooks must be off and
operands must be plain numbers or tables; any other case runs the normal VM.

  - Two new opcodes, OP_GETTABUP_PLAIN and OP_GETTABUP_MEMPOKE: runtime
    specializations of GETTABUP that the VM rewrites in place. They keep its
    A/B/C operands, and the compiler never emits them.
  - PICO8_VM_MEMORY_SUPEROPS (off unless defined to 1): recognizes the
    compiled form of poke(addr, @(mask | @addr)) in a numeric for loop, and a
    52-instruction masked-span closure (classified once per Proto in the new
    p8fastkind field), and runs the whole span in one dispatch.
  - PICO8_VM_FIELD_CACHE (default on): a 512-entry cache of hash-node indices
    for constant-key field reads, rechecking the live node's key on every use.
  - PICO8_VM_HOT_CALLS (default on): direct calls beside OP_CALL for exact
    numeric forms of rnd, srand, sin, cos, sqrt, max, min, mid, flr, ceil,
    abs, sgn, band, poke, cls, circfill, rectfill, spr, sspr, fget, mget, btn
    and btnp, and setmetatable on a new table. The adapters are in
    picoluaapi.cpp; the Lua functions they are compared against lose `static`.
  - A native all() iterator, stepped directly by the VM for plain arrays.
    p8GlobalLuaFunctions.h keeps the Lua all() as the fallback when none is
    registered. split() sizes its result array first, and split"..."[i] reads
    the number without building the table.
  - fix32 division with 32-bit divides only; % and rnd() by a power of two
    with a mask; srand() keeps the mixed RNG state of the last 64 seeds.
  - PICO8_VM_PROFILE (default off): opcode, call and division counters.
    PICO8_PERF_LOG (default off) now gates 0015's tline counters.
  - Whitespace only: the copy dropped the trailing blank line of several files.
```

</details>

### 0018 Walk a container natively for all() and foreach()

*z8lua and fake-08*

Gives all() and foreach() one native mutation-safe walk that reads a
container with no metatable directly, and stops foreach() allocating an
iterator closure per call. The generic accessors, __index, __len and
__eq behaviour are all preserved, including the trailing read that
makes an always-answering __index an infinite container.

<details><summary>Notes from the original patch</summary>

```text
Walk a container natively for all() and foreach().

PICO-8 defines foreach(c,f) as `for v in all(c) do f(v) end`, so both share one
rule: hold an index and the previously yielded value, and advance the index
only when the slot still holds that value. That is what makes removing the
current element inside a loop safe, and it is why neither can be replaced by a
plain numeric for.

all() already had a native iterator here, but its body used the generic Lua C
API: two lua_gettable calls and a luaL_len -- so a luaH_getn, which is a search
rather than a field read -- for every element. foreach() was still interpreted,
allocating an iterator closure per call on top of that. Both now go through one
walk that reads the container raw when it has no metatable, and keeps the
generic accessors otherwise. A container with no metatable has no __index and
no __len, so the raw read is the same observation the interpreted version made;
the check is redone whenever cart code could have attached one in between.

Equality needs the same care. __eq is reached only when both sides are tables,
or both full userdata, that are not already raw-equal and at least one carries
a metatable, so raw equality is the same answer for everything an entity list
holds, and the generic comparison still runs for the cases where it is not.

One behaviour is easy to lose and is the reason the two functions share a step
function rather than each having its own loop: after the skip loop runs the
index past the end, the interpreted iterator still evaluates c[i] and yields
the result. A container whose __index answers every integer key therefore never
finishes. A first version of foreach() stopped at the length instead, and the
host regression below caught it.

Measured on Pico Crate's room rebuild, which calls foreach() 256 times: 19.2 ms
to 18.1 ms, and 3,070 allocator calls to 2,578. The remainder of that rebuild
is dominated by allocation and PSRAM traffic for about 2,600 objects rather
than by interpretation -- roughly 19,000 VM instructions against 2,500 C calls -- so
this is the end of what the list walk can contribute there. Two unrelated
canaries confirm no regression against a control build with these files
reverted: Scrap Boy stage 1 at 11.0-11.3 ms against 11.0-11.2, and the Dank
Tomb walk benchmark at 12.0-15.1 ms against 12.3-15.2.

Verified by tools/host_tests/p4PortTests.cpp, which runs 35 walk scenarios and
8 error scenarios against the interpreted implementations copied verbatim and
requires character-identical traces: holes, repeated values, deletion and
insertion during the walk, nested walks, __index, __len, __eq, string
containers, and the deliberately infinite container above. The pinned desktop
suite passes 34 cases / 926 assertions unchanged.
```

</details>

### 0019 Serialize FAKE-08 audio state shared by the game and I2S worker tasks

*fake-08*

Serializes audio state shared by the game and a host's asynchronous
audio worker, with std::mutex on desktop builds. See the patch.

<details><summary>Notes from the original patch</summary>

```text
Serialize FAKE-08 audio state shared by the game and I2S worker tasks.

The ESP32-P4 port synthesizes audio asynchronously on core 1 so expensive
carts do not lose frame rate while filling the I2S queue. Cart-side sfx() and
music() calls mutate the same channel state that FillAudioBuffer() advances,
which made the new two-task path a data race. Guard those operations with one
mutex. Desktop builds use std::mutex; ESP-IDF uses the lighter FreeRTOS mutex.
```

</details>

### 0020 Fix multi-cart loading, large transfers, circle inversion and recovery

*z8lua and fake-08*

Adds multi-cart bundles and game-relative loading, protected startup
and external-frontend recovery, deferred transitions with upper RAM
retention, large peek/chr transfers, and per-call circle inversion.

<details><summary>Notes from the original patch</summary>

```text
Fix PICO-8 multi-cart loading, large data transfers, circle inversion and recovery.

Support borrowed P8BUNDLE cart views and game-relative SD companions, defer
load() until the caller coroutine yields, preserve upper RAM, and release the
retired Lua sandbox before starting the next cart. Protect source allocation
and breadcrumb setup inside pcall. External frontends exit instead of loading
the BIOS into an exhausted Lua state.

Preserve circle colour control bits in both API paths and require the per-call
inversion bits. Allow Into Ruins' 10019-byte peek/chr transfer without silent
8 KiB truncation or a large C stack buffer; check Lua stack growth failures.

The ESP host keeps automatic GC enabled alongside frame-boundary GC work;
that host-side change and flash/SD packaging changes live outside this patch.
```

</details>

### 0021 Bound pause-menu navigation when trailing entries are hidden

*fake-08*

Cart release clears the final menu entry but keeps its slot table. Pressing
Down on "exit to menu" used to skip that empty entry and index beyond the
menu, causing a Lua error. External frontends then exited the game as part
of error recovery, making Down appear to activate the exit option.

Search the same slots (0 through 8) that the menu draws, skipping missing
tables and empty labels in either direction. If no later visible entry exists,
leave the selection in place. Confirmation callbacks retain their existing
controls. `test/pausemenutests.cpp` exercises the actual BIOS menu functions
with hidden trailing slots, repeated boundary presses, unused/removed custom
entries, breadcrumbs and explicit confirmation.

## Rebasing onto a newer upstream

Rebase z8lua first, since the fake-08 branch points at it. Tag the current
heads so the old series stays reachable:

```sh
# z8lua (a clone of giobauermeister/z8lua, or libs/z8lua)
git remote add upstream https://github.com/jtothebell/z8lua.git   # once
git fetch upstream
git tag tv-patches-e6928578 tv-patches
git rebase --onto upstream/z8lua-3ds-switch e6928578 tv-patches
git push --force-with-lease origin tv-patches

# fake-08
git remote add upstream https://github.com/jtothebell/fake-08.git # once
git fetch upstream
git tag tv-patches-814991a tv-patches
git rebase --onto upstream/master 814991a tv-patches
```

The last fake-08 commit only moves the `libs/z8lua` pointer. If it conflicts,
check out the rebased z8lua `tv-patches` head in `libs/z8lua`, `git add
libs/z8lua` and continue. After `git submodule update`, build the TV app
against the result (`make FAKE08_DIR=<this clone>` there) before pushing.

What to expect against z8lua's `z8lua-3ds-switch` as of July 2026, ten commits
past `e6928578` (checked with a trial rebase):

| Patch | Upstream commit | Result |
|---|---|---|
| 0006 Restrict the cart-sandbox fallback to actual _ENV lookups | `de41ca2b`, `ba1e3d3a` | identical to these two, so git drops it |
| 0017 Add guarded VM specializations and fast paths for common cart idioms | `3f345520` "remove digit separators from fix32.h" | conflict in `fix32.h`: 0017 rewrites the division that used `0x1'0000` |
| 0020 Fix multi-cart loading, large transfers, circle inversion and recovery | `9e6ee3d2` "Update chr to use new peek.poke size limit" | conflict in `lpico8lib.c`: both replace `chr()`'s 8 KiB stack buffer, upstream with `malloc` capped at 32767 bytes, 0020 with a `luaL_Buffer`; keep one |
| all others | | apply unchanged |

FAKE-08's own `master` has no commits past `814991a` yet.
