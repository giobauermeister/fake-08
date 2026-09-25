#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// P8BUNDLE v1: magic[8], count:u32le, then count entries of
// nameLength:u16le, dataLength:u32le, name bytes, cart bytes. First cart boots.
// Views borrow the input buffer, which must outlive all cart transitions.
struct CartBundleEntry {
    std::string name;
    const unsigned char* data;
    size_t size;
};
inline std::string cartBundleKey(std::string name) {
    if (!name.empty() && name[0] == '#') name.erase(0, 1);
    if (name.size() >= 7 && name.compare(name.size()-7, 7, ".p8.png") == 0)
        name.resize(name.size()-7);
    else if (name.size() >= 3 && name.compare(name.size()-3, 3, ".p8") == 0)
        name.resize(name.size()-3);
    return name;
}
// BBS load("#name") means the available local revision of that cart.
inline std::string cartBundleBaseKey(std::string name) {
    name = cartBundleKey(name);
    size_t dash = name.find_last_of('-');
    if (dash != std::string::npos && dash + 1 < name.size() &&
        name.find_first_not_of("0123456789", dash + 1) == std::string::npos)
        name.resize(dash);
    return name;
}
inline bool isCartBundle(const unsigned char* data, size_t size) {
    return size >= 8 && memcmp(data, "P8BUNDLE", 8) == 0;
}
inline bool parseCartBundle(const unsigned char* data, size_t size,
                            std::vector<CartBundleEntry>& entries) {
    entries.clear();
    if (!isCartBundle(data, size) || size < 12) return false;
    auto u32 = [](const unsigned char* p) -> uint32_t {
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
               uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    };
    uint32_t count = u32(data + 8);
    if (!count || count > 64) return false;
    size_t at = 12;
    std::vector<CartBundleEntry> parsed;
    for (uint32_t i = 0; i < count; ++i) {
        if (size - at < 6) return false;
        size_t length = data[at] | unsigned(data[at+1]) << 8;
        size_t bytes = u32(data + at + 2);
        at += 6;
        if (!length || length > 96 || length > size-at) return false;
        std::string name(reinterpret_cast<const char*>(data+at), length);
        if (name.find_first_of("/\\") != std::string::npos ||
            name.find('\0') != std::string::npos || name[0] == '#' ||
            cartBundleKey(name).empty()) return false;
        for (const auto& e : parsed)
            if (cartBundleKey(e.name) == cartBundleKey(name)) return false;
        at += length;
        if (!bytes || bytes > size-at) return false;
        parsed.push_back({name, data+at, bytes});
        at += bytes;
    }
    if (at != size) return false;
    entries.swap(parsed);
    return true;
}
