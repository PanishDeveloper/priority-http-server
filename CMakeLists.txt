cmake_minimum_required(VERSION 4.00)
project(priority-http-server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED COMPONENTS thread OPTIONAL_COMPONENTS system)

include(FetchContent)
FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
        URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
)
FetchContent_MakeAvailable(json)

add_executable(prserver
        src/main.cpp
        src/server.cpp
        src/thread_pool.cpp
        src/http_task.cpp
        src/router.cpp
        src/status_handler.cpp
)

target_include_directories(prserver PUBLIC include)

target_link_libraries(prserver PRIVATE nlohmann_json::nlohmann_json)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(prserver PRIVATE -Wall -Wextra)
elseif(MSVC)
    target_compile_options(prserver PRIVATE /W4)
endif()

if(Boost_VERSION VERSION_LESS 1.89.0)
    target_link_libraries(prserver PRIVATE Boost::system Boost::thread)
else()
    target_link_libraries(prserver PRIVATE Boost::thread)
endif()