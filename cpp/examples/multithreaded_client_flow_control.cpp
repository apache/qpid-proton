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
// A multi-threaded client that sends and receives messages from multiple AMQP
// addresses.
//
// Demonstrates how to:
//
// - implement proton handlers that interact with user threads safely
// - block sender threads to respect AMQP flow control
// - use AMQP flow control to limit message buffering for receivers threads
//
// We define sender and receiver classes with simple, thread-safe blocking
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
#include <proton/receiver.hpp>
#include <proton/receiver_options.hpp>
#include <proton/sender.hpp>
#include <proton/work_queue.hpp>

#include <atomic>
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

// Exception raised if a sender or receiver is closed when trying to send/receive
class closed : public std::runtime_error {
  public:
    closed(const std::string& msg) : std::runtime_error(msg) {}
};

// A thread-safe sending connection that blocks sending threads when there
// is no AMQP credit to send messages.
class sender : private proton::messaging_handler {
    // Only used in proton handler thread
    proton::sender sender_;

    // Shared by proton and user threads, protected by lock_
    std::mutex lock_;
    proton::work_queue *work_queue_;
    std::condition_variable sender_ready_;
    int queued_;                       // Queued messages waiting to be sent
    int credit_;                       // AMQP credit - number of messages we can send

  public:
    sender(proton::container& cont, const std::string& url, const std::string& address)
        : work_queue_(0), queued_(0), credit_(0)
    {
        cont.open_sender(url+"/"+address, proton::connection_options().handler(*this));
    }

    // Thread safe
    void send(const proton::message& m) {
        {
            std::unique_lock<std::mutex> l(lock_);
            // Don't queue up more messages than we have credit for
            while (!work_queue_ || queued_ >= credit_) sender_ready_.wait(l);
            ++queued_;
        }
        work_queue_->add([=]() { this->do_send(m); }); // work_queue_ is thread safe
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

    void on_sender_open(proton::sender& s) override {
        // Make sure sender_ and work_queue_ are set atomically
        std::lock_guard<std::mutex> l(lock_);
        sender_ = s;
        work_queue_ = &s.work_queue();
    }

    void on_sendable(proton::sender& s) override {
        std::lock_guard<std::mutex> l(lock_);
        credit_ = s.credit();
        sender_ready_.notify_all(); // Notify senders we have credit
    }

    // work_queue work items is are automatically dequeued and called by proton
    // This function is called because it was queued by send()
    void do_send(const proton::message& m) {
        sender_.send(m);
        std::lock_guard<std::mutex> l(lock_);
        --queued_;                    // work item was consumed from the work_queue
        credit_ = sender_.credit();   // update credit
        sender_ready_.notify_all();       // Notify senders we have space on queue
    }

    void on_error(const proton::error_condition& e) override {
        OUT(std::cerr << "unexpected error: " << e << std::endl);
        exit(1);
    }
};

// A thread safe receiving connection that blocks receiving threads when there
// are no messages available, and maintains a bounded buffer of incoming
// messages by issuing AMQP credit only when there is space in the buffer.
class receiver : private proton::messaging_handler {
    static const size_t MAX_BUFFER = 100; // Max number of buffered messages

    // Used in proton threads only
    proton::receiver receiver_;

    // Used in proton and user threads, protected by lock_
    std::mutex lock_;
    proton::work_queue* work_queue_;
    std::queue<proton::message> buffer_; // Messages not yet returned by receive()
    std::condition_variable can_receive_; // Notify receivers of messages
    bool closed_;

  public:

    // Connect to url
    receiver(proton::container& cont, const std::string& url, const std::string& address)
        : work_queue_(0), closed_(false)
    {
        // NOTE:credit_window(0) disables automatic flow control.
        // We will use flow control to match AMQP credit to buffer capacity.
        cont.open_receiver(url+"/"+address, proton::receiver_options().credit_window(0),
                           proton::connection_options().handler(*this));
    }

    // Thread safe receive
    proton::message receive() {
        std::unique_lock<std::mutex> l(lock_);
        // Wait for buffered messages
        while (!closed_ && (!work_queue_ || buffer_.empty())) {
            can_receive_.wait(l);
        }
        if (closed_) throw closed("receiver closed");
        proton::message m = std::move(buffer_.front());
        buffer_.pop();
        // Add a lambda to the work queue to call receive_done().
        // This will tell the handler to add more credit.
        work_queue_->add([=]() { this->receive_done(); });
        return m;
    }

    // Thread safe
    void close() {
        std::lock_guard<std::mutex> l(lock_);
        if (!closed_) {
            closed_ = true;
            can_receive_.notify_all();
            if (work_queue_) {
                work_queue_->add([this]() { this->receiver_.connection().close(); });
            }
        }
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
        OUT(std::cerr << "unexpected error: " << e << std::endl);
        exit(1);
    }
};

// ==== Example code using the sender and receiver

// Send n messages
void send_thread(sender& s, int n) {
    auto id = std::this_thread::get_id();
    for (int i = 0; i < n; ++i) {
        std::ostringstream ss;
        ss << std::this_thread::get_id() << "-" << i;
        s.send(proton::message(ss.str()));
        OUT(std::cout << id << " sent \"" << ss.str() << '"' << std::endl);
    }
    OUT(std::cout << id << " sent " << n << std::endl);
}

// Receive messages till atomic remaining count is 0.
// remaining is shared among all receiving threads
void receive_thread(receiver& r, std::atomic_int& remaining) {
    try {
        auto id = std::this_thread::get_id();
        int n = 0;
        // atomically check and decrement remaining *before* receiving.
        // If it is 0 or less then return, as there are no more
        // messages to receive so calling r.receive() would block forever.
        while (remaining-- > 0) {
            auto m = r.receive();
            ++n;
            OUT(std::cout << id << " received \"" << m.body() << '"' << std::endl);
        }
        OUT(std::cout << id << " received " << n << " messages" << std::endl);
    } catch (const closed&) {}
}

int main(int argc, const char **argv) {
    try {
        if (argc != 5) {
            std::cerr <<
                "Usage: " << argv[0] << " CONNECTION-URL AMQP-ADDRESS MESSAGE-COUNT THREAD-COUNT\n"
                "CONNECTION-URL: connection address, e.g.'amqp://127.0.0.1'\n"
                "AMQP-ADDRESS: AMQP node address, e.g. 'examples'\n"
                "MESSAGE-COUNT: number of messages to send\n"
                "THREAD-COUNT: number of sender/receiver thread pairs\n";
            return 1;
        }

        const char *url = argv[1];
        const char *address = argv[2];
        int n_messages = atoi(argv[3]);
        int n_threads = atoi(argv[4]);
        int count = n_messages * n_threads;

        // Total messages to be received, multiple receiver threads will decrement this.
        std::atomic_int remaining;
        remaining.store(count);

        // Run the proton container
        proton::container container;
        auto container_thread = std::thread([&]() { container.run(); });

        // A single sender and receiver to be shared by all the threads
        sender send(container, url, address);
        receiver recv(container, url, address);

        // Start receiver threads, then sender threads.
        // Starting receivers first gives all receivers a chance to compete for messages.
        std::vector<std::thread> threads;
        threads.reserve(n_threads*2); // Avoid re-allocation once threads are started
        for (int i = 0; i < n_threads; ++i)
            threads.push_back(std::thread([&]() { receive_thread(recv, remaining); }));
        for (int i = 0; i < n_threads; ++i)
            threads.push_back(std::thread([&]() { send_thread(send, n_messages); }));

        // Wait for threads to finish
        for (auto& t : threads) t.join();
        send.close();
        recv.close();
        container_thread.join();
        if (remaining > 0)
            throw std::runtime_error("not all messages were received");
        std::cout << count << " messages sent and received" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
