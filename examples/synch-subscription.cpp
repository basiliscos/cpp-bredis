//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

// This is simple example of subscription to redis channel and endless
// loop of receiving message (while(true){...}) using synchronous interface
// with simplified error handling and result extracture
//
// Sample output:
//
// ./synch-subscription 127.0.0.1:6379 FEED::R2_10 FEED::R2_100
// connecting to 127.0.0.1:6379
// connected
// subscribed
// subscription(s) has been confirmed
// on channel 'FEED::R2_10' received payload ::
// '{"bid":"7765.660","epoch":"1497868248.500","symbol":"R2_10","price":"7765.760","ask":"7765.860"}'
// on channel 'FEED::R2_100' received payload ::
// '{"bid":"7365.905","epoch":"1497868248.500","symbol":"R2_100","price":"7366.905","ask":"7367.905"}'
// ...
//

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <bredis/Connection.hpp>
#include <bredis/Extract.hpp>
#include <bredis/MarkerHelpers.hpp>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// alias namespaces
namespace r = bredis;
namespace asio = boost::asio;

// Auxillary class, returns optional channel/payload string from the extracts
// from redis of published message
//
// We need it as the redis messages come in the format
// [ [string] "message", [string] channel_name, [string] payload]
//
// This class is NOT templated by Iterator as it operates on extracts
// of redis replies, i.e. already decoupled (copies) from buffer.
//
// redis reply without results extraction, what is obviously faster

using string_wrapper_t = std::reference_wrapper<const std::string>;
using optional_string_t =
    boost::optional<std::pair<string_wrapper_t, string_wrapper_t>>;

struct payload_extractor : public boost::static_visitor<optional_string_t> {
    template <typename T> optional_string_t operator()(const T & /* value */) const {
        return optional_string_t{};
    }

    optional_string_t
    operator()(const r::extracts::array_holder_t &value) const {
        // actually we can do more stricter checking, i.e.
        // expect "message" marker ...
        if (value.elements.size() == 3) {
            const auto &channel = value.elements[1];
            const auto *channel_data =
                boost::get<r::extracts::string_t>(&channel);
            const auto &payload = value.elements[2];
            const auto *payload_data =
                boost::get<r::extracts::string_t>(&payload);
            if (channel_data && payload_data) {
                // std::cout << "[debug3]" << payload_ref << "\n";
                string_wrapper_t channel_ref(channel_data->str);
                string_wrapper_t payload_ref(payload_data->str);
                return optional_string_t{
                    std::make_pair(channel_ref, payload_ref)};
            }
        }
        return optional_string_t{};
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

    try {
        // connect to redis
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

        // write subscribe cmd
        r::single_command_t subscribe_cmd{cmd_items.cbegin(), cmd_items.cend()};
        c.write(subscribe_cmd);
        std::cout << "subscribed\n";

        // get the subscription confirmation
        Buffer rx_buff;
        r::marker_helpers::check_subscription<Iterator> check_subscription{
            std::move(subscribe_cmd)};

        for (auto it = cmd_items.cbegin() + 1; it != cmd_items.cend(); it++) {
            auto parse_result = c.read(rx_buff);

            bool subscription_confirmed =
                boost::apply_visitor(check_subscription, parse_result.result);
            if (!subscription_confirmed) {
                std::cout << "subscription for channel " << *it
                          << "was not confirmed\n";
                return 1;
            }
            rx_buff.consume(parse_result.consumed);
        }
        std::cout << "subscription(s) has been confirmed\n";

        // endless receive loop
        while (true) {
            // blocks until new message
            auto parse_result = c.read(rx_buff);
            // extract (aka decouple from buffer / copy) result
            auto extract = boost::apply_visitor(r::extractor<Iterator>(),
                                                parse_result.result);
            // safe to consume buffer now
            rx_buff.consume(parse_result.consumed);

            // select the result in the form of
            // pair<const std::string&, const std::string&>
            auto payload = boost::apply_visitor(payload_extractor(), extract);
            if (payload) {
                // and finally print it
                std::cout << "on channel '" << payload->first.get()
                          << "' received payload :: '" << payload->second.get()
                          << "'\n";
            } else {
                std::cout << "received something else\n";
            }
        };
    } catch (const boost::system::system_error &e) {
        std::cout << "error :: " << e.what() << "\n";
        return 1;
    }

    std::cout << "normal exit\n";
    return 0;
}
