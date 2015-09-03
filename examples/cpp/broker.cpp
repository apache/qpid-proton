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

#include "proton/acceptor.hpp"
#include "proton/container.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/url.hpp"

#include <iostream>
#include <sstream>
#include <deque>
#include <map>
#include <list>
#include <string>

class queue {
  public:
    bool dynamic;
    typedef std::deque<proton::message_value> message_queue;
    typedef std::list<proton::counted_ptr<proton::sender> > sender_list;
    message_queue messages;
    sender_list consumers;

    queue(bool dyn = false) : dynamic(dyn) {}

    void subscribe(proton::sender &c) {
        consumers.push_back(c);
    }

    bool unsubscribe(proton::sender &c) {
        consumers.remove(c);
        return (consumers.size() == 0 && (dynamic || messages.size() == 0));
    }

    void publish(const proton::message_value &m) {
        messages.push_back(m);
        dispatch(0);
    }

    void dispatch(proton::sender *s) {
        while (deliver_to(s)) {}
    }

    bool deliver_to(proton::sender *consumer) {
        // deliver to single consumer if supplied, else all consumers
        int count = consumer ? 1 : consumers.size();
        if (!count) return false;
        bool result = false;
        sender_list::iterator it = consumers.begin();
        if (!consumer && count)
            consumer = it->get();

        while (messages.size()) {
            if (consumer->credit()) {
                consumer->send(messages.front());
                messages.pop_front();
                result = true;
            }
            if (--count)
                it++;
            else
                return result;
        }
        return false;
    }
};

class broker : public proton::messaging_handler {
  private:
    typedef std::map<std::string, queue *> queue_map;
    proton::url url;
    queue_map queues;
    uint64_t queue_count;       // Use to generate unique queue IDs.

  public:

    broker(const proton::url &u) : url(u), queue_count(0) {}

    void on_start(proton::event &e) {
        e.container().listen(url);
        std::cout << "broker listening on " << url << std::endl;
    }

    class queue &get_queue(std::string &address) {
        queue_map::iterator it = queues.find(address);
        if (it == queues.end()) {
            queues[address] = new queue();
            return *queues[address];
        }
        else {
            return *it->second;
        }
    }

    std::string queue_name() {
        std::ostringstream os;
        os << "q" << queue_count++;
        return os.str();
    }

    void on_link_opening(proton::event &e) {
        proton::link& lnk = e.link();
        if (lnk.is_sender()) {
            proton::sender &sender(lnk.sender());
            proton::terminus &remote_source(lnk.remote_source());
            if (remote_source.is_dynamic()) {
                std::string address = queue_name();
                lnk.source().address(address);
                queue *q = new queue(true);
                queues[address] = q;
                q->subscribe(sender);
                std::cout << "broker dynamic outgoing link from " << address << std::endl;
            }
            else {
                std::string address = remote_source.address();
                if (!address.empty()) {
                    lnk.source().address(address);
                    get_queue(address).subscribe(sender);
                    std::cout << "broker outgoing link from " << address << std::endl;
                }
            }
        }
        else {
            std::string address = lnk.remote_target().address();
            if (!address.empty())
                lnk.target().address(address);
            std::cout << "broker incoming link to " << address << std::endl;
        }
    }

    void unsubscribe (proton::sender &lnk) {
        std::string address = lnk.source().address();
        queue_map::iterator it = queues.find(address);
        if (it != queues.end() && it->second->unsubscribe(lnk)) {
            delete it->second;
            queues.erase(it);
        }
    }

    void on_link_closing(proton::event &e) {
        proton::link &lnk = e.link();
        if (lnk.is_sender()) {
            unsubscribe(lnk.sender());
        }
    }

    void on_connection_closing(proton::event &e) {
        remove_stale_consumers(e.connection());
    }

    void on_disconnected(proton::event &e) {
        remove_stale_consumers(e.connection());
    }

    void remove_stale_consumers(proton::connection &connection) {
        proton::link *l = connection.link_head(proton::endpoint::REMOTE_ACTIVE);
        while (l) {
            if (l->is_sender()) {
                unsubscribe(l->sender());
            }
            l = l->next(proton::endpoint::REMOTE_ACTIVE);
        }
    }

    void on_sendable(proton::event &e) {
        proton::link& lnk = e.link();
        std::string addr = lnk.source().address();
        proton::sender &s(lnk.sender());
        get_queue(addr).dispatch(&s);
    }

    void on_message(proton::event &e) {
        std::string addr = e.link().target().address();
        proton::message_value msg = e.message();
        get_queue(addr).publish(msg);
    }
};

int main(int argc, char **argv) {
    // Command line options
    proton::url url("0.0.0.0");
    options opts(argc, argv);
    opts.add_value(url, 'a', "address", "listen on URL", "URL");
    try {
        opts.parse();
        broker broker(url);
        proton::container(broker).run();
        return 0;
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
