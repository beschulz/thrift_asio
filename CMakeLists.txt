cmake_minimum_required(VERSION 2.8.4)
project(thrift_asio)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra")

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

# create one target for each test
file(GLOB_RECURSE tests RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/tests" "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
foreach(test_file ${tests})
    get_filename_component(test_name ${test_file} NAME_WE)

    add_executable(${test_name} "${CMAKE_CURRENT_SOURCE_DIR}/tests/${test_file}")
    target_include_directories(${test_name} PUBLIC "./include")
    target_link_libraries(${test_name} "boost_system" "boost_unit_test_framework")
endforeach(test_file)


set(
    skeleton_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/chat_client_server.skeleton.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/chat_server_server.skeleton.cpp"
)

set(
    thrift_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/chat_client.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/chat_server.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/chat_constants.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/chat_types.cpp"
)

# compile thrift file
add_custom_command(
    OUTPUT ${thrift_sources}
    COMMAND thrift -gen cpp ${CMAKE_CURRENT_SOURCE_DIR}/examples/model/chat.thrift
    COMMAND rm -f ${skeleton_sources}
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/examples/model/chat.thrift"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/examples/model"
    COMMENT "compiling thrift file" VERBATIM
)

file(GLOB examples RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/examples" "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp")

# create one target for each example
foreach(example_file ${examples})
    get_filename_component(example_name ${example_file} NAME_WE)

    add_executable(${example_name} "${CMAKE_CURRENT_SOURCE_DIR}/examples/${example_name}" ${thrift_sources})
    target_include_directories(${example_name} PUBLIC "./include" "./examples/model/gen-cpp")
    target_link_libraries(${example_name} "boost_system" "thrift")
endforeach(example_file)
