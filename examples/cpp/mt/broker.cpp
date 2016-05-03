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

#include <proton/connection.hpp>
#include <proton/controller.hpp>
#include <proton/delivery.hpp>
#include <proton/handler.hpp>
#include <proton/work_queue.hpp>

#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

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
    std::atomic<uint64_t> next_id_; // Use to generate unique queue IDs.
};

/// Broker connection handler. Things to note:
///
/// Each handler manages a single connection. Proton AMQP callbacks and queue
/// callbacks via proton::work_queue are serialized per-connection, so the
/// handler does not need a lock. Handlers for different connections can be
/// called concurrently.
///
/// Senders (aka subscriptions) need some cross-thread notification:.
///
/// - a sender that gets credit calls queue::pop() in `on_sendable()`
///   - on success it sends the message immediatly.
///   - on queue empty, the sender is added to the `blocked_` set and the queue stores a callback.
/// - when a receiver thread pushes a message, the queue calls its callbacks.
/// - the callback causes a serialized call to has_messages() which re-tries all `blocked_` senders.
///
class broker_connection_handler : public proton::handler {
  public:
    broker_connection_handler(queues& qs) : queues_(qs) {}

    void on_connection_open(proton::connection& c) override {
        // Create the has_messages callback for use with queue subscriptions.
        //
        // Note the captured and bound arguments must be thread-safe to copy,
        // shared_ptr<work_queue>, and plain pointers this and q are all safe.
        //
        // The proton::connection object c is not thread-safe to copy.
        // However when the work_queue calls this->has_messages it will be safe
        // to use any proton objects associated with c again.
        auto work = proton::work_queue::get(c);
        has_messages_callback_ = [this, work](queue* q) {
            work->push(std::bind(&broker_connection_handler::has_messages, this, q));
        };
        c.open();               // Always accept
    }

    // A sender sends messages from a queue to a subscriber.
    void on_sender_open(proton::sender &sender) override {
        queue *q = sender.source().dynamic() ?
            queues_.dynamic() : queues_.get(sender.source().address());
        std::cout << "sending from " << q->name() << std::endl;
    }

    // We have credit to send a message.
    void on_sendable(proton::sender &s) override {
        queue* q = sender_queue(s);
        if (!do_send(q, s))     // Queue is empty, save ourselves in the blocked set.
            blocked_.insert(std::make_pair(q, s));
    }

    // A receiver receives messages from a publisher to a queue.
    void on_receiver_open(proton::receiver &receiver) override {
        std::string qname = receiver.target().address();
        if (qname == "shutdown") {
            std::cout << "broker shutting down" << std::endl;
            // Sending to the special "shutdown" queue stops the broker.
            proton::controller::get(receiver.connection()).stop(
                proton::error_condition("shutdown", "stop broker"));
        } else {
            std::cout << "receiving to " << qname << std::endl;
        }
    }

    // A message is received.
    void on_message(proton::delivery &d, proton::message &m) override {
        std::string qname = d.receiver().target().address();
        queues_.get(qname)->push(m);
    }

    void on_session_close(proton::session &session) override {
        // Erase all blocked senders that belong to session.
        auto predicate = [session](const proton::sender& s) {
            return s.session() == session;
        };
        erase_sender_if(blocked_.begin(), blocked_.end(), predicate);
    }

    void on_sender_close(proton::sender &sender) override {
        // Erase sender from the blocked set.
        auto range = blocked_.equal_range(sender_queue(sender));
        auto predicate = [sender](const proton::sender& s) { return s == sender; };
        erase_sender_if(range.first, range.second, predicate);
    }

    // The controller calls on_transport_close() last.
    void on_transport_close(proton::transport&) override {
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

    // Called via @ref work_queue when q has messages. Try all the blocked senders.
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
    broker(const std::string addr) : controller_(proton::controller::create()) {
        controller_->options(proton::connection_options().container_id("mt_broker"));
        std::cout << "broker listening on " << addr << std::endl;
        controller_->listen(addr, std::bind(&broker::new_handler, this));
    }

    void run() {
        for(size_t i = 0; i < std::thread::hardware_concurrency(); ++i)
            std::thread(&proton::controller::run, controller_.get()).detach();
        controller_->wait();
    }

  private:
    proton::handler* new_handler() {
        return new broker_connection_handler(queues_);
    }

    queues queues_;
    std::unique_ptr<proton::controller> controller_;
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
