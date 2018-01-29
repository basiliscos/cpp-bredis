cmake_minimum_required(VERSION 3.0)
project (bredis)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include (CTest)
enable_testing()

add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY)
#add_definitions(-DBREDIS_DEBUG)

find_package(
    Boost
    COMPONENTS
        chrono
        coroutine
        context
        filesystem
        regex
        system
        thread
    REQUIRED
)

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "-g -O0 -fprofile-arcs -ftest-coverage")
    set(CMAKE_C_FLAGS "-g -O0 -fprofile-arcs -ftest-coverage")

    execute_process(COMMAND
        ${CMAKE_CXX_COMPILER} -dumpversion
        OUTPUT_VARIABLE GCC_VERSION)
    string(STRIP ${GCC_VERSION} GCC_VERSION)
    MESSAGE(STATUS "gcc version: [" ${GCC_VERSION} "]")

    # this works on gentoo, but not on travis/ubuntu, let's hard-code the value
    #execute_process(COMMAND
    #    "which" "gcov-${GCC_VERSION}"
    #    OUTPUT_VARIABLE GCOV_TOOL)
    #MESSAGE(STATUS "gcov: [" ${GCOV_TOOL} "]")
    #string(STRIP ${GCOV_TOOL} GCOV_TOOL)
    #MESSAGE(STATUS "gcov(fixed): [" ${GCOV_TOOL} "]")
    set(GCOV_TOOL "gcov-5")

    add_custom_target("coverage"
        COMMAND "lcov" --directory . --zerocounters
        COMMAND ctest
        COMMAND "lcov" --directory . --capture --output-file coverage.info --gcov-tool ${GCOV_TOOL}
        COMMAND "lcov" --remove coverage.info "'/t/*'" "'/boost_1_63_0/*'" "'/usr/*'" --output-file coverage.info.cleaned
        COMMAND "rm" coverage.info

        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    add_custom_target("coverage-report"
        COMMAND "genhtml" -o coverage coverage.info.cleaned
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS "coverage"
    )
endif()

include_directories(${bredis_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})
MESSAGE(STATUS "boost inc: " ${Boost_INCLUDE_DIRS})
MESSAGE(STATUS "boost libs: " ${Boost_LIBRARIES})

# ASIO maintenance is lagging behind the rest of boost, see
# https://github.com/chriskohlhoff/asio/pull/200 for one of the proposed patches.
set(DISABLE_COROUTINES true)
add_subdirectory(t)
add_subdirectory(examples)


