include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
        URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

add_executable(prserver_test
        ${PROJECT_SOURCE_DIR}/src/server.cpp
        ${PROJECT_SOURCE_DIR}/src/thread_pool.cpp
        ${PROJECT_SOURCE_DIR}/src/http_task.cpp
        ${PROJECT_SOURCE_DIR}/src/router.cpp
        ${PROJECT_SOURCE_DIR}/src/status_handler.cpp
        ${PROJECT_SOURCE_DIR}/src/static_file_handler.cpp
        ${PROJECT_SOURCE_DIR}/src/logger.cpp
        thread_pool_test.cpp
        router_test.cpp
        logger_test.cpp
)

target_include_directories(prserver_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(prserver_test PRIVATE GTest::gtest_main nlohmann_json::nlohmann_json)

if(Boost_VERSION VERSION_LESS 1.89.0)
    target_link_libraries(prserver_test PRIVATE Boost::system Boost::thread)
else()
    target_link_libraries(prserver_test PRIVATE Boost::thread)
endif()

gtest_discover_tests(prserver_test)