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
#include <boost/asio/async_result.hpp>
#include <boost/asio/handler_type.hpp>
#include <boost/utility/string_ref.hpp>

#include "Command.hpp"
#include "Protocol.hpp"
#include "Result.hpp"

namespace bredis {

#define BREDIS_PARSE_RESULT(B)                                                 \
    positive_parse_result_t<typename to_iterator<B>::iterator_t,               \
                            bredis::parsing_policy::keep_result>

template <typename NextLayer> class Connection {

  private:
    NextLayer stream_;

  public:
    template <typename... Args>
    explicit Connection(Args &&... args)
        : stream_(std::forward<Args>(args)...) {}

    inline NextLayer &next_layer() { return stream_; }
    inline const NextLayer &next_layer() const { return stream_; }

    /* asynchronous interface */
    template <typename DynamicBuffer, typename WriteCallback>
    BOOST_ASIO_INITFN_RESULT_TYPE(WriteCallback,
                                  void(boost::system::error_code, std::size_t))
    async_write(DynamicBuffer &tx_buff, const command_wrapper_t &command,
                WriteCallback &&write_callback);

    template <typename DynamicBuffer, typename ReadCallback>
    BOOST_ASIO_INITFN_RESULT_TYPE(ReadCallback,
                                  void(boost::system::error_code,
                                       BREDIS_PARSE_RESULT(DynamicBuffer)))
    async_read(DynamicBuffer &rx_buff, ReadCallback &&read_callback,
               std::size_t replies_count = 1);

    /* synchronous interface */
    void write(const command_wrapper_t &command);
    void write(const command_wrapper_t &command, boost::system::error_code &ec);

    template <typename DynamicBuffer>
    BREDIS_PARSE_RESULT(DynamicBuffer)
    read(DynamicBuffer &rx_buff);

    template <typename DynamicBuffer>
    BREDIS_PARSE_RESULT(DynamicBuffer)
    read(DynamicBuffer &rx_buff, boost::system::error_code &ec);
};

} // namespace bredis

#include "impl/connection.ipp"
