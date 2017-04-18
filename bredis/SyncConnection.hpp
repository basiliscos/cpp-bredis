//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <initializer_list>
#include <boost/asio.hpp>
#include <boost/utility/string_ref.hpp>

#include "Result.hpp"
#include "Protocol.hpp"
#include "Error.hpp"

namespace bredis {

// Consider renaming `S` to `SyncStream` or `NextLayer`
//
template <typename S> class SyncConnection {
    using protocol_type_t = typename S::protocol_type;
    using string_t = boost::string_ref;

    // I'm not sure why you want to constrain the type of `S`, this
    // looks like an unnecessary restriction. Relaxing this constraint
    // could let this wrapper class work with a greater variety of type
    // choices for `S`.
    // 
    static_assert(std::is_same<protocol_type_t, boost::asio::ip::tcp>::value ||
                      std::is_same<protocol_type_t,
                                   boost::asio::local::stream_protocol>::value,
                  "SyncConnection can be specialized either for ip::tcp or "
                  "local::stream_protocol");

  private:
    S socket_;

  public:
    // Consider this isgnature instead>:
    //
    // template<class... Args>
    // SyncConnection(Args&&... args)
    // and use perfect forwarding to initialize `socket_`.
    // 
    SyncConnection(S &&socket) : socket_(std::move(socket)) {}

    // Could benefit from `next_layer` and `lowest_layer`
    // member functions, what if the caller wants to change
    // socket options or query the socket?
    //
    template <typename C = std::initializer_list<string_t>>
    redis_result_t command(const std::string &cmd, C &&container,
                           boost::asio::streambuf &rx_buff);

    redis_result_t inline command(const std::string &cmd,
                                  boost::asio::streambuf &rx_buff) {
        return command(cmd, std::initializer_list<string_t>{}, rx_buff);
    }
};

// With some refactoring you could combine SyncConnection and
// AsyncConnection into a single class, this would make the interface
// cleaner and eliminate redundant code. It would work the same way
// as Boost.Asio sockets, which support both synchronous and
// asynchronous interfaces.

} // namespace bredis

#include "impl/sync_connection.hpp"
