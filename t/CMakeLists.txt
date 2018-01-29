add_library(catch_lib tests-main.cpp)

if(WIN32)
    set(Boost_USE_MULTITHREADED ON)
    set(LINK_DEPENDENCIES ${Boost_LIBRARIES} catch_lib)
    add_definitions(-DBOOST_ALL_DYN_LINK -DBOOST_THREAD_USE_LIB -D_WIN32_WINNT=0x0501)
else()
    set(LINK_DEPENDENCIES ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES} catch_lib)
endif()


add_executable(t-05-protocol 05-protocol.cpp)
target_link_libraries(t-05-protocol ${LINK_DEPENDENCIES})
add_test("t-05-protocol" t-05-protocol)

add_executable(t-06-fragmented-buffer 06-fragmented-buffer.cpp)
target_link_libraries(t-06-fragmented-buffer ${LINK_DEPENDENCIES})
add_test("t-06-fragmented-buffer" t-06-fragmented-buffer)

add_executable(t-07-extract 07-extract.cpp)
target_link_libraries(t-07-extract ${LINK_DEPENDENCIES})
add_test("t-07-extract" t-07-extract)

add_executable(t-08-markers 08-markers.cpp)
target_link_libraries(t-08-markers ${LINK_DEPENDENCIES})
add_test("t-07-extract" t-08-markers)

add_executable(t-10-ping 10-ping.cpp)
target_link_libraries(t-10-ping ${LINK_DEPENDENCIES})
add_test("t-10-ping" t-10-ping)

add_executable(t-11-multi-ping 11-multi-ping.cpp)
target_link_libraries(t-11-multi-ping ${LINK_DEPENDENCIES})
add_test("t-11-multi-ping" t-11-multi-ping)

add_executable(t-12-basic-types 12-basic-types.cpp)
target_link_libraries(t-12-basic-types ${LINK_DEPENDENCIES})
add_test("t-12-basic-types" t-12-basic-types)

add_executable(t-13-protol-error 13-protol-error.cpp)
target_link_libraries(t-13-protol-error ${LINK_DEPENDENCIES})
add_test("t-13-protol-error" t-13-protol-error)

if(NOT WIN32)
    add_executable(t-14-uds 14-uds.cpp)
    target_link_libraries(t-14-uds ${LINK_DEPENDENCIES})
    add_test("t-14-uds" t-14-uds)
endif()

add_executable(t-15-cancellation 15-cancellation.cpp)
target_link_libraries(t-15-cancellation ${LINK_DEPENDENCIES})
add_test("t-15-cancellation" t-15-cancellation)

add_executable(t-16-close-connection 16-close-connection.cpp)
target_link_libraries(t-16-close-connection ${LINK_DEPENDENCIES})
add_test("t-16-close-connection" t-16-close-connection)

add_executable(t-17-sync 17-sync.cpp)
target_link_libraries(t-17-sync ${LINK_DEPENDENCIES})
add_test("t-17-sync" t-17-sync)

add_executable(t-18-subscription 18-subscription.cpp)
target_link_libraries(t-18-subscription ${LINK_DEPENDENCIES})
add_test("t-18-subscription" t-18-subscription)

add_executable(t-19-transaction 19-transaction.cpp)
target_link_libraries(t-19-transaction ${LINK_DEPENDENCIES})
add_test("t-19-transaction" t-19-transaction)

add_executable(t-20-promise 20-promise.cpp)
target_link_libraries(t-20-promise ${LINK_DEPENDENCIES})
add_test("t-20-promise" t-20-promise)

if(NOT DISABLE_COROUTINES)
    add_executable(t-21-coroutine 21-coroutine.cpp)
    target_link_libraries(t-21-coroutine ${LINK_DEPENDENCIES})
    add_test("t-21-coroutine" t-21-coroutine)
endif()
