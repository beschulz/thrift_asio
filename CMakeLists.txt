cmake_minimum_required(VERSION 2.8.4)
project(asio_service_discovery)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra")

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

# create one target for each example
add_custom_target(
    thrift-compile
    COMMAND thrift -gen cpp ${CMAKE_CURRENT_SOURCE_DIR}/examples/model/chat.thrift
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/examples/model/chat.thrift
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/examples/model
    COMMENT "Compiling thrift model" VERBATIM
)

set(
    skeleton_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/chat_client_server.skeleton.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/chat_server_server.skeleton.cpp
)

add_custom_target(
    thrift-compile-and-cleanup
    COMMAND rm ${skeleton_sources}
    DEPENDS thrift-compile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/
    COMMENT "removing unneeded thrift output files" VERBATIM
)

file(GLOB examples RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/examples" "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp")
file(GLOB thrift_sources RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/examples/model/gen-cpp/*.cpp")
list(REMOVE_ITEM thrift_sources ${skeleton_sources})

foreach(example_file ${examples})
    get_filename_component(example_name ${example_file} NAME_WE)

    add_executable(${example_name} "${CMAKE_CURRENT_SOURCE_DIR}/examples/${example_name}" ${thrift_sources})
    target_include_directories(${test_name} PUBLIC "./include" "./examples/model/gen-cpp")
    target_link_libraries(${example_name} "boost_system" "thrift")
    add_dependencies(${example_name} thrift-compile-and-cleanup)
endforeach(example_file)
