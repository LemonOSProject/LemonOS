cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)

add_executable(formatter_test tests/formatter.cpp)
add_executable(list_test tests/list.cpp)
add_executable(tuple_test tests/tuple.cpp)
add_executable(pq_test tests/pq.cpp)
add_executable(cb_test tests/callback.cpp)

enable_testing()

add_test(NAME formatter_test COMMAND $<TARGET_FILE:formatter_test>)
add_test(NAME list_test COMMAND $<TARGET_FILE:list_test>)
add_test(NAME tuple_test COMMAND $<TARGET_FILE:tuple_test>)
add_test(NAME pq_test COMMAND $<TARGET_FILE:pq_test>)
add_test(NAME cb_test COMMAND $<TARGET_FILE:cb_test>)
