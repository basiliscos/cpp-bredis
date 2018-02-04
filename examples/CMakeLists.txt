if(WIN32)
    set(Boost_USE_MULTITHREADED ON)
    set(LINK_DEPENDENCIES ${Boost_LIBRARIES} catch_lib)
    add_definitions(-DBOOST_ALL_DYN_LINK -DBOOST_THREAD_USE_LIB -D_WIN32_WINNT=0x0501)
endif()

if(NOT DISABLE_COROUTINES)
    add_executable(stream-parse stream-parse.cpp)
    target_link_libraries(stream-parse ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})
endif()

add_executable(synch-subscription synch-subscription.cpp)
target_link_libraries(synch-subscription ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

add_executable(speed_test_async_multi speed_test_async_multi.cpp)
target_link_libraries(speed_test_async_multi ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})
