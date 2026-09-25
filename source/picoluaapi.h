#pragma once

#include <string>

//extern "C" {
  #include <lua.h>
  #include <lualib.h>
  #include <lauxlib.h>
//}

#include "graphics.h"
#include "Input.h"
#include "vm.h"
#include "PicoRam.h"

//this can probably go away when I'm loading actual carts and just have to expose api to lua
void initPicoApi(PicoRam* memory, Graphics* graphics, Input* input, Vm* vm, Audio* audio);

//graphics api
int cls(lua_State *L);
int pset(lua_State *L);
int pget(lua_State *L);
int color(lua_State *L);
int line (lua_State *L);
int tline (lua_State *L);
int circ(lua_State *L);
int circfill(lua_State *L);
int oval(lua_State *L);
int ovalfill(lua_State *L);
int rect(lua_State *L);
int rectfill(lua_State *L);

// Internal VM fast-path helpers.  These implement the same numeric cases as
// the Lua wrappers above without constructing a temporary Lua C-call frame.
// lvm.c guards exact function identity, arity, types, result count and hooks;
// every other call continues through the normal Lua API.
lua_Number pico8_fast_pget(lua_Number x, lua_Number y);
void pico8_fast_poke(lua_Number address, lua_Number value);
void pico8_fast_cls();
void pico8_fast_cls(lua_Number color);
void pico8_fast_circfill(lua_Number x, lua_Number y);
void pico8_fast_circfill(lua_Number x, lua_Number y, lua_Number radius);
void pico8_fast_circfill(lua_Number x, lua_Number y, lua_Number radius,
                         lua_Number color);
void pico8_fast_tline(lua_Number x0, lua_Number y0,
                      lua_Number x1, lua_Number y1,
                      lua_Number mx, lua_Number my,
                      lua_Number mdx, lua_Number mdy);
void pico8_fast_rectfill(lua_Number x1, lua_Number y1,
                         lua_Number x2, lua_Number y2, lua_Number color);
void pico8_fast_rectfill(lua_Number x1, lua_Number y1,
                         lua_Number x2, lua_Number y2);
lua_Number pico8_fast_rnd(lua_Number range);
lua_Number pico8_fast_rnd_default();
void pico8_fast_srand(lua_Number seed);
lua_Number pico8_fast_mget(lua_Number x, lua_Number y);
lua_Number pico8_fast_fget(lua_Number sprite);
bool pico8_fast_fget(lua_Number sprite, lua_Number flag);
bool pico8_fast_btn(lua_Number button, lua_Number player);
bool pico8_fast_btnp(lua_Number button, lua_Number player);
lua_Number pico8_fast_btn_default();
lua_Number pico8_fast_btnp_default();
void pico8_fast_spr(lua_Number n, lua_Number x, lua_Number y,
                    lua_Number w, lua_Number h, bool flip_x, bool flip_y);
void pico8_fast_sspr(lua_Number sx, lua_Number sy, lua_Number sw, lua_Number sh,
                     lua_Number dx, lua_Number dy, lua_Number dw, lua_Number dh,
                     bool flip_x, bool flip_y);
int rrect(lua_State *L);
int rrectfill(lua_State *L);
int print(lua_State *L);
int spr(lua_State *L);
int sspr(lua_State *L);
int fget(lua_State *L);
int fset(lua_State *L);
int sget(lua_State *L);
int sset(lua_State *L);
int camera(lua_State *L);
int clip(lua_State *L);
int mget(lua_State *L);
int mset(lua_State *L);
int gfx_map(lua_State *L);
int pal(lua_State *L);
int palt(lua_State *L);
int cursor(lua_State *L);
//todo:
int fillp(lua_State *L); 
int flip(lua_State *L);
//end todo

//tables functions, maybe implemented in lua? 
//list from: https://pico-8.fandom.com/wiki/APIReference
/*
     add(t, v)
    all(t)
    del(t, v)
    foreach(t, f)
    pairs(t) 
*/

//input api
int btn(lua_State *L);
int btnp(lua_State *L);

//audio api
int music(lua_State *L);
int sfx(lua_State *L);

//system
int time(lua_State *L);
int stat(lua_State *L);

//memory api
int cstore(lua_State *L);
int api_memcpy(lua_State *L);
int api_memset(lua_State *L);
int peek(lua_State *L);
int poke(lua_State *L);
int peek2(lua_State *L);
int poke2(lua_State *L);
int peek4(lua_State *L);
int poke4(lua_State *L);
int reload(lua_State *L);

//cart data
int cartdata(lua_State *L);
int dget(lua_State *L);
int dset(lua_State *L);

//
int printh(lua_State *L);

//file system/vm functions
int listcarts(lua_State *L);
int listdirs(lua_State *L);
int cd(lua_State *L);
int pwd(lua_State *L);

int getbioserror(lua_State *L);
int loaddefaultcart(lua_State *L);
int loadsettingscart(lua_State *L);
int togglepausemenu(lua_State *L);
int ispaused(lua_State *L);
int resetcart(lua_State *L);

//settings functions

int getsetting(lua_State *L);
int setsetting(lua_State *L);

int installpackins(lua_State *L);

//label

int loadlabel(lua_State *L);

int getlualine(lua_State *L);

//system functions

int rnd(lua_State *L);
int srand(lua_State *L);
int _update_buttons(lua_State *L);
int run(lua_State *L);
int extcmd(lua_State *L);
int load(lua_State *L);
int reset(lua_State *L);
int setFps(lua_State *L);

//api.tonum(val)

//api.tostr(val, hex)

//api.rnd(x)

//api.srand(seed)

//api.flr=math.floor

//api.ceil=math.ceil

//api.sgn(x)

//api.abs=math.abs

//api.min(a, b)

//api.max(a, b)

//api.mid(x, y, z)

//api.cos(x)

//api.sin(x)

//api.sqrt=math.sqrt

//api.atan2(x, y)

//api.band(x, y)

//api.bor(x, y)

//api.bxor(x, y)

//api.bnot(x)

//api.shl(x, y)

//api.shr(x, y)

//api.lshr(x, y)

//api.rotl(x, y)

//api.rotr(x, y)

//sound
//api.music(n, fade_len, channel_mask)

//api.sfx(n, channel, offset)


//not to do any time soon, but don't crash?
//directory stuff
//api.load(filename)

//api.save()

//api.run()

//api.stop()

//api.reboot()

//api.shutdown()

//api.exit()

//api.info()

//api.export()

//api.import()

//api.help()

//api.folder()

//api.ls()

//api.dir=api.ls

//api.cd()

//api.mkdir()

//api.install_demos()

//api.install_games()

//api.keyconfig()

//api.splore()

//memory
//api.peek(addr)

//api.poke(addr, val)

//api.peek4(addr)

//api.poke4(addr, val)

//api.memcpy(dest_addr, source_addr, len)

//api.memset(dest_addr, val, len)

//api.reload(dest_addr, source_addr, len)

//api.cstore(dest_addr, source_addr, len)

//
