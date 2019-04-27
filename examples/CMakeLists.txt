find_package(Threads REQUIRED)

set(LINK_DEPENDENCIES ${Boost_LIBRARIES} Threads::Threads)

set(BOOST_VERSION_OK, (Boost_MAJOR_VERSION GREATER_EQUAL 1) AND (Boost_MINOR_VERSION GREATER_EQUAL 70))

add_executable(stream-parse stream-parse.cpp)
target_link_libraries(stream-parse ${LINK_DEPENDENCIES})

add_executable(synch-subscription synch-subscription.cpp)
target_link_libraries(synch-subscription ${LINK_DEPENDENCIES})

if(BOOST_VERSION_OK)
    add_executable(speed_test_async_multi speed_test_async_multi.cpp)
    target_link_libraries(speed_test_async_multi ${LINK_DEPENDENCIES})
endif()

add_executable(multi-threads-1 multi-threads-1.cpp)
target_link_libraries(multi-threads-1 ${LINK_DEPENDENCIES})
