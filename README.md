# bredis
Boost::ASIO low-level redis client (connector)

[![Travis](https://img.shields.io/travis/basiliscos/cpp-bredis.svg)](https://travis-ci.org/basiliscos/cpp-bredis)
[![Build status](https://ci.appveyor.com/api/projects/status/a302juc7hrcdhhoc?svg=true)](https://ci.appveyor.com/project/basiliscos/cpp-bredis)
[![license](https://img.shields.io/github/license/basiliscos/cpp-bredis.svg)](https://github.com/basiliscos/cpp-bredis/blob/master/LICENSE)
[![codecov](https://codecov.io/gh/basiliscos/cpp-bredis/badge.svg)](https://codecov.io/gh/basiliscos/cpp-bredis)

## Features

- header only
- zero-copy (currently only for received replies from Redis)
- low-level controls (i.e. you can cancel, or do manual DNS-resolving before a connection)
- unix domain sockets support
- works on linux (clang, gcc) and windows (msvc)
- synchronous & asynchronous interface
- inspired by [beast](https://github.com/vinniefalco/Beast)

## Changelog

### 0.08
- relaxed c++ compiler requirements: c++11 can be used instead of c++14

### 0.07
- minor parsing speed improvements (upto 10% in synthetic tests)
- fix compilation issues on boost::asio 1.70
- make it possible to use `DynamicBuffer_v2` (dynamic_string_buffer, dynamic_vector_buffer)
    from boost::asio 1.70 in addition to `streambuf`. `DynamicBuffer_v1` was actually never
    supported by `bredis`
- [API breakage] `boos::asio::use_future` cannot be used with `bredis` and `boost::asio`
    prior `v1.70` (see [issue](https://github.com/boostorg/asio/issues/226)). If you need
    `use_future` then either upgrade boost::asio or use previous `bredis` version.

### 0.06
- the `parsing_policy::drop_result` was documented and made applicable in client code
- updated preformance results
- fixed compliation warnings (`-Wall -Wextra -pedantic -Werror`)
- added shortcut header `include/bredis.hpp`
- added redis-streams usage example
- added multi-thread example

### 0.05
- fixed level 4 warning in MSVC
- fixed compilation issues on archlinux
- improved documentation (numerous typos etc.)

### 0.04
 - [bugfix] removed unneeded `tx_buff.commit()` on `async_write` which corrupted buffer

### 0.03
- improved protocol parser (no memory allocations during input stream validity check)
- more detailed information in `protocol_error_t`
- added async `incr` speed test example
- [small API breakage] `positive_parse_result_t` was enriched with parcing policy;
now instead of `positive_parse_result_t<Iterator>` should be written:

```cpp
using Policy = r::parsing_policy::keep_result;
using result_t = r::parse_result_mapper_t<Iterator, Policy>;
```
- [small API breakage] `protocol_error_t` instead of `std::string what` member
now contains `boost::system::error_code code`

### 0.02
- added windows support
- added coroutines & futures support
- generalised (templated) buffer support
- changed return type: instead of result of parsing just result markers are returned, extraction of result can be done as separate step
- dropped queing support (queuing policy should be implemented at more higher levels)
- dropped subscription support (can be implemented at higher levels)
- dropped internal buffers (can be implemented at higher levels)
- dropped explicit cancellation (socket reference can be passed to connector, and cancellation
can be done on the socket object outside of the connector)

### 0.01
- initial version

## Performance

Results achieved with `examples/speed_test_async_multi.cpp` for 1 thread, Intel Core i7-8550U, void-linux, gcc 8.3.0

 | bredis (commands/s) | bredis(*) (commands/s) | redox (commands/s)|
 |---------------------|------------------------|-------------------|
 |      1.80845e+06    |      2.503e+06         |    0.999375+06    |

These results are not completely fair, because of the usage of different semantics in the
APIs; however they are still interesting, as they are using different
underlying event libraries ([Boost::ASIO](http://www.boost.org/doc/libs/release/libs/asio/) vs [libev](http://software.schmorp.de/pkg/libev.html)) as well as redis protocol
parsing libraries (written from scratch vs [hiredis](https://github.com/redis/hiredis))

`(*)` bredis with drop_result policy, i.e. replies from redis server are
scanned only for formal correctness and never delivered to the caller.


## Work with the result

The general idea is that the result of trying to parse a redis reply can be either: not enough data, protocol error (in an extreme case) or some positive parse result. The last one is just **markers** of the result, which is actually stored in the *receive buffer* (i.e. outside of markers, and outside of the bredis-connection).

The further work with markers depends on your needs: it is possible to either **scan** the result for the expected results (e.g. for a `PONG` reply on a `PING` command, or for `OK`/`QUEUED` replies on `MULTI`/`EXEC` commands) or to **extract** the results (the common redis types: `nil`, `string`, `error`, `int` or a (recursive) array of them).

When the data in the receive buffer is no longer required, it should be consumed.

Scan example:

```cpp
#include "bredis/MarkerHelpers.hpp"
...
namespace r = bredis;
...
using Buffer = boost::asio::streambuf;
...
Buffer rx_buff;
auto result_markers = c.read(rx_buff);
/* check for the response */
auto eq_pong = r::marker_helpers::equality<Iterator>("PONG");
/* print true or false */
std::cout << boost::apply_visitor(eq_pong, result_markers.result) << "\n";
/* consume the buffers, after finishing work with the markers */
rx_buff.consume(result_markers.consumed);
```

For *extraction* of results it is possible to use either one of the shipped extractors or to write a custom one. Shipped extractors detach (copy / convert) the extraction results from the receive buffer.

```cpp
#include "bredis/Extract.hpp"
...
auto result_markers = c.read(rx_buff);
auto extract = boost::apply_visitor(r::extractor<Iterator>(), result_markers.result);
/* safe to consume buffers now */
rx_buff.consume(result_markers.consumed);
/* we know what the type is, safe to unpack to string */
auto &reply_str = boost::get<r::extracts::string_t>(extract);
/* print "PONG" */
std::cout << reply_str.str << "\n";
```

Custom extractors (visitors) might be useful for performance-sensitive cases, e.g. when JSON is re-constructed in-place by using string reply markers **without** re-allocating the whole JSON-string reply.

The underlying reason for the decision to retrieve the final results in two steps (get markers and then scan/extract results) is that the *receive buffer* might be scattered (fragmented). In such cases scan and extraction can be performed without gathering receive buffers (i.e. without flattening / linearizing it) if they are separate steps.

In other words, *markers* have **reference semantics** (they refer to memory regions in the buffer, but do not own it), while *extracted results* have **value semantics** (they take ownership).

## Synchronous TCP-connection example

```cpp
#include "bredis/Connection.hpp"
#include "bredis/MarkerHelpers.hpp"

#include <boost/variant.hpp>
...
namespace r = bredis;
namespace asio = boost::asio;
...
/* define used types */
using socket_t = asio::ip::tcp::socket;
using Buffer = boost::asio::streambuf;
using Iterator = typename r::to_iterator<Buffer>::iterator_t;
...
/* establishing connection to redis is outside of bredis */
asio::ip::tcp::endpoint end_point(
    asio::ip::address::from_string("127.0.0.1"), port);
socket_t socket(io_service, end_point.protocol());
socket.connect(end_point);

/* wrap socket to bredis connection */
r::Connection<socket_t> c(std::move(socket));

/* synchronously write command */
c.write("ping");

/* buffer is allocated outside of bredis connection*/
Buffer rx_buff;
/* get the result markers */
auto result_markers = c.read(rx_buff);
/* check for the response */
auto eq_pong = r::marker_helpers::equality<Iterator>("PONG");
/* print true */
std::cout << boost::apply_visitor(eq_pong, result_markers.result) << "\n";
/* consume the buffers, after finishing work with the markers */
rx_buff.consume(result_markers.consumed);
```

In the ping example above the `PONG` reply string from redis is not (re)allocated, but directly scanned from the `rx_buff` using a result markers. This can be useful for performance-sensitive cases, e.g. when JSON is re-constructed in-place by using string reply markers **without** re-allocating the whole JSON-string reply.

In cases where you need to **extract** the reply (i.e. to detach it from `rx_buff`), the following can be done:

```cpp
#include "bredis/Extract.hpp"
...
auto result_markers = c.read(rx_buff);
/* extract the results */
auto extract = boost::apply_visitor(r::extractor<Iterator>(), result_markers.result);
/* safe to consume buffers now */
rx_buff.consume(result_markers.consumed);
/* we know what the type is, safe to unpack to string */
auto &reply_str = boost::get<r::extracts::string_t>(extract);
/* print "PONG" */
std::cout << reply_str.str << "\n";
```

The examples above throw an exception in case of I/O or protocol errors. Another way to use the API is

```cpp
boost::system::error_code ec;
c.write("ping", ec);
...
parse_result = c.read(rx_buff, ec);
```

in case you don't want the throw-exception behaviour.

## Asynchronous TCP-connection example
```cpp
#include "bredis/Connection.hpp"
#include "bredis/MarkerHelpers.hpp"
...
namespace r = bredis;
namespace asio = boost::asio;
namespace sys = boost::system;
...
using socket_t = asio::ip::tcp::socket;
using Buffer = boost::asio::streambuf;
using Iterator = typename r::to_iterator<Buffer>::iterator_t;
using Policy = r::parsing_policy::keep_result;
using result_t = r::parse_result_mapper_t<Iterator, Policy>;

...
/* establishing the connection to redis is outside of bredis */
asio::ip::tcp::endpoint end_point(
    asio::ip::address::from_string("127.0.0.1"), port);
socket_t socket(io_service, end_point.protocol());
socket.connect(end_point);
...
Buffer tx_buff, rx_buff;
c.async_write(
    tx_buff, r::single_command_t{"llen", "my-queue"}, [&](const sys::error_code &ec, std::size_t bytes_transferred) {
        /* tx_buff must be consumed when it is no longer needed */
        tx_buff.consume(bytes_transferred);
        c.async_read(rx_buff, [&](const sys::error_code &ec, result_t &&r) {
            /* see above how to work with the result */
            auto extract = boost::apply_visitor(r::extractor<Iterator>(), r.result);
            auto &queue_size = boost::get<r::extracts::int_t>(extract);
            std::cout << "queue size: " << queue_size << "\n";
            ...
            /* consume rx_buff when it is no longer needed */
            rx_buff.consume(r.consumed);
        });
    });

```

In the example above separate receive and transfer buffers are used. In theory you can use only one buffer for both operations, but you must ensure that it will not be used simultaneously for reading and writing, in other words you cannot use the [pipelining](https://redis.io/topics/pipelining) redis feature.


## Asynchronous unix domain socket connections

The same as above, except the underlying socket type must be changed:

```cpp
using socket_t = asio::local::stream_protocol::socket;
```

## Subscriptions

There is no specific support for subscriptions, but you can easily build your own like

### synchronous subscription

```cpp
r::single_command_t subscribe_cmd{"subscribe", "some-channel1", "some-channel2"};
c.write(subscribe_cmd);
Buffer rx_buff;

/* get the 2 confirmations, as we subscribed to 2 channels */
r::marker_helpers::check_subscription<Iterator> check_subscription{std::move(subscribe_cmd)};
for (auto i = 0; i < 2; ++i){
  auto result_markers = c.read(rx_buff);
  bool confirmed =  boost::apply_visitor(check_subscription, result_markers.result);
  if (!confirmed) {
    // do something!
    ...;
  }
  rx_buff.consume(result_markers.consumed);
}

while(true) {
  auto result_markers = c.read(rx_buff);
  auto extract = boost::apply_visitor(r::extractor<Iterator>(), result_markers.result);
  rx_buff.consume(result_markers.consumed);

  /* process the result  */
  auto& array_reply = boost::get<r::extracts::array_holder_t>(extract);
  auto* type_reply = boost::get<r::extracts::string_t>(&array_reply.elements[0]);
  if (type_reply && type_reply->str == "message") {
      auto& channel = boost::get<r::extracts::string_t>(array_reply.elements[1]);
      auto& payload = boost::get<r::extracts::string_t>(array_reply.elements[2]);
      ...
  }
}
```

See `examples/synch-subscription.cpp` for the full example.

### asynchronous subscription

These work similarly to the synchronous approach. However you have to provide a read callback initially and again after each successfull read
```cpp
using Policy = r::parsing_policy::keep_result;
using ParseResult = r::parse_result_mapper_t<Iterator, Policy>;
using read_callback_t = std::function<void(const boost::system::error_code &error_code, ParseResult &&r)>;
using Extractor = r::extractor<Iterator>;
...
/* we can execute the subscription command synchronously, as it is easier */
c.command("subscribe", "channel-1", "channel-2");
...
Buffer rx_buff;
read_callback_t notification_callback = [&](const boost::system::error_code,
                                            ParseResult &&r) {
    auto extract = boost::apply_visitor(Extractor(), r.result);
    rx_buff.consume(r.consumed);
    /* process the result, see above */
    ...
    /* re-trigger new message processing */
    c.async_read(rx_buff, notification_callback);
};

/* initialise listening subscriptions */
c.async_read(rx_buff, notification_callback);
```

See `examples/stream-parse.cpp` for the full example.

## Transactions

There is no specific support for transactions in bredis, but you can easily build your own for your needs.

First, wrap your commands into a transaction:

```cpp

r::command_container_t tx_commands = {
    r::single_command_t("MULTI"),
    r::single_command_t("INCR", "foo"),
    r::single_command_t("GET", "bar"),
    r::single_command_t("EXEC"),
};
r::command_wrapper_t cmd(tx_commands);
c.write(cmd);
```

Then, as above there were **4** redis commands, there we should receive **4** redis
replies: `OK`, `QUEUED`, `QUEUED` followed by the array of results of the execution of the commands
in the transaction (i.e. results for `INCR` and `GET` above)

```cpp
Buffer rx_buff;
c.async_read(rx_buff, [&](const sys::error_code &ec, result_t&& r){
    auto &replies = boost::get<r::markers::array_holder_t<Iterator>>(r.result);
    /* scan stream for OK, QUEUED, QUEUED */
    ...
    assert(replies.elements.size() == 4);
    auto eq_OK = r::marker_helpers::equality<Iterator>("OK");
    auto eq_QUEUED = r::marker_helpers::equality<Iterator>("QUEUED");
    assert(boost::apply_visitor(eq_OK, replies.elements[0]));
    assert(boost::apply_visitor(eq_QUEUED, replies.elements[1]));
    assert(boost::apply_visitor(eq_QUEUED, replies.elements[2]));

    /* get tx replies */
    auto &tx_replies = boost::get<r::markers::array_holder_t<Iterator>>(replies.elements[3]);
    ...;
    rx_buff.consume(r.consumed);
},
4); /* pay attention here */

```

## Futures & Coroutines

Done in a similiar way as in `Boost::ASIO` (special thanks to Vinnie Falko for the suggestion)

### Futures

```cpp
#include <boost/asio/use_future.hpp>
...
Buffer rx_buff, tx_buff;
auto f_tx_consumed = c.async_write(tx_buff, "ping", asio::use_future);
auto f_result_markers = c.async_read(rx_buff, asio::use_future);
...
tx_buff.consume(f_tx_consumed.get());
auto result_markers = f_result_markers.get();
/* scan/extract result, and consume rx_buff as usual */
```

### Coroutines

```cpp
#include <boost/asio/spawn.hpp>
Buffer rx_buff, tx_buff;

boost::asio::spawn(
    io_service, [&](boost::asio::yield_context yield) mutable {
        boost::system::error_code error_code;
        auto consumed = c.async_write(tx_buff, "ping", yield[error_code]);
        tx_buff.consume(consumed);
        ...
        auto parse_result = c.async_read(rx_buff, yield[error_code], 1);
        /* scan/extract result */
        rx_buff.consume(parse_result.consumed);
    });
```

## Steams

There is no specific support for streams (appeared in redis 5.0) in bredis,
they are just usual `XADD`, `XRANGE` etc. commands and corresponding replies.

```cpp
...
Buffer rx_buff;
c.write(r::single_command_t{ "XADD", "mystream", "*", "cpu-temp", "23.4", "load", "2.3" });
auto parse_result1 = c.read(rx_buff);
auto extract1 = boost::apply_visitor(Extractor(), parse_result1.result);
auto id1 = boost::get<r::extracts::string_t>(extract1);

c.write(r::single_command_t{ "XADD", "mystream", "*", "cpu-temp", "23.2", "load", "2.1" });
auto parse_result2 = c.read(rx_buff);
auto extract2 = boost::apply_visitor(Extractor(), parse_result2.result);
auto id2 = boost::get<r::extracts::string_t>(extract2);
rx_buff.consume(parse_result2.consumed);

c.write(r::single_command_t{ "XRANGE" , "mystream",  id1.str, id2.str});
auto parse_result3 = c.read(rx_buff);
auto extract3 = boost::apply_visitor(Extractor(), parse_result3.result);
rx_buff.consume(parse_result3.consumed);

auto& outer_arr = boost::get<r::extracts::array_holder_t>(extract3);
auto& inner_arr1 = boost::get<r::extracts::array_holder_t>(outer_arr.elements[0]);
auto& inner_arr2 = boost::get<r::extracts::array_holder_t>(outer_arr.elements[1]);
...

```


## Inspecting network traffic

See `t/SocketWithLogging.hpp` for an example. The main idea is quite simple:
Instead of providing a real socket implementation supplied by `Boost::ASIO`,
provide a wrapper (proxy) which will **spy** on the traffic before
delegating it to/from a `Boost::ASIO` socket.

## Cancellation & other socket operations

There is nothing specific to this in bredis. If you need low-level socket
operations, instead of moving *socket* into bredis connection, you can
simply move a *reference* to it and keep (own) the socket somewhere
outside of the bredis connection.

```cpp
using socket_t = asio::ip::tcp::socket;
using next_layer_t = socket_t &;
...
asio::ip::tcp::endpoint end_point(asio::ip::address::from_string("127.0.0.1"), port);
socket_t socket(io_service, end_point.protocol());
socket.connect(end_point);
r::Connection<next_layer_t> c(socket);
...
socket.cancel();
```

## Thread-safety

`bredis` itself is thread-agnostic, however the underlying socket (`next_layer_t`)
and used buffers are usually not thread-safe. To handle that in multi-thead
environment the access to those objects should be sequenced via
`asio::io_context::strand` . See the `examples/multi-threads-1.cpp`.


## parsing_policy::drop_result
The performance still can be boosted if it is known beforehand that the response from
redis server is not needed at all. For example, the only possible response to `PING`
command is `PONG` reply, usually there is no sense it validating that `PONG` reply,
as soon as it is known, that redis-server alredy delivered us **some** reply
(in practice it is `PONG`). Another example is `SET` command, when redis-server
**usually** replies with `OK`.

With `parsing_policy::drop_result` the reply result is just verified with formal
compliance to redis protocol, and then it is discarded.

It should be noted, that redis can reply back with error, which aslo correct
reply, but the caller side isn't able to see it when `parsing_policy::drop_result`
is applied. So, it should be used with care, when you know what your are doing. You have
been warned.

It is safe, however, to mix different parsing policies on the same connection,
i.e. write `SET` command and read it's reply with `parsing_policy::drop_result` and
then write `GET` command and read it's reply with `parsing_policy::keep_result`.
See the `examples/speed_test_async_multi.cpp`.

## API

There's a convenience header include/bredis.hpp, doing `#include "bredis.hpp"` will include
every header under include/bredis/ .

### `Iterator` template

The underlying iterator type used for the dynamic buffer type (e.g. `boost::asio::streambuf`)

### `redis_result_t<Iterator>`

Header: `include/bredis/Markers.hpp`

Namespace: `bredis::markers`


`boost::variant` for the basic types in the redis protocol [](https://redis.io/topics/protocol),
i.e. the following marker types :
- `nil_t<Iterator>`
- `int_t<Iterator>`
- `string_t<Iterator>` (simple string and bulk strings)
- `error_t<Iterator>`
- `array_holder_t<Iterator>`

The basic type is `string_t<Iterator>`, which contains `from` and `to` members (`Iterator`)
to where the string is held. String does not contain the special redis-protocol symbols or any other
metadata, i.e. it can be used to extract/flatten the whole string.

`nil_t<Iterator>`, `int_t<Iterator>`, `error_t<Iterator>` just have a `string` member
to point to the underlying string in the redis protocol.

`array_holder_t` is recursive wrapper for the `redis_result_t<Iterator>`, it contains a
`elements` member of `std::array` of `redis_result_t<Iterator>` type.

### `parse_result_t<Iterator, Policy>`

Header: `include/bredis/Result.hpp`

Namespace: `bredis`

Represents the results of a parse attempt. It is a `boost::variant` of the following types:
- `not_enough_data_t`
- `protocol_error_t`
- `positive_parse_result_t<Iterator, Policy>`

`not_enough_data_t` is a empty struct. It means that buffer just does not contain enough
information to completely parse it.

`protocol_error_t` has a `boost::system::error_code code` member. It describes the error
in the protocol (e.g. when the type in the stream is specified as an integer, but it cannot be
converted to an integer). This error should never occur in production code, meaning
that no (logical) errors are expected in the redis-server nor in the bredis parser. The
error might occur if the buffer is corrupted.

`Policy` (namespace `bredis::parsing_policy`) specifies what to do with the result:
Either drop it (`bredis::parsing_policy::drop_result`) or keep it
(`bredis::parsing_policy::keep_result`). The helper
`parse_result_mapper_t<Iterator, Policy>` helps to get the proper
`positive_parse_result_t<Iterator, Policy>` type.

`positive_parse_result_t<Iterator, Policy>` contains members:
- `markers::redis_result_t<Iterator> result` - the result of mark-up buffer; can be used
either for scanning for particular results or for extraction of results. Valid only
for `keep_result` policy.
- `size_t consumed` - how many bytes of receive buffer must be consumed after
using the `result` field.

### marker helpers

Header: `include/bredis/MarkerHelpers.hpp`

Namespace: `bredis::marker_helpers`

#### `stringizer<Iterator>`

Apply this `boost::static_visitor<std::string>`s to
stringize the result (can be useful for debugging).

#### `equality<Iterator>`

Apply this `boost::static_visitor<bool>` to find a *string* in the
parsed results (the markup can point to integer types, but as it
is transferred as a string anyway, it still can be founded as string
too).

Constructor: `equality<Iterator>(std::string str)`

#### `check_subscription<Iterator>`

This `boost::static_visitor<bool>` helper is used to check
whether the redis reply confirms one of the requested channels. Hence,
the constructor is `check_subscription(single_command_t)`.

Usually, the redis subscription reply is in the form:
```
[array] {
    [string] "subcribe",
    [string] channel_name,
    [int] reference
}
```

So it checks that:
1. The redis reply is a 3-element array
2. The 1st reply element is a string, and it *case-insensitively*
matches the command, i.e. it is assumed, that
command will be `subscribe` or `psubscribe` depending on the original command
3. That the 3rd reply element is a reference, and it is present
among the command arguments.

It is possible to reuse the same `check_subscription<Iterator>`
on *multiple* redis replies to a single subsription command for multiple channels.

Example:

```cpp
bredis::single_command_t subscribe_cmd{
    "subscribe", "channel-1", "channel-2"
};
...
// write the command, so the subscribe_cmd
// will be no longer be required
...;
bredis::marker_helpers::check_subscription<Iterator>
    check_subscription{std::move(subscribe_cmd)};
...;
// get the 1st reply
auto parse_result = ...;
bool channel_1_ok = boost::apply_visitor(check_subscription, parse_result.result);
...;
// get the 2nd reply
parse_result = ...;
bool channel_2_ok = boost::apply_visitor(check_subscription, parse_result.result);
```

### `command_wrapper_t`

Header: `include/bredis/Command.hpp`

Namespace: `bredis`

`boost::variant` for the basic commands:
- `single_command_t`
- `command_container_t`

`single_command_t` represents a single redis command with all its arguments, e.g.:

```cpp
// compile-time version
r::single_command_t cmd_ping {"ping"};
r::single_command_t cmd_get {"get", "queu-name"};
...
// or runtime-version
std::vector<std::string> subscription_items { "subscribe", "channel-a", "channel-b"};
r::single_command_t cmd_subscribe {
    subscription_items.cbegin(),
    subscription_items.cend()
};
```

The arguments must be conversible to `boost::string_ref`.

`command_container_t` is a `std::vector` of `single_command_t`. It is useful for transactions
or bulk message creation.

### `Connection<NextLayer>`

Header: `include/bredis/Connection.hpp`

Namespace: `bredis`

A thin wrapper around `NextLayer`; represents a connection to redis. `NextLayer` can
be either `asio::ip::tcp::socket` or `asio::ip::tcp::socket&` or a custom wrapper, which
follows the specification of `asio::ip::tcp::socket`.

The constructor `template <typename... Args> Connection(Args &&... args)` is used for the
construction of NextLayer (stream interface).

Stream interface accessors:
- `NextLayer &next_layer()`
- `const NextLayer &next_layer() const`

return the underlying stream object.

#### Synchronous interface

Performs a synchonous write of a redis command:

- `void write(const command_wrapper_t &command)`
- `void write(const command_wrapper_t &command, boost::system::error_code &ec)`

Performs a synchronous read of a redis result until the buffer is parsed or
some error (protocol or I/O) occurs:

- `template <typename DynamicBuffer> positive_parse_result_t<Iterator, Policy = bredis::parsing_policy::keep_result> read(DynamicBuffer &rx_buff)`
- `template <typename DynamicBuffer> positive_parse_result_t<Iterator, Policy = bredis::parsing_policy::keep_result> read(DynamicBuffer &rx_buff, boost::system::error_code &ec);`

`DynamicBuffer` must conform to the `boost::asio::streambuf` interface.

#### Asynchronous interface

##### async_write

The `WriteCallback` template should be a callable object with the signature:

`void (const boost::system::error_code&, std::size_t bytes_transferred)`

The asynchronous write has the following signature:

```cpp
void-or-deduced
async_write(DynamicBuffer &tx_buff, const command_wrapper_t &command,
                WriteCallback write_callback)
```

It writes the redis command (or commands) into a *transfer buffer*, sends them
to the *next_layer* stream and invokes `write_callback` after completion.

`tx_buff` must consume `bytes_transferred` upon `write_callback` invocation.

The client must guarantee that `async_write` is not invoked until the previous
invocation is finished.

##### async_read

`ReadCallback` template should be a callable object with the signature:

`void(boost::system::error_code, r::positive_parse_result_t<Iterator, Policy = bredis::parsing_policy::keep_result>&& result)`

The asynchronous read has the following signature:

```cpp
void-or-deduced
async_read(DynamicBuffer &rx_buff, ReadCallback read_callback,
               std::size_t replies_count = 1, Policy = bredis::parsing_policy::keep_result{});
```

It reads `replies_count` replies from the *next_layer* stream, which will be
stored in `rx_buff`, or until an error (I/O or protocol) is encountered; then
`read_callback` will be invoked.

If `replies_count` is greater than `1`, the result type will always be
`bredis::array_wrapper_t`; if the `replies_count` is `1` then the result type
depends on redis answer type.

On `read_callback` invocation with a successful parse result it is expected,
that `rx_buff` will consume the amount of bytes specified in the `result`.

The client must guarantee that `async_read` is not invoked until the previous
invocation is finished. If you invoke `async_read` from `read_callback`
don't forget to **consume** `rx_buff` first, otherwise it leads to
subtle bugs.

# License

MIT

# Contributors

- [Derek Colley](https://github.com/dcolley)
- [Stefan Hacker](https://github.com/hacst)
- [nkochakian](https://github.com/nkochakian)
- [Yuval Hager](https://github.com/yhager)
- [Vinnie Falco](https://github.com/vinniefalco)
- [Stephen Coleman](https://github.com/omegacoleman)
- [maxtorm miximtor](https://github.com/miximtor)

## See also
- https://github.com/Cylix/cpp_redis
- https://github.com/hmartiro/redox
- https://github.com/nekipelov/redisclient
