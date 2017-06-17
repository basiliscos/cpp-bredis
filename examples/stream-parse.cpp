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

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <bredis/Connection.hpp>
#include <bredis/MarkerHelpers.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "picojson.h"

// alias namespaces
namespace r = bredis;
namespace asio = boost::asio;

// Auxillary class, that scans redis parse results for the matching
// of the strings provided in constructor.
//
// We need to check subscription confirmation from redis, as it comes
// from redis in a form
// [[string] "subscribe", [string] channel_name, [int] subscribes_count]
// we check only first two fields (by string equality) and ignore the
// last (as we usually do not care)
//
// This class is templated by Iterator, as we actually just scan
// redis reply without results extraction, what is obviously faster
template <typename Iterator>
struct eq_array : public boost::static_visitor<bool> {
    std::vector<std::string> values_;

    template <typename... Args>
    eq_array(Args &&... args) : values_{std::forward<Args>(args)...} {}

    template <typename T> bool operator()(const T &value) const {
        return false;
    }

    bool operator()(const r::markers::array_holder_t<Iterator> &value) const {
        bool r = values_.size() <= value.elements.size();
        if (r) {
            for (std::size_t i = 0; i < values_.size() && r; i++) {
                const auto &exemplar = values_[i];
                const auto &item = value.elements[i];
                r = r &&
                    boost::apply_visitor(
                        r::marker_helpers::equality<Iterator>(exemplar), item);
            }
        }
        return r;
    }
};

// Auxillary class, extracs picojson::value from a published redis message.
//
// We need it as the redis messages come in the format
// [ [string] "message", [string] channel_name, [string] payload]
//
// This class is templated by Iterator as it operates directy on markers
// in  redis reply, i.e. still bounded to Buffer (type)

template <typename Iterator>
struct json_extractor : public boost::static_visitor<picojson::value> {
    template <typename T> picojson::value operator()(const T &value) const {
        return picojson::value{};
    }

    picojson::value
    operator()(const r::markers::array_holder_t<Iterator> &value) const {
        picojson::value v;
        // "message", channel_name, payload.
        // It is possible to have more stricter checks here, as well as
        // return back boost::optional and channel name
        if (value.elements.size() == 3) {
            // everything except payload is ignored
            const auto &payload = value.elements[2];

            const auto *message =
                boost::get<r::markers::string_t<Iterator>>(&payload);
            if (message) {
                std::string err;
                picojson::parse(v, message->from, message->to, &err);
                if (!err.empty()) {
                    std::cout << "json parse error :: " << err << "\n";
                }
            }
        }
        return v;
    }
};

int main(int argc, char **argv) {
    // common setup
    using socket_t = asio::ip::tcp::socket;
    using next_layer_t = socket_t;
    using Buffer = boost::asio::streambuf;
    using Iterator = typename r::to_iterator<Buffer>::iterator_t;

    // connect to redis
    asio::io_service io_service;
    asio::ip::tcp::endpoint end_point(
        asio::ip::address::from_string("127.0.0.1"), 6379);
    socket_t socket(io_service, end_point.protocol());
    socket.connect(end_point);

    // wrap socket into bredis connection
    r::Connection<next_layer_t> c(std::move(socket));
    std::string channel_name{"json.channel"};

    // let's have it synchronous-like sexy syntax
    boost::asio::spawn(io_service, [&](boost::asio::yield_context
                                           yield) mutable {
        boost::system::error_code error_code;

        // this is rx_and tx_buff simultaneously. Usually,
        // you should NOT do that, i.e. in case of pipe-lining
        Buffer buff;

        // write subscription command
        r::single_command_t cmd{"subscribe", channel_name};
        auto consumed = c.async_write(buff, cmd, yield[error_code]);

        if (error_code) {
            std::cout << "subscription error :: " << error_code.message()
                      << "\n";
            io_service.stop();
        }
        std::cout << "subscribed to " << channel_name << "\n";
        buff.consume(consumed);

        bool subscription_confirmed = false;
        while (true) {
            // read the reply
            auto parse_result = c.async_read(buff, yield[error_code], 1);
            std::cout << "received something...\n";
            if (error_code) {
                std::cout << "reading result error :: " << error_code.message()
                          << "\n";
                break;
            }
            if (!subscription_confirmed) {
                // check that it is subscription confirmation
                eq_array<Iterator> confirm_message{"subscribe", channel_name};
                subscription_confirmed =
                    boost::apply_visitor(confirm_message, parse_result.result);
                if (subscription_confirmed) {
                    std::cout << "subscription to " << channel_name
                              << " confirmed\n";
                } else {
                    std::cout << "cannot get subscription confirmation\n";
                    break;
                }
            } else {
                // extract the JSON from message payload
                json_extractor<Iterator> extract;
                auto json_value =
                    boost::apply_visitor(extract, parse_result.result);
                std::cout << "got json :: " << json_value.serialize() << "\n";
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
