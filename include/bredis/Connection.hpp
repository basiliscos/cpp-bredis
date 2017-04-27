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

template <typename NextLayer> class Connection {

  public:
    using string_t = boost::string_ref;
    using args_container_t = std::vector<string_t>;

  private:
    NextLayer stream_;

  public:
    template <typename... Args>
    Connection(Args &&... args) : stream_(std::forward<Args>(args)...) {}

    inline NextLayer &next_layer() { return stream_; }
    inline const NextLayer &next_layer() const { return stream_; }

    /* asynchronous interface */
    template <typename WriteCallback>
    void async_write(const command_wrapper_t &command,
                     WriteCallback write_callback);

    template <typename ReadCallback, typename DynamicBuffer>
    void async_read(DynamicBuffer &rx_buff, ReadCallback read_callback,
                    std::size_t replies_count = 1);

    /* synchronous interface */
    void write(const command_wrapper_t &command);
    void write(const command_wrapper_t &command, boost::system::error_code &ec);

    template <typename DynamicBuffer>
    positive_parse_result_t read(DynamicBuffer &rx_buff);
    template <typename DynamicBuffer>
    positive_parse_result_t read(DynamicBuffer &rx_buff,
                                 boost::system::error_code &ec);
};

} // namespace bredis

#include "impl/connection.ipp"
