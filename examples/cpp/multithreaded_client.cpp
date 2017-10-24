/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

//
// C++11 or greater
//
// A multi-threaded client that calls proton::container::run() in one thread, sends
// messages in another and receives messages in a third.
//
// Note this client does not deal with flow-control. If the sender is faster
// than the receiver, messages will build up in memory on the sending side.
// See @ref multithreaded_client_flow_control.cpp for a more complex example with
// flow control.
//
// NOTE: no proper error handling

#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver.hpp>
#include <proton/sender.hpp>
#include <proton/work_queue.hpp>

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>

// Lock output from threads to avoid scrambling
std::mutex out_lock;
#define OUT(x) do { std::lock_guard<std::mutex> l(out_lock); x; } while (false)

// Handler for a single thread-safe sending and receiving connection.
class client : public proton::messaging_handler {
    // Invariant
    const std::string url_;
    const std::string address_;

    // Only used in proton handler thread
    proton::sender sender_;

    // Shared by proton and user threads, protected by lock_
    std::mutex lock_;
    proton::work_queue *work_queue_;
    std::condition_variable sender_ready_;
    std::queue<proton::message> messages_;
    std::condition_variable messages_ready_;

  public:
    client(const std::string& url, const std::string& address) : url_(url), address_(address), work_queue_(0) {}

    // Thread safe
    void send(const proton::message& msg) {
        // Use [=] to copy the message, we cannot pass it by reference since it
        // will be used in another thread.
        work_queue()->add([=]() { sender_.send(msg); });
    }

    // Thread safe
    proton::message receive() {
        std::unique_lock<std::mutex> l(lock_);
        while (messages_.empty()) messages_ready_.wait(l);
        auto msg = std::move(messages_.front());
        messages_.pop();
        return msg;
    }

    // Thread safe
    void close() {
        work_queue()->add([=]() { sender_.connection().close(); });
    }

  private:

    proton::work_queue* work_queue() {
        // Wait till work_queue_ and sender_ are initialized.
        std::unique_lock<std::mutex> l(lock_);
        while (!work_queue_) sender_ready_.wait(l);
        return work_queue_;
    }

    // == messaging_handler overrides, only called in proton handler thread

    // Note: this example creates a connection when the container starts.
    // To create connections after the container has started, use
    // container::connect().
    // See @ref multithreaded_client_flow_control.cpp for an example.
    void on_container_start(proton::container& cont) override {
        cont.connect(url_);
    }

    void on_connection_open(proton::connection& conn) override {
        conn.open_sender(address_);
        conn.open_receiver(address_);
    }

    void on_sender_open(proton::sender& s) override {
        // sender_ and work_queue_ must be set atomically
        std::lock_guard<std::mutex> l(lock_);
        sender_ = s;
        work_queue_ = &s.work_queue();
        sender_ready_.notify_all();
    }

    void on_message(proton::delivery& dlv, proton::message& msg) override {
        std::lock_guard<std::mutex> l(lock_);
        messages_.push(msg);
        messages_ready_.notify_all();
    }

    void on_error(const proton::error_condition& e) override {
        OUT(std::cerr << "unexpected error: " << e << std::endl);
        exit(1);
    }
};

int main(int argc, const char** argv) {
    try {
        if (argc != 4) {
            std ::cerr <<
                "Usage: " << argv[0] << " CONNECTION-URL AMQP-ADDRESS MESSAGE-COUNT\n"
                "CONNECTION-URL: connection address, e.g.'amqp://127.0.0.1'\n"
                "AMQP-ADDRESS: AMQP node address, e.g. 'examples'\n"
                "MESSAGE-COUNT: number of messages to send\n";
            return 1;
        }
        const char *url = argv[1];
        const char *address = argv[2];
        int n_messages = atoi(argv[3]);

        client cl(url, address);
        proton::container container(cl);
        std::thread container_thread([&]() { container.run(); });

        std::thread sender([&]() {
                for (int i = 0; i < n_messages; ++i) {
                    proton::message msg(std::to_string(i + 1));
                    cl.send(msg);
                    OUT(std::cout << "sent \"" << msg.body() << '"' << std::endl);
                }
            });

        int received = 0;
        std::thread receiver([&]() {
                for (int i = 0; i < n_messages; ++i) {
                    auto msg = cl.receive();
                    OUT(std::cout << "received \"" << msg.body() << '"' << std::endl);
                    ++received;
                }
            });

        sender.join();
        receiver.join();
        cl.close();
        container_thread.join();
        std::cout << received << " messages sent and received" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
