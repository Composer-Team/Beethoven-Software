add_executable(alloc_test alloc_test.cc)
target_include_directories(alloc_test PUBLIC ../include/)
target_link_libraries(alloc_test PUBLIC APEX::beethoven)