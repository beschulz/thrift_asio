cmake_minimum_required(VERSION 2.8.4)
project(thrift_asio)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wconversion")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdocumentation")
endif()

# this is here, so that users can use a custom boost installation
# used also by travis CI
if(EXISTS "$ENV{BOOST_ROOT}")
    link_directories("$ENV{BOOST_ROOT}/lib")
    include_directories("$ENV{BOOST_ROOT}/include")
endif(EXISTS "$ENV{BOOST_ROOT}")

# create documentation
# add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
add_custom_target(
    doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs
    COMMENT "Generating API documentation with Doxygen" VERBATIM
)
endif(DOXYGEN_FOUND)



set(
    test_thrift_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/model/gen-cpp/synchronous_service.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/model/gen-cpp/asynchronous_server.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/model/gen-cpp/asynchronous_client.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/model/gen-cpp/test_constants.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/model/gen-cpp/test_types.cpp"
)

set(
    test_skeleton_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/model/gen-cpp/synchronous_service_server.skeleton.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/model/gen-cpp/asynchronous_server_server.skeleton.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/model/gen-cpp/asynchronous_client_server.skeleton.cpp"
)

# compile thrift file
add_custom_command(
    OUTPUT ${test_thrift_sources}
    COMMAND thrift -gen cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/model/test.thrift
    COMMAND rm -f ${test_skeleton_sources}
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/model/test.thrift"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests/model"
    COMMENT "compiling test.thrift" VERBATIM
)

# create one target for each test
file(GLOB tests RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/tests" "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
foreach(test_file ${tests})
    get_filename_component(test_name ${test_file} NAME_WE)

    add_executable(${test_name} "${CMAKE_CURRENT_SOURCE_DIR}/tests/${test_file}" ${test_thrift_sources})
    target_include_directories(${test_name} PUBLIC "./include" "./tests/model/gen-cpp")
    target_link_libraries(${test_name} "boost_system" "boost_unit_test_framework" "thrift")
endforeach(test_file)





set(
    example_thrift_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/chat_client.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/chat_server.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/chat_constants.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/chat_types.cpp"
)

set(
    example_skeleton_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/chat_client_server.skeleton.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/chat_server_server.skeleton.cpp"
)

# compile thrift file
add_custom_command(
    OUTPUT ${example_thrift_sources}
    COMMAND thrift -gen cpp ${CMAKE_CURRENT_SOURCE_DIR}/examples/model/chat.thrift
    COMMAND rm -f ${example_skeleton_sources}
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/examples/model/chat.thrift"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/examples/model"
    COMMENT "compiling thrift file" VERBATIM
)

file(GLOB examples RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/examples" "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp")

# create one target for each example
foreach(example_file ${examples})
    get_filename_component(example_name ${example_file} NAME_WE)

    add_executable(${example_name} "${CMAKE_CURRENT_SOURCE_DIR}/examples/${example_name}" ${example_thrift_sources})
    target_include_directories(${example_name} PUBLIC "./include" "./examples/model/gen-cpp")
    target_link_libraries(${example_name} "boost_system" "thrift")
endforeach(example_file)
