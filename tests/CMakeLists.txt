add_executable(bang-core-smoke-test hash_smoke_test.cpp)
target_link_libraries(bang-core-smoke-test PRIVATE bang::core)

add_test(NAME bang-core-smoke-test COMMAND bang-core-smoke-test)

add_executable(bang-process-runner-test process_runner_test.cpp)
target_link_libraries(bang-process-runner-test PRIVATE bang::core)
bang_enable_sanitizers(bang-process-runner-test)
add_test(NAME bang-process-runner-test COMMAND bang-process-runner-test)
set_tests_properties(bang-process-runner-test PROPERTIES TIMEOUT 10)

add_executable(bang-download-service-test download_service_test.cpp)
target_link_libraries(bang-download-service-test PRIVATE bang::core)
bang_enable_sanitizers(bang-download-service-test)
add_test(NAME bang-download-service-test COMMAND bang-download-service-test)
set_tests_properties(bang-download-service-test PROPERTIES TIMEOUT 20)
