add_library(Lua STATIC
"lua.cpp"
"lua_class_interface.cpp"
"lua_env.cpp"
"lua_function.cpp"
"lua_state.cpp"
"lua_strategic.cpp"
"lua_table.cpp"
"lua_tactical.cpp"
"lwstring.cpp"
)
target_include_directories(Lua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Pull in the Lua C library built from source in the top-level CMakeLists (its
# include dir supplies lua.h/lauxlib.h/lualib.h/luaconf.h -- the stale 5.1
# copies that used to live here were removed in the Lua upgrade).
target_link_libraries(Lua PUBLIC lua)
