#include <memory>

#include "doctest.h"
#include "../source/vm.h"

// Load the same BIOS globals as Vm, without starting a cart coroutine.
extern bool _initializeLuaState(lua_State* luaState);

static void runMenuLua(lua_State* L, const char* code) {
    int status = luaL_dostring(L, code);
    INFO("Lua error: " << (status == LUA_OK ? "" : lua_tostring(L, -1)));
    REQUIRE(status == LUA_OK);
}

TEST_CASE("Pause menu navigation stays within visible items") {
    std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(), lua_close);
    REQUIRE(state != nullptr);
    REQUIRE(_initializeLuaState(state.get()));
    lua_State* L = state.get();
    lua_settop(L, 0);

    runMenuLua(L, R"LUA(
        pressed = -1
        function btnp(b) return b == pressed end
        function navigate(b) pressed = b __f08_menu_update() end
    )LUA");

    SUBCASE("Down stops at exit after cart release hides the trailing slot") {
        runMenuLua(L, R"LUA(
            __z8_release_cart()
            exits = 0
            __f08_menu_items[7][2] = function() exits = exits + 1 end
            navigate(3)
            assert(__f08_menu_selected == 6)
            navigate(3)
            assert(__f08_menu_selected == 7)
            for i = 1, 20 do
                navigate(3)
                assert(__f08_menu_selected == 7)
            end
            assert(exits == 0)
            navigate(5)
            assert(exits == 1)
            navigate(4)
            assert(exits == 2)
        )LUA");
    }

    SUBCASE("Up stops at continue and skips unused custom slots") {
        runMenuLua(L, R"LUA(
            __z8_release_cart()
            __f08_menu_selected = 7
            navigate(2)
            assert(__f08_menu_selected == 6)
            navigate(2)
            assert(__f08_menu_selected == 0)
            for i = 1, 20 do
                navigate(2)
                assert(__f08_menu_selected == 0)
            end
        )LUA");
    }

    SUBCASE("Visible custom items and breadcrumbs remain reachable") {
        runMenuLua(L, R"LUA(
            __z8_release_cart()
            menuitem(2, "custom", function() return true end)
            __addbreadcrumb("previous cart", "previous.p8")
            for _, expected in ipairs({2, 6, 7, 8, 8}) do
                navigate(3)
                assert(__f08_menu_selected == expected)
            end
            for _, expected in ipairs({7, 6, 2, 0, 0}) do
                navigate(2)
                assert(__f08_menu_selected == expected)
            end
        )LUA");
    }

    SUBCASE("Removed slot tables and labels are skipped in both directions") {
        runMenuLua(L, R"LUA(
            __z8_release_cart()
            menuitem(1, "first", function() return true end)
            menuitem(4, "fourth", function() return true end)
            __f08_menu_items[2] = nil
            __f08_menu_items[3] = nil
            __f08_menu_items[6] = nil
            for _, expected in ipairs({1, 4, 7, 7}) do
                navigate(3)
                assert(__f08_menu_selected == expected)
            end
            for _, expected in ipairs({4, 1, 0, 0}) do
                navigate(2)
                assert(__f08_menu_selected == expected)
            end
        )LUA");
    }

    SUBCASE("Removing all later labels leaves the selection in place") {
        runMenuLua(L, R"LUA(
            __z8_release_cart()
            for i = 1, 8 do __f08_menu_items[i] = {nil, nil} end
            navigate(3)
            assert(__f08_menu_selected == 0)
            navigate(2)
            assert(__f08_menu_selected == 0)
        )LUA");
    }

    SUBCASE("Removing all earlier labels leaves the selection in place") {
        runMenuLua(L, R"LUA(
            __z8_release_cart()
            __f08_menu_items[0] = {nil, nil}
            __f08_menu_selected = 6
            navigate(2)
            assert(__f08_menu_selected == 6)
        )LUA");
    }
}
