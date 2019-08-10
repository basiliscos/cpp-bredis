//
//
// Copyright (c) 2017-2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
// mimics performance measurements from
// https://github.com/hmartiro/redox/blob/master/examples/speed_test_async_multi.cpp
//
// Results (1 thread, Intel Core i7-8550U, void-linux, gcc 8.3.0)
//
//  bredis (commands/s) | bredis(*) (commands/s) | redox (commands/s)
// ---------------------+------------------------+---------------------
//       1.80845e+06    |      2.503e+06         |    0.999375+06
//
// Results are not completely fair, because of usage of different semantics in
// APIs; however they are still interesting, as there are used different
// underlying event libraries (Boost::ASIO vs libev) as well redis protocol
// parsing library (written from scratch vs hiredis)
//
// (*) bredis with drop_result policy, i.e. replies from redis server are
// scanned only for formal correctness and never delivered to the caller


#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
#include <string>

#include <bredis/Connection.hpp>
#include <bredis/Extract.hpp>
#include <bredis/Extract.hpp>
#include <bredis/MarkerHelpers.hpp>

#include <boost/algorithm/string.hpp>

double time_s() {
    using namespace std;
    unsigned long ms = chrono::system_clock::now().time_since_epoch() /
                       chrono::microseconds(1);
    return (double)ms / 1e6;
}

// alias namespaces
namespace r = bredis;
namespace asio = boost::asio;
namespace sys = boost::system;
using boost::get;

int main(int argc, char **argv) {
    // common setup
    using socket_t = asio::ip::tcp::socket;
    using next_layer_t = socket_t;
    //using Buffer = boost::asio::streambuf;
    using Buffer = boost::asio::dynamic_string_buffer<
        std::string::value_type,
        std::string::traits_type,
        std::string::allocator_type
    >;
    using Iterator = typename r::to_iterator<Buffer>::iterator_t;
    //using policy_t = r::parsing_policy::drop_result;
    using policy_t = r::parsing_policy::keep_result;
    using result_t = r::positive_parse_result_t<Iterator, policy_t>;

    if (argc < 2) {
        std::cout << "Usage : " << argv[0] << " ip:port \n";
        return 1;
    }

    std::string address(argv[1]);
    std::vector<std::string> dst_parts;
    boost::split(dst_parts, address, boost::is_any_of(":"));
    if (dst_parts.size() != 2) {
        std::cout << "Usage : " << argv[0]
                  << " ip:port channel1 channel2 ...\n";
        return 1;
    }

    std::size_t cmds_count = 500000;
    std::atomic_int count{0};

    // write subscribe cmd
    r::single_command_t cmd_incr{"INCR", "simple_loop:count"};
    r::command_container_t cmd_container;
    for (size_t i = 0; i < cmds_count; ++i) {
        cmd_container.push_back(cmd_incr);
    }

    r::command_wrapper_t cmd_wpapper{std::move(cmd_container)};

    asio::io_service io_service;
    auto ip_address = asio::ip::address::from_string(dst_parts[0]);
    auto port = boost::lexical_cast<std::uint16_t>(dst_parts[1]);
    std::cout << "connecting to " << address << "\n";
    asio::ip::tcp::endpoint end_point(ip_address, port);
    socket_t socket(io_service, end_point.protocol());
    socket.connect(end_point);
    std::cout << "connected\n";

    // wrap it into bredis connection
    r::Connection<next_layer_t> c(std::move(socket));

    std::string tx_backend, rx_backend;
    //tx_backend.reserve(cmds_count * 4);
    //rx_backend.reserve(cmds_count * 4);
    Buffer tx_buff(tx_backend), rx_buff(rx_backend);
    //Buffer tx_buff, rx_buff;
    std::promise<void> completion_promise;
    auto completion_future = completion_promise.get_future();

    c.async_read(
        rx_buff,
        [&](const sys::error_code &ec, result_t &&r) {
            assert(!ec);
            (void)ec;
            rx_buff.consume(r.consumed);
            // cannot be done with drop_result
            //auto &replies = get<r::markers::array_holder_t<Iterator>>(r.result);
            //count += replies.elements.size() - 1;
            count = static_cast<int>(cmds_count);
            completion_promise.set_value();
            std::cout << "done reading...\n";
        },
    cmds_count, policy_t{});

    c.async_write(tx_buff, cmd_wpapper, [&](const sys::error_code &ec,
                                            std::size_t bytes_transferred) {
        (void)ec;
        assert(!ec);
        tx_buff.consume(bytes_transferred);
        std::cout << "done writing...\n";
    });

    std::chrono::nanoseconds sleep_delay(1);
    double t0 = time_s();
    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }
    double t_elapsed = time_s() - t0;

    io_service.run();
    std::cout << "done...\n";
    completion_future.get();

    c.write(r::single_command_t{"GET", "simple_loop:count"});
    auto r = c.read(rx_buff);
    auto &str_reply = get<r::markers::string_t<Iterator>>(r.result);

    std::string counter_value {str_reply.from, str_reply.to};

    double actual_freq = (double)count / t_elapsed;
    std::cout << "Sent " << cmds_count << " commands in " << t_elapsed << "s, "
              << "that's " << actual_freq << " commands/s."
              << "\n";

    std::cout << "Final value of counter: " << counter_value << "\n";

    std::cout << "exiting...\n";
    return 0;
}
