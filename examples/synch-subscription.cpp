//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

// This is simple example of subscription to redis channel and endless
// loop of receiving message (while(true){...}) using synchronous interface
// with simplified error handling and result extracture

#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <bredis/Connection.hpp>
#include <bredis/Extract.hpp>
#include <bredis/MarkerHelpers.hpp>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

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

using optional_string_t =
    boost::optional<std::pair<const std::string &, const std::string &>>;

struct payload_extractor : public boost::static_visitor<optional_string_t> {
    template <typename T> optional_string_t operator()(const T &value) const {
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
                const std::string &channel_ref = channel_data->str;
                const std::string &payload_ref = payload_data->str;
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

    // connect to redis
    asio::io_service io_service;
    asio::ip::tcp::endpoint end_point(
        asio::ip::address::from_string("127.0.0.1"), 6379);
    socket_t socket(io_service, end_point.protocol());
    socket.connect(end_point);

    // wrap it into bredis connection
    r::Connection<next_layer_t> c(std::move(socket));

    try {
        // subscribe
        r::single_command_t subscribe_cmd{"SUBSCRIBE", "channel-one"};
        c.write(subscribe_cmd);
        std::cout << "subscribed\n";

        // get the subscription confirmation
        Buffer rx_buff;
        auto parse_result = c.read(rx_buff);
        eq_array<Iterator> confirm_message{
            "subscribe", "channel-one",
        };
        // .. and check it
        bool subscription_confirmed =
            boost::apply_visitor(confirm_message, parse_result.result);
        if (!subscription_confirmed) {
            std::cout << "subscription was not confirmed\n";
            return 1;
        }
        std::cout << "subscription confirmed\n";
        rx_buff.consume(parse_result.consumed);

        // endless receive loop
        while (true) {
            // blocks until new message
            parse_result = c.read(rx_buff);
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
                std::cout << "on channel '" << payload->first
                          << "' received payload :: '" << payload->second
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
