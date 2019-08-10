//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License

// this is an example how to use bredis in multi-threaded environment, i.e
// the access to sockets and buffers should be routed via asio::io_context::strand
//
// sample output:
//
// connecting to 127.0.0.1:6379
// connected
// pings: 37582, pongs: 37264
// pings: 70115, pongs: 69801
// pings: 109627, pongs: 109627
// pings: 151142, pongs: 151142
// pings: 194718, pongs: 194521
// pings: 233520, pongs: 233519
// pings: 272180, pongs: 272179
// pings: 310104, pongs: 310103
// pings: 346666, pongs: 346666
// pings: 385500, pongs: 385500


#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/tokenizer.hpp>

#include <bredis.hpp>
//#include "../t/SocketWithLogging.hpp"

// alias namespaces
namespace r = bredis;
namespace asio = boost::asio;
namespace po = boost::program_options;
namespace sys = boost::system;

using socket_t = asio::ip::tcp::socket;
using next_layer_t = socket_t;
//using next_layer_t = r::test::SocketWithLogging<asio::ip::tcp::socket>;
using Buffer = boost::asio::streambuf;
using Iterator = typename r::to_iterator<Buffer>::iterator_t;
using Policy = r::parsing_policy::keep_result;
using result_t = r::positive_parse_result_t<Iterator, Policy>;
using Connection = r::Connection<next_layer_t>;

struct redis_accessor_t {
    Buffer tx_buff;
    Buffer rx_buff;
    Connection conn;
    asio::io_context& io;
    asio::io_context::strand strand;
    std::uint64_t ping_count;
    std::uint64_t pong_count;
    redis_accessor_t(socket_t&& s, asio::io_context& context): conn{std::move(s)}, io(context), strand(context), ping_count{0}, pong_count{0}{}
};

struct producer_t {
    redis_accessor_t& redis;
    producer_t(redis_accessor_t& redis_):redis(redis_) {
        produce();
    }
    void produce(){
        r::single_command_t cmd_ping{"PING"};
        redis.conn.async_write(
            redis.tx_buff,
            cmd_ping,
            asio::bind_executor(redis.strand, [this](const sys::error_code &ec, std::size_t bytes_transferred){
                if (!ec){
                    auto self = this;
                    self->redis.ping_count++;
                    self->redis.tx_buff.consume(bytes_transferred);
                    self->produce();
                }
            })
        );
    }
};

struct consumer_t {
    redis_accessor_t& redis;
    consumer_t(redis_accessor_t& redis_):redis(redis_) {
        consume();
    }
    void consume(){
        redis.conn.async_read(
            redis.rx_buff,
            asio::bind_executor(redis.strand, [this](const sys::error_code &ec, result_t &&r){
                if(!ec){
                    auto self = this;
                    self->redis.pong_count++;
                    self->redis.rx_buff.consume(r.consumed);
                    self->consume();
                }
            })
        );
    }
};

struct watcher_t {
    redis_accessor_t& redis;
    asio::steady_timer timer;
    watcher_t(redis_accessor_t& redis_):redis(redis_), timer(redis_.io, asio::chrono::seconds(1)){
        watch();
    }
    void watch() {
        timer.expires_after(asio::chrono::seconds(1));
        timer.async_wait(
            asio::bind_executor(redis.strand, [this](const sys::error_code &ec){
                if (!ec) {
                    auto self = this;
                    std::cout << "pings: " << self->redis.ping_count << ", pongs: " << self->redis.pong_count << "\n";
                    self->watch();
                }
            })
        );
    }
};

int main(int argc, char **argv) {
    po::options_description description("Allowed options");
    description.add_options()("help", "show this help message")
            ("host", po::value<std::string>(), "redis host (default 127.0.0.1)")
            ("port", po::value<std::uint16_t>(), "redis port (default 6379)");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, description), vm);
    po::notify(vm);

    bool show_help = vm.count("help");
    if (show_help) {
        std::cout << description << "\n";
        return 1;
    }

    std::string host = vm.count("host") ? vm["host"].as<std::string>() : "127.0.0.1";
    uint16_t port = vm.count("host") ? vm["port"].as<std::uint16_t>() : 6379;

    asio::io_context io;
    //asio::io_service io_service;
    auto ip_address = asio::ip::address::from_string(host);
    std::cout << "connecting to " << host << ":" << port << "\n";
    asio::ip::tcp::endpoint end_point(ip_address, port);
    socket_t socket(io, end_point.protocol());
    socket.connect(end_point);
    std::cout << "connected\n";

    // wrap it into bredis connection
    redis_accessor_t redis_accessor(std::move(socket), io);
    producer_t producer(redis_accessor);
    consumer_t consumer(redis_accessor);
    watcher_t watcher(redis_accessor);

    // launch!
    auto workers_count = boost::thread::physical_concurrency() * 2;
    boost::thread_group pool;
    for(size_t i = 0; i < workers_count; i++) {
        pool.create_thread([&io] { io.run(); });
    }
    io.run();
    pool.join_all();
}
