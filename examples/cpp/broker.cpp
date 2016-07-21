/*
 *
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
 *
 */

#include "options.hpp"

#include <proton/connection.hpp>
#include <proton/default_container.hpp>
#include <proton/delivery.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/message.hpp>
#include <proton/receiver_options.hpp>
#include <proton/sender.hpp>
#include <proton/sender_options.hpp>
#include <proton/source_options.hpp>
#include <proton/target_options.hpp>
#include <proton/transport.hpp>
#include <proton/url.hpp>

#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <string>

#include "fake_cpp11.hpp"

/// A simple implementation of a queue.
class queue {
  public:
    queue(const std::string &name, bool dynamic = false) : name_(name), dynamic_(dynamic) {}

    std::string name() const { return name_; }

    void subscribe(proton::sender s) {
        consumers_.push_back(s);
    }

    // Return true if queue can be deleted.
    bool unsubscribe(proton::sender s) {
        consumers_.remove(s);
        return (consumers_.size() == 0 && (dynamic_ || messages_.size() == 0));
    }

    void publish(const proton::message &m) {
        messages_.push_back(m);
        dispatch(0);
    }

    void dispatch(proton::sender *s) {
        while (deliver_to(s)) {}
    }

    bool deliver_to(proton::sender *s) {
        // Deliver to single sender if supplied, else all consumers
        int count = s ? 1 : consumers_.size();

        if (!count) return false;

        bool result = false;
        sender_list::iterator it = consumers_.begin();

        if (!s && count) {
            s = &*it;
        }

        while (messages_.size()) {
            if (s->credit()) {
                const proton::message& m = messages_.front();

                s->send(m);
                messages_.pop_front();
                result = true;
            }

            if (--count) {
                it++;
            } else {
                return result;
            }
        }

        return false;
    }

  private:
    typedef std::deque<proton::message> message_queue;
    typedef std::list<proton::sender> sender_list;

    std::string name_;
    bool dynamic_;
    message_queue messages_;
    sender_list consumers_;
};

/// A collection of queues and queue factory, used by a broker.
class queues {
  public:
    queues() : next_id_(0) {}
    virtual ~queues() {}

    // Get or create a queue.
    virtual queue &get(const std::string &address) {
        if (address.empty()) {
            throw std::runtime_error("empty queue name");
        }

        queue*& q = queues_[address];

        if (!q) q = new queue(address);

        return *q;
    }

    // Create a dynamic queue with a unique name.
    virtual queue &dynamic() {
        std::ostringstream os;
        os << "q" << next_id_++;
        queue *q = queues_[os.str()] = new queue(os.str(), true);

        return *q;
    }

    // Delete the named queue
    virtual void erase(std::string &name) {
        delete queues_[name];
        queues_.erase(name);
    }

  protected:
    typedef std::map<std::string, queue *> queue_map;
    queue_map queues_;
    uint64_t next_id_; // Use to generate unique queue IDs.
};

// A handler to implement broker logic
class broker_handler : public proton::messaging_handler {
  public:
    broker_handler(queues& qs) : queues_(qs) {}

    void on_sender_open(proton::sender &sender) OVERRIDE {
        proton::source src(sender.source());
        queue *q;
        if (src.dynamic()) {
            q = &queues_.dynamic();
        } else if (!src.address().empty()) {
            q = &queues_.get(src.address());
        } else {
            sender.close(proton::error_condition("No queue address supplied"));
            return;
        }
        sender.open(proton::sender_options().source(proton::source_options().address(q->name())));
        q->subscribe(sender);
        std::cout << "broker outgoing link from " << q->name() << std::endl;
    }

    void on_receiver_open(proton::receiver &receiver) OVERRIDE {
        std::string address = receiver.target().address();
        if (!address.empty()) {
            receiver.open(proton::receiver_options().target(proton::target_options().address(address)));
            std::cout << "broker incoming link to " << address << std::endl;
        } else {
            receiver.close(proton::error_condition("No queue address supplied"));
        }
    }

    void unsubscribe(proton::sender lnk) {
        std::string address = lnk.source().address();

        if (queues_.get(address).unsubscribe(lnk)) {
            queues_.erase(address);
        }
    }

    void on_sender_close(proton::sender &sender) OVERRIDE {
        unsubscribe(sender);
    }

    void on_connection_close(proton::connection &c) OVERRIDE {
        remove_stale_consumers(c);
    }

    void on_transport_close(proton::transport &t) OVERRIDE {
        remove_stale_consumers(t.connection());
    }

    void on_transport_error(proton::transport &t) OVERRIDE {
        std::cout << "broker client disconnect: " << t.error().what() << std::endl;
    }

    void on_error(const proton::error_condition &c) OVERRIDE {
        std::cerr << "broker error: " << c.what() << std::endl;
    }

    void remove_stale_consumers(proton::connection connection) {
        proton::sender_range r = connection.senders();
        for (proton::sender_iterator i = r.begin(); i != r.end(); ++i) {
            if (i->active())
                unsubscribe(*i);
        }
    }

    void on_sendable(proton::sender &s) OVERRIDE {
        std::string address = s.source().address();

        queues_.get(address).dispatch(&s);
    }

    void on_message(proton::delivery &d, proton::message &m) OVERRIDE {
        std::string address = d.receiver().target().address();
        queues_.get(address).publish(m);
    }

  protected:
    queues& queues_;
};


// The broker
class broker {
  public:
    broker(const std::string& url) : handler_(url, queues_) {}

    proton::messaging_handler& handler() { return handler_; }

  private:
    class my_handler : public broker_handler {
      public:
        my_handler(const std::string& u, queues& qs) : broker_handler(qs), url_(u) {}

        void on_container_start(proton::container &c) OVERRIDE {
            c.listen(url_);
            std::cout << "broker listening on " << url_ << std::endl;
        }

      private:
        const std::string& url_;
    };

  private:
    queues queues_;
    my_handler handler_;
};

int main(int argc, char **argv) {
    std::string url("0.0.0.0");
    example::options opts(argc, argv);

    opts.add_value(url, 'a', "address", "listen on URL", "URL");

    try {
        opts.parse();

        broker b(url);
        proton::default_container(b.handler()).run();

        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
