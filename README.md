# bredis
Boost::ASIO low-lever redis client (connector)

[![Travis](https://img.shields.io/travis/basiliscos/cpp-bredis.svg)]()
[![license](https://img.shields.io/github/license/basiliscos/cpp-bredis.svg)]()

## Features

- header only
- low-level controls (i.e. you can cancel, or do you manual DNS-resolving before connection)
- unix domain sockets support
- synchronous & asynchronous interface

## Syncronous TCP-connection example

```cpp
#include <bredis/SyncConnection.hpp>
#include <boost/variant.hpp>
#include <boost/utility/string_ref.hpp>
...
namespace r = bredis;
namespace asio = boost::asio;

...
/* define used socket type */
using socket_t = asio::ip::tcp::socket;
...
/* establishing connection to redis is outside of bredis */
asio::ip::tcp::endpoint end_point(
    asio::ip::address::from_string("127.0.0.1"), port);
socket_t socket(io_service, end_point.protocol());
socket.connect(end_point);

/* buffer is not allocated inside bredis */ 
asio::streambuf rx_buff;
r::SyncConnection<socket_t> connection(std::move(socket));
/* get the result, boost::variant */ 
auto result = c.command("ping", rx_buff);
/* we know what the type is, safe to unpack to string_ref */
auto &reply_str = boost::get<r::string_holder_t>(result).str;
/* now we have a string copy, which is actually "PONG" */
std::string str(reply_str.cbegin(), reply_str.cend());
```

# License 

MIT

## See also
- https://github.com/Cylix/cpp_redis
- https://github.com/blackjack/booredis
- https://github.com/nekipelov/redisclient
