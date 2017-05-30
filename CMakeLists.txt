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
        coroutine
        context
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
    set(GCOV_TOOL, "gcov-5")

    add_custom_target("coverage"
        COMMAND "lcov" --directory . --zerocounters
        COMMAND ctest
        #COMMAND "lcov" --directory . --capture --output-file coverage.info --gcov-tool ${GCOV_TOOL}
        COMMAND "lcov" --directory . --capture --output-file coverage.info
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

add_library(catch_lib t/tests-main.cpp)
set(LINK_DEPENDENCIES pthread ${Boost_LIBRARIES} catch_lib)

add_executable(t-05-protocol t/05-protocol.cpp)
target_link_libraries(t-05-protocol ${LINK_DEPENDENCIES})
add_test("t-05-protocol" t-05-protocol)

add_executable(t-06-fragmented-buffer t/06-fragmented-buffer.cpp)
target_link_libraries(t-06-fragmented-buffer ${LINK_DEPENDENCIES})
add_test("t-06-fragmented-buffer" t-06-fragmented-buffer)

add_executable(t-07-extract t/07-extract.cpp)
target_link_libraries(t-07-extract ${LINK_DEPENDENCIES})
add_test("t-07-extract" t-07-extract)

add_executable(t-10-ping t/10-ping.cpp)
target_link_libraries(t-10-ping ${LINK_DEPENDENCIES})
add_test("t-10-ping" t-10-ping)

add_executable(t-11-multi-ping t/11-multi-ping.cpp)
target_link_libraries(t-11-multi-ping ${LINK_DEPENDENCIES})
add_test("t-11-multi-ping" t-11-multi-ping)

add_executable(t-12-basic-types t/12-basic-types.cpp)
target_link_libraries(t-12-basic-types ${LINK_DEPENDENCIES})
add_test("t-12-basic-types" t-12-basic-types)

add_executable(t-13-protol-error t/13-protol-error.cpp)
target_link_libraries(t-13-protol-error ${LINK_DEPENDENCIES})
add_test("t-13-protol-error" t-13-protol-error)

add_executable(t-14-uds t/14-uds.cpp)
target_link_libraries(t-14-uds ${LINK_DEPENDENCIES})
add_test("t-14-uds" t-14-uds)

add_executable(t-15-cancellation t/15-cancellation.cpp)
target_link_libraries(t-15-cancellation ${LINK_DEPENDENCIES})
add_test("t-15-cancellation" t-15-cancellation)

add_executable(t-16-close-connection t/16-close-connection.cpp)
target_link_libraries(t-16-close-connection ${LINK_DEPENDENCIES})
add_test("t-16-close-connection" t-16-close-connection)

add_executable(t-17-sync t/17-sync.cpp)
target_link_libraries(t-17-sync ${LINK_DEPENDENCIES})
add_test("t-17-sync" t-17-sync)

add_executable(t-18-subscription t/18-subscription.cpp)
target_link_libraries(t-18-subscription ${LINK_DEPENDENCIES})
add_test("t-18-subscription" t-18-subscription)

add_executable(t-19-transaction t/19-transaction.cpp)
target_link_libraries(t-19-transaction ${LINK_DEPENDENCIES})
add_test("t-19-transaction" t-19-transaction)

add_executable(t-20-promise t/20-promise.cpp)
target_link_libraries(t-20-promise ${LINK_DEPENDENCIES})
add_test("t-20-promise" t-20-promise)

add_executable(t-21-coroutine t/21-coroutine.cpp)
target_link_libraries(t-21-coroutine ${LINK_DEPENDENCIES})
add_test("t-21-coroutine" t-21-coroutine)

