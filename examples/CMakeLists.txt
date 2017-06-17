add_executable(stream-parse stream-parse.cpp)
target_link_libraries(stream-parse ${Boost_LIBRARIES})

add_executable(synch-subscription synch-subscription.cpp)
target_link_libraries(synch-subscription ${Boost_LIBRARIES})
