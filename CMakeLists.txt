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
#    COMPONENTS
#        system
#    REQUIRED
)
include_directories(${bredis_SOURCE_DIR} ${BOOST_INCLUDEDIR})

set(LINK_DEPENDENCIES pthread ${Boost_LIBRARIES})

add_executable(t-05-protocol t/05-protocol.cpp)
target_link_libraries(t-05-protocol ${LINK_DEPENDENCIES})
add_test("t-05-protocol" t-05-protocol)

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
