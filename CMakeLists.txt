cmake_minimum_required(VERSION 3.16)

project(ACaOS-lab-2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)

find_package(Threads REQUIRED)

add_library(socket STATIC
    socket/socket.cpp
)

target_include_directories(socket PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(
    server
    server/main.cpp
    server/server.cpp
    )

target_link_libraries(server PRIVATE socket)

add_executable(
    client
    client/main.cpp
    client/client.cpp
)

target_link_libraries(client PRIVATE socket)

add_library(history STATIC history/history.cpp)

target_include_directories(history PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(server PRIVATE history)

target_link_libraries(server PRIVATE Threads::Threads)

target_link_libraries(client PRIVATE Threads::Threads)