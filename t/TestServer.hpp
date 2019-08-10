#pragma once

#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

#include <boost/algorithm/string/join.hpp>
#include <boost/process/child.hpp>

namespace test_server {
struct TestServer {
    using child_t = std::unique_ptr<boost::process::child>;
    child_t child;

    TestServer(std::initializer_list<std::string> &&args) {
        std::string stringized = boost::algorithm::join(args, " ");
        std::cout << "going to fork to start: " << stringized << std::endl;
        auto process = new boost::process::child(stringized);
        child.reset(process);
    }
    ~TestServer() { std::cout << "terminating child " << child->id() << "\n"; }
};

using result_t = std::unique_ptr<TestServer>;

result_t make_server(std::initializer_list<std::string> &&args) {
    return result_t{new TestServer(std::move(args))};
}
}
