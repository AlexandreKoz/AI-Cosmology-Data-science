add_executable(test_layout unit/test_soa_layout.cpp)
target_link_libraries(test_layout PRIVATE cosmosim_core)
add_test(NAME test_soa_layout COMMAND test_layout)
