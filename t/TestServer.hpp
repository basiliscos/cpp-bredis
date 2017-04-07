#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <iostream>

#include <cstdlib>

#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

namespace test_server {
struct TestServer {
    pid_t child_pid;
    TestServer(std::initializer_list<std::string> &&args) {
        auto begin = args.begin();
        auto end = args.end();

        auto program = (*begin).c_str();
        auto args_count = args.size();
        const char **c_args =
            (const char **)std::malloc(sizeof(char *) * (args_count + 1));
        auto it = begin;
        std::string stringized;
        for (auto i = 0; i < args_count; i++, it++) {
            const char *c_arg = (*it).c_str();
            c_args[i] = c_arg;
            stringized += " ";
            stringized += c_arg;
        }
        c_args[args_count] = NULL;
        std::cout << "going to fork to start: " << stringized << std::endl;

        pid_t pid = fork();
        if (pid == -1) {
            throw std::runtime_error("cannot fork");
        } else if (pid == 0) {
            // child
            std::cout << "executing in child" << std::endl;
            int result = execvp(program, (char *const *)c_args);
            std::cout << "failed to execute: " << strerror(errno) << std::endl;
            exit(-1);
        } else {
            // parent
            child_pid = pid;
            // sleep(5);
        }
    }
    ~TestServer() {
        std::cout << "terminating child " << child_pid << std::endl;
        kill(child_pid, 9);
    }
};

using result_t = std::unique_ptr<TestServer>;

result_t make_server(std::initializer_list<std::string> &&args) {
    return std::make_unique<TestServer>(std::move(args));
}
};
