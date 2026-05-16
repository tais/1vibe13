add_library(Lua
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
# Pull in the Lua 5.1 C library built from source in the top-level
# CMakeLists. Replaces the Windows lua51.lib link.
target_link_libraries(Lua PUBLIC lua51)
