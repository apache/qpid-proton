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

#include "../options.hpp"
#include "mt_container.hpp"

#include <proton/connection.hpp>
#include <proton/default_container.hpp>
#include <proton/delivery.hpp>
#include <proton/error_condition.hpp>
#include <proton/listen_handler.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/sender_options.hpp>
#include <proton/source_options.hpp>
#include <proton/thread_safe.hpp>

#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

#include "../fake_cpp11.hpp"

// Thread safe queue.
// Stores messages, notifies subscribed connections when there is data.
class queue {
  public:
    queue(const std::string& name) : name_(name) {}

    std::string name() const { return name_; }

    // Push a message onto the queue.
    // If the queue was previously empty, notify subscribers it has messages.
    // Called from receiver's connection.
    void push(const proton::message &m) {
        std::lock_guard<std::mutex> g(lock_);
        messages_.push_back(m);
        if (messages_.size() == 1) { // Non-empty, notify subscribers
            for (auto cb : callbacks_)
                cb(this);
            callbacks_.clear();
        }
    }

    // If the queue is not empty, pop a message into m and return true.
    // Otherwise save callback to be called when there are messages and return false.
    // Called from sender's connection.
    bool pop(proton::message& m, std::function<void(queue*)> callback) {
        std::lock_guard<std::mutex> g(lock_);
        if (messages_.empty()) {
            callbacks_.push_back(callback);
            return false;
        } else {
            m = std::move(messages_.front());
            messages_.pop_front();
            return true;
        }
    }

  private:
    const std::string name_;
    std::mutex lock_;
    std::deque<proton::message> messages_;
    std::vector<std::function<void(queue*)> > callbacks_;
};

/// Thread safe map of queues.
class queues {
  public:
    queues() : next_id_(0) {}

    // Get or create the named queue.
    queue* get(const std::string& name) {
        std::lock_guard<std::mutex> g(lock_);
        auto i = queues_.insert(queue_map::value_type(name, nullptr)).first;
        if (!i->second)
            i->second.reset(new queue(name));
        return i->second.get();
    }

    // Create a dynamic queue with a unique name.
    queue* dynamic() {
        std::ostringstream os;
        os << "_dynamic_" << next_id_++;
        return get(os.str());
    }

  private:
    typedef std::map<std::string, std::unique_ptr<queue> > queue_map;

    std::mutex lock_;
    queue_map queues_;
    std::atomic<int> next_id_; // Use to generate unique queue IDs.
};

/// Broker connection handler. Things to note:
///
/// 1. Each handler manages a single connection.
///
/// 2. For a *single connection* calls to proton::handler functions and calls to
/// function objects passed to proton::event_loop::inject() are serialized,
/// i.e. never called concurrently. Handlers can have per-connection state
/// without needing locks.
///
/// 3. Handler/injected functions for *different connections* can be called
/// concurrently. Resources used by multiple connections (e.g. the queues in
/// this example) must be thread-safe.
///
/// 4. You can 'inject' work to be done sequentially using a connection's
/// proton::event_loop. In this example, we create a std::function callback
/// that we pass to queues, so they can notify us when they have messages.
///
class broker_connection_handler : public proton::messaging_handler {
  public:
    broker_connection_handler(queues& qs) : queues_(qs) {}

    void on_connection_open(proton::connection& c) OVERRIDE {
        // Create the has_messages callback for queue subscriptions.
        //
        // Make a std::shared_ptr to a thread_safe handle for our proton::connection.
        // The connection's proton::event_loop will remain valid as a shared_ptr exists.
        std::shared_ptr<proton::thread_safe<proton::connection> > ts_c = make_shared_thread_safe(c);

        // Make a lambda function to inject a call to this->has_messages() via the proton::event_loop.
        // The function is bound to a shared_ptr so this is safe. If the connection has already closed
        // proton::event_loop::inject() will drop the callback.
        has_messages_callback_ = [this, ts_c](queue* q) mutable {
            ts_c->event_loop()->inject(
                std::bind(&broker_connection_handler::has_messages, this, q));
        };

        c.open();            // Accept the connection
    }

    // A sender sends messages from a queue to a subscriber.
    void on_sender_open(proton::sender &sender) OVERRIDE {
        queue *q = sender.source().dynamic() ?
            queues_.dynamic() : queues_.get(sender.source().address());
        sender.open(proton::sender_options().source((proton::source_options().address(q->name()))));
        std::cout << "sending from " << q->name() << std::endl;
    }

    // We have credit to send a message.
    void on_sendable(proton::sender &s) OVERRIDE {
        queue* q = sender_queue(s);
        if (!do_send(q, s))     // Queue is empty, save ourselves in the blocked set.
            blocked_.insert(std::make_pair(q, s));
    }

    // A receiver receives messages from a publisher to a queue.
    void on_receiver_open(proton::receiver &r) OVERRIDE {
        std::string qname = r.target().address();
        if (qname == "shutdown") {
            std::cout << "broker shutting down" << std::endl;
            // Sending to the special "shutdown" queue stops the broker.
            r.connection().container().stop(
                proton::error_condition("shutdown", "stop broker"));
        } else {
            std::cout << "receiving to " << qname << std::endl;
        }
    }

    // A message is received.
    void on_message(proton::delivery &d, proton::message &m) OVERRIDE {
        std::string qname = d.receiver().target().address();
        queues_.get(qname)->push(m);
    }

    void on_session_close(proton::session &session) OVERRIDE {
        // Erase all blocked senders that belong to session.
        auto predicate = [session](const proton::sender& s) {
            return s.session() == session;
        };
        erase_sender_if(blocked_.begin(), blocked_.end(), predicate);
    }

    void on_sender_close(proton::sender &sender) OVERRIDE {
        // Erase sender from the blocked set.
        auto range = blocked_.equal_range(sender_queue(sender));
        auto predicate = [sender](const proton::sender& s) { return s == sender; };
        erase_sender_if(range.first, range.second, predicate);
    }

    void on_error(const proton::error_condition& e) OVERRIDE {
        std::cerr << "error: " << e.what() << std::endl;
    }
    // The container calls on_transport_close() last.
    void on_transport_close(proton::transport&) OVERRIDE {
        delete this;            // All done.
    }

  private:
    typedef std::multimap<queue*, proton::sender> blocked_map;

    // Get the queue associated with a sender.
    queue* sender_queue(const proton::sender& s) {
        return queues_.get(s.source().address()); // Thread safe.
    }

    // Only called if we have credit. Return true if we sent a message.
    bool do_send(queue* q, proton::sender &s) {
        proton::message m;
        bool popped =  q->pop(m, has_messages_callback_);
        if (popped)
            s.send(m);
        /// if !popped the queue has saved the callback for later.
        return popped;
    }

    // Called via the connection's proton::event_loop when q has messages.
    // Try all the blocked senders.
    void has_messages(queue* q) {
        auto range = blocked_.equal_range(q);
        for (auto i = range.first; i != range.second;) {
            if (i->second.credit() <= 0 || do_send(q, i->second))
                i = blocked_.erase(i); // No credit or send was successful, stop blocked.
            else
                ++i;            // have credit, didn't send, keep blocked
        }
    }

    // Use to erase closed senders from blocked_ set.
    template <class Predicate>
    void erase_sender_if(blocked_map::iterator begin, blocked_map::iterator end, Predicate p) {
        for (auto i = begin; i != end; ) {
            if (p(i->second))
                i = blocked_.erase(i);
            else
                ++i;
        }
    }

    queues& queues_;
    blocked_map blocked_;
    std::function<void(queue*)> has_messages_callback_;
    proton::connection connection_;
};


class broker {
  public:
    broker(const std::string addr) :
        container_(make_mt_container("mt_broker")), listener_(queues_)
    {
        container_->listen(addr, listener_);
        std::cout << "broker listening on " << addr << std::endl;
    }

    void run() {
        std::vector<std::thread> threads(std::thread::hardware_concurrency()-1);
        for (auto& t : threads)
            t = std::thread(&proton::container::run, container_.get());
        container_->run();      // Use this thread too.
        for (auto& t : threads)
            t.join();
    }

  private:
    struct listener : public proton::listen_handler {
        listener(queues& qs) : queues_(qs) {}

        proton::connection_options on_accept() OVERRIDE{
            return proton::connection_options().handler(*(new broker_connection_handler(queues_)));
        }

        void on_error(const std::string& s) OVERRIDE {
            std::cerr << "listen error: " << s << std::endl;
            throw std::runtime_error(s);
        }
        queues& queues_;
    };

    queues queues_;
    std::unique_ptr<proton::container> container_;
    listener listener_;
};

int main(int argc, char **argv) {
    // Command line options
    std::string address("0.0.0.0");
    example::options opts(argc, argv);
    opts.add_value(address, 'a', "address", "listen on URL", "URL");
    try {
        opts.parse();
        broker(address).run();
        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "broker shutdown: " << e.what() << std::endl;
    }
    return 1;
}
