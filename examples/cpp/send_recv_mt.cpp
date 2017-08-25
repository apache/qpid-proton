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

// C++11 only
//
// A multi-threaded client that sends and receives messages from multiple AMQP
// addresses.
//
// Demonstrates how to:
//
// - implement proton handlers that interact with user threads safely
// - block user threads calling send() to respect AMQP flow control
// - use AMQP flow control to limit message buffering for receivers
//
// We define mt_sender and mt_receiver classes with simple, thread-safe blocking
// send() and receive() functions.
//
// These classes are also privately proton::message_handler instances. They use
// the thread-safe proton::work_queue and standard C++ synchronization (std::mutex
// etc.) to pass messages between user and proton::container threads.
//
// NOTE: no proper error handling

#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver_options.hpp>
#include <proton/sender.hpp>
#include <proton/work_queue.hpp>

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>

// Lock to serialize std::cout, std::cerr used from multiple threads.
std::mutex out_lock;
#define LOCK(EXPR) do { std::lock_guard<std::mutex> l(out_lock); EXPR; } while(0)
#define COUT(EXPR) do { LOCK(std::cout << EXPR); } while(0)
#define CERR(EXPR) do { LOCK(std::cerr << EXPR); } while(0)

// A thread-safe sending connection.
class mt_sender : private proton::messaging_handler {
    // Only used in proton thread
    proton::sender sender_;

    // Shared by proton and user threads, use lock_ to protect.
    std::mutex lock_;
    proton::work_queue* work_queue_;   // Messages waiting to be sent
    std::condition_variable can_send_; // Signal sending threads
    int queued_;                       // Queued messages waiting to be sent
    int credit_;                       // AMQP credit - number of messages we can send

  public:
    // Connect to url
    mt_sender(proton::container& cont, const std::string& url) :
        work_queue_(0), queued_(0), credit_(0)
    {
        // Pass *this as handler.
        cont.open_sender(url, proton::connection_options().handler(*this));
    }

    // Thread safe send()
    void send(const proton::message& m) {
        std::unique_lock<std::mutex> l(lock_);
        // Don't queue up more messages than we have credit for
        while (!(work_queue_ && queued_ < credit_))
            can_send_.wait(l);
        ++queued_;
        // Add a lambda function to the work queue.
        // This will call do_send() with a copy of m in the correct proton thread.
        work_queue_->add([=]() { this->do_send(m); });
    }

    void close() {
        std::lock_guard<std::mutex> l(lock_);
        if (work_queue_)
            work_queue_->add([this]() { this->sender_.connection().close(); });
    }

  private:
    // ==== called by proton threads only

    void on_sender_open(proton::sender& s) override {
        sender_ = s;
        std::lock_guard<std::mutex> l(lock_);
        work_queue_ = &s.work_queue();
    }

    void on_sendable(proton::sender& s) override {
        std::lock_guard<std::mutex> l(lock_);
        credit_ = s.credit();
        can_send_.notify_all(); // Notify senders we have credit
    }

    // work_queue work items is are automatically dequeued and called by proton
    // This function is called because it was queued by send()
    void do_send(const proton::message& m) {
        sender_.send(m);
        std::lock_guard<std::mutex> l(lock_);
        --queued_;                    // work item was consumed from the work_queue
        credit_ = sender_.credit();   // update credit
        can_send_.notify_all();       // Notify senders we have space on queue
    }

    void on_error(const proton::error_condition& e) override {
        CERR("unexpected error: " << e << std::endl);
        exit(1);
    }
};

// A thread safe receiving connection.
class mt_receiver : private proton::messaging_handler {
    static const size_t MAX_BUFFER = 100; // Max number of buffered messages

    // Used in proton threads only
    proton::receiver receiver_;

    // Used in proton and user threads, protected by lock_
    std::mutex lock_;
    proton::work_queue* work_queue_;
    std::queue<proton::message> buffer_; // Messages not yet returned by receive()
    std::condition_variable can_receive_; // Notify receivers of messages

  public:

    // Connect to url
    mt_receiver(proton::container& cont, const std::string& url) : work_queue_()
    {
        // NOTE:credit_window(0) disables automatic flow control.
        // We will use flow control to match AMQP credit to buffer capacity.
        cont.open_receiver(url, proton::receiver_options().credit_window(0),
                           proton::connection_options().handler(*this));
    }

    // Thread safe receive
    proton::message receive() {
        std::unique_lock<std::mutex> l(lock_);
        // Wait for buffered messages
        while (!work_queue_ || buffer_.empty())
            can_receive_.wait(l);
        proton::message m = std::move(buffer_.front());
        buffer_.pop();
        // Add a lambda to the work queue to call receive_done().
        // This will tell the handler to add more credit.
        work_queue_->add([=]() { this->receive_done(); });
        return m;
    }

    void close() {
        std::lock_guard<std::mutex> l(lock_);
        if (work_queue_)
            work_queue_->add([this]() { this->receiver_.connection().close(); });
    }

  private:
    // ==== The following are called by proton threads only.

    void on_receiver_open(proton::receiver& r) override {
        receiver_ = r;
        std::lock_guard<std::mutex> l(lock_);
        work_queue_ = &receiver_.work_queue();
        receiver_.add_credit(MAX_BUFFER); // Buffer is empty, initial credit is the limit
    }

    void on_message(proton::delivery &d, proton::message &m) override {
        // Proton automatically reduces credit by 1 before calling on_message
        std::lock_guard<std::mutex> l(lock_);
        buffer_.push(m);
        can_receive_.notify_all();
    }

    // called via work_queue
    void receive_done() {
        // Add 1 credit, a receiver has taken a message out of the buffer.
        receiver_.add_credit(1);
    }

    void on_error(const proton::error_condition& e) override {
        CERR("unexpected error: " << e << std::endl);
        exit(1);
    }
};

// ==== Example code using the mt_sender and mt_receiver

// Send n messages
void send_thread(mt_sender& s, int n) {
    for (int i = 0; i < n; ++i) {
        std::ostringstream o;
        o << std::this_thread::get_id() << ":" << i;
        s.send(proton::message(o.str()));
    }
    COUT(std::this_thread::get_id() << " sent " << n << std::endl);
}

// Receive messages till atomic remaining count is 0.
// remaining is shared among all receiving threads
void receive_thread(mt_receiver& r, std::atomic_int& remaining, bool print) {
    auto id = std::this_thread::get_id();
    int n = 0;
    while (remaining-- > 0) {
        auto m = r.receive();
        ++n;
        if (print)
            COUT(id << " received \"" << m.body() << '"' << std::endl);
    }
    COUT(id << " received " << n << " messages" << std::endl);
}

int main(int argc, const char **argv) {
    try {
        int n_threads = argc > 1 ? atoi(argv[1]) : 2;
        int n_messages = argc > 2 ? atoi(argv[2]) : 10;
        const char *url =  argc > 3 ? argv[3] : "amqp://127.0.0.1/examples";
        std::atomic_int remaining(n_messages * n_threads); // Total messages to be received
        bool print = (remaining <= 30); // Print messages for short runs only

        // Run the proton container
        proton::container container;
        auto container_thread = std::thread([&]() { container.run(); });

        // A single sender and receiver to be shared by all the threads
        mt_sender sender(container, url);
        mt_receiver receiver(container, url);

        // Start receiver threads, then sender threads.
        // Starting receivers first gives all receivers a chance to compete for messages.
        std::vector<std::thread> threads;
        for (int i = 0; i < n_threads; ++i)
            threads.push_back(std::thread([&]() { receive_thread(receiver, remaining, print); }));
        for (int i = 0; i < n_threads; ++i)
            threads.push_back(std::thread([&]() { send_thread(sender, n_messages); }));

        // Wait for threads to finish
        for (auto& n_messages_threads : threads)
            n_messages_threads.join();
        sender.close();
        receiver.close();

        container_thread.join();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
