cmake_minimum_required(VERSION 3.10)

project(hserver.c C)

include_directories("include")

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

set(SRC
    "src/main.c"
    "src/server.c"
)

add_executable(hserver ${SRC})
