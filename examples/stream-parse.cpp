//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

// This is example how to use bredis for
// 1. perform asynchronous subscription to channel
// 2. extract the JSON payload from directly from redis reply
//    buffers without relocations.
// 3. with help of coroutine-sugar

// For JSON extraction picojson library (https://github.com/kazuho/picojson)
// is used as it provides interface to parse the results using iterators.
//
// Why this matters? If you expect that messages on redis channels (or
// any other data in redis) will be small, this should not matter a lot,
// but as the if data size becomes significant (upto 512 MB by redis
// protocol specification) moving data buffers around just to make then
// flat and convenient parse becomes too expensive luxury.
//
// If you don't need that, just apply shipped bredis::extractor<Iterator>()
// to get the familiar std::string, int64_t etc. to work with.

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/optional.hpp>
#include <bredis/Connection.hpp>
#include <bredis/MarkerHelpers.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "picojson.h"

// alias namespaces
namespace r = bredis;
namespace asio = boost::asio;

struct json_payload {
    std::string channel;
    picojson::value json;
};

// Auxillary class, extracs option<json_payload> from a published redis message.
//
// We need it as the redis messages come in the format
// [ [string] "message", [string] channel_name, [string] payload]
//
// This class is templated by Iterator as it operates directy on markers
// in  redis reply, i.e. still bounded to Buffer (type)

using option_t = boost::optional<json_payload>;

template <typename Iterator>
struct json_extractor : public boost::static_visitor<option_t> {
    template <typename T> option_t operator()(const T & /*value*/) const {
        return option_t{};
    }

    option_t
    operator()(const r::markers::array_holder_t<Iterator> &value) const {
        // "message", channel_name, payload.
        // It is possible to have more stricter checks here, as well as
        // return back boost::optional and channel name
        if (value.elements.size() == 3) {
            const auto *channel =
                boost::get<r::markers::string_t<Iterator>>(&value.elements[1]);
            const auto *payload =
                boost::get<r::markers::string_t<Iterator>>(&value.elements[2]);
            if (channel && payload) {
                std::string err;
                picojson::value v;
                picojson::parse(v, payload->from, payload->to, &err);
                if (!err.empty()) {
                    std::cout << "json parse error :: " << err << "\n";
                    return option_t{};
                }
                // all OK
                return option_t{
                    json_payload{std::string{channel->from, channel->to}, v}};
            }
        }
        return option_t{};
    }
};

int main(int argc, char **argv) {
    // common setup
    using socket_t = asio::ip::tcp::socket;
    using next_layer_t = socket_t;
    using Buffer = boost::asio::streambuf;
    using Iterator = typename r::to_iterator<Buffer>::iterator_t;

    if (argc < 2) {
        std::cout << "Usage : " << argv[0]
                  << " ip:port channel1 channel2 ...\n";
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

    std::vector<std::string> cmd_items{"subscribe"};
    std::copy(&argv[2], &argv[argc], std::back_inserter(cmd_items));

    // connect to redis
    asio::io_service io_service;
    auto ip_address = asio::ip::address::from_string(dst_parts[0]);
    auto port = boost::lexical_cast<std::uint16_t>(dst_parts[1]);
    std::cout << "connecting to " << address << "\n";
    asio::ip::tcp::endpoint end_point(ip_address, port);
    socket_t socket(io_service, end_point.protocol());
    socket.connect(end_point);
    std::cout << "connected\n";

    // wrap socket into bredis connection
    r::Connection<next_layer_t> c(std::move(socket));

    // let's have it synchronous-like sexy syntax
    boost::asio::spawn(io_service, [&](boost::asio::yield_context
                                           yield) mutable {
        boost::system::error_code error_code;

        // this is rx_and tx_buff simultaneously. Usually,
        // you should NOT do that, i.e. in case of pipe-lining
        Buffer buff;

        // write subscription command
        r::single_command_t subscribe_cmd{cmd_items.cbegin(), cmd_items.cend()};
        auto consumed = c.async_write(buff, subscribe_cmd, yield[error_code]);

        if (error_code) {
            std::cout << "subscription error :: " << error_code.message()
                      << "\n";
            io_service.stop();
        }
        std::cout << "send subscription\n";
        buff.consume(consumed);

        // check subscription
        {
            r::marker_helpers::check_subscription<Iterator> check_subscription{
                std::move(subscribe_cmd)};

            for (auto it = cmd_items.cbegin() + 1; it != cmd_items.cend();
                 it++) {
                auto parse_result = c.async_read(buff, yield[error_code], 1);
                std::cout << "received something...\n";
                if (error_code) {
                    std::cout
                        << "reading result error :: " << error_code.message()
                        << "\n";
                    io_service.stop();
                    return;
                }

                bool confimed = boost::apply_visitor(check_subscription,
                                                     parse_result.result);
                if (!confimed) {
                    std::cout << "subscription was not confirmed\n";
                    io_service.stop();
                    return;
                }
                buff.consume(parse_result.consumed);
            }
            std::cout << "subscription(s) has been confirmed\n";
        }

        while (true) {
            // read the reply
            auto parse_result = c.async_read(buff, yield[error_code], 1);
            std::cout << "received something...\n";
            if (error_code) {
                std::cout << "reading result error :: " << error_code.message()
                          << "\n";
                break;
            }
            // extract the JSON from message payload

            json_extractor<Iterator> extract;
            auto payload = boost::apply_visitor(extract, parse_result.result);

            if (payload) {
                std::cout << "on channel '" << payload.get().channel
                          << "' got json :: " << payload.get().json.serialize()
                          << "\n";
            } else {
                std::cout << "wasn't able to parse payload as json\n";
            }

            buff.consume(parse_result.consumed);
        }
        io_service.stop();

    });

    io_service.run();
    std::cout << "exiting..."
              << "\n";
    return 0;
}
