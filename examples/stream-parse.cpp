//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <bredis/Connection.hpp>
#include <bredis/MarkerHelpers.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "picojson.h"

namespace r = bredis;
namespace asio = boost::asio;

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

template <typename Iterator>
struct json_extractor : public boost::static_visitor<picojson::value> {
    template <typename T> picojson::value operator()(const T &value) const {
        return picojson::value{};
    }

    picojson::value
    operator()(const r::markers::array_holder_t<Iterator> &value) const {
        picojson::value v;
        if (value.elements.size() == 3) {
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
    using socket_t = asio::ip::tcp::socket;
    using next_layer_t = socket_t;
    using Buffer = boost::asio::streambuf;
    using Iterator = typename r::to_iterator<Buffer>::iterator_t;

    asio::io_service io_service;
    asio::ip::tcp::endpoint end_point(
        asio::ip::address::from_string("127.0.0.1"), 6379);
    socket_t socket(io_service, end_point.protocol());
    socket.connect(end_point);

    r::Connection<next_layer_t> c(std::move(socket));
    std::string channel_name{"json.channel"};
    boost::asio::spawn(io_service, [&](boost::asio::yield_context
                                           yield) mutable {
        boost::system::error_code error_code;
        Buffer buff;

        auto consumed =
            c.async_write(buff, r::single_command_t{"subscribe", channel_name},
                          yield[error_code]);
        if (error_code) {
            std::cout << "subscription error :: " << error_code.message()
                      << "\n";
            io_service.stop();
        }
        std::cout << "subscribed to " << channel_name << "\n";
        buff.consume(consumed);

        bool subscription_confirmed = false;
        while (true) {
            auto parse_result = c.async_read(buff, yield[error_code], 1);
            std::cout << "received something...\n";
            if (error_code) {
                std::cout << "reading result error :: " << error_code.message()
                          << "\n";
                break;
            }
            if (!subscription_confirmed) {
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
