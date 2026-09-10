add_executable(bang-core-smoke-test hash_smoke_test.cpp)
target_link_libraries(bang-core-smoke-test PRIVATE bang::core)

add_test(NAME bang-core-smoke-test COMMAND bang-core-smoke-test)
