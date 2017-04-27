//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#pragma once

#include <atomic>
#include <initializer_list>
#include <mutex>
#include <queue>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/utility/string_ref.hpp>

#include "Command.hpp"
#include "Error.hpp"
#include "Protocol.hpp"
#include "Result.hpp"

namespace bredis {

template <typename NextLayer> class AsyncConnection {
    using protocol_type_t = typename NextLayer::protocol_type;

  public:
    using string_t = boost::string_ref;
    using args_container_t = std::vector<string_t>;

  private:
    NextLayer socket_;

  public:
    template <typename... Args>
    AsyncConnection(Args &&... args) : socket_(std::forward<Args>(args)...) {}

    inline NextLayer &next_layer() { return socket_; }
    inline NextLayer &&move_layer() { return std::move(socket_); }

    /* asynchronous interface */
    template <typename WriteCallback>
    void async_write(const command_wrapper_t &command,
                     WriteCallback write_callback);

    template <typename ReadCallback, typename Buffer>
    void async_read(Buffer &rx_buff, ReadCallback read_callback,
                    std::size_t replies_count = 1);

    /* synchronous interface */
    void write(const command_wrapper_t &command);

    positive_parse_result_t read(boost::asio::streambuf &rx_buff);
};

} // namespace bredis

#include "impl/async_connection.ipp"
