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

#include "proton/cpp/Container.h"
#include "proton/cpp/MessagingHandler.h"

#include <iostream>
#include <deque>
#include <map>
#include <list>
#include <stdlib.h>
#include <string.h>

using namespace proton::reactor;

std::string generateUuid(){
    throw "TODO: platform neutral uuid";
}

class Queue {
  public:
    bool dynamic;
    typedef std::deque<Message> MsgQ;
    typedef std::list<Sender> List;
    MsgQ queue;
    List consumers;

    Queue(bool dyn = false) : dynamic(dyn), queue(MsgQ()), consumers(List()) {}

    void subscribe(Sender &c) {
        consumers.push_back(c);
    }

    bool unsubscribe(Sender &c) {
        consumers.remove(c);
        return (consumers.size() == 0 && (dynamic || queue.size() == 0));
    }

    void publish(Message &m) {
        queue.push_back(m);
        dispatch(0);
    }

    void dispatch(Sender *s) {
        while (deliverTo(s)) {
        }
    }

    bool deliverTo(Sender *consumer) {
        // deliver to single consumer if supplied, else all consumers
        int count = consumer ? 1 : consumers.size();
        if (!count) return false;
        bool result = false;
        List::iterator it = consumers.begin();
        if (!consumer && count) consumer = &*it;

        while (queue.size()) {
            if (consumer->getCredit()) {
                consumer->send(queue.front());
                queue.pop_front();
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

class Broker : public MessagingHandler {
  private:
    std::string url;
    typedef std::map<std::string, Queue *> QMap;
    QMap queues;
  public:

    Broker(const std::string &s) : url(s), queues(QMap()) {}

    void onStart(Event &e) {
        e.getContainer().listen(url);
        std::cout << "broker listening on " << url << std::endl;
    }

    Queue &queue(std::string &address) {
        QMap::iterator it = queues.find(address);
        if (it == queues.end()) {
            queues[address] = new Queue();
            return *queues[address];
        }
        else {
            return *it->second;
        }
    }

    void onLinkOpening(Event &e) {
        Link lnk = e.getLink();
        if (lnk.isSender()) {
            Sender sender(lnk);
            Terminus remoteSource(lnk.getRemoteSource());
            if (remoteSource.isDynamic()) {
                std::string address = generateUuid();
                lnk.getSource().setAddress(address);
                Queue *q = new Queue(true);
                queues[address] = q;
                q->subscribe(sender);
            }
            else {
                std::string address = remoteSource.getAddress();
                if (!address.empty()) {
                    lnk.getSource().setAddress(address);
                    queue(address).subscribe(sender);
                }
            }
        }
        else {
            std::string address = lnk.getRemoteTarget().getAddress();
            if (!address.empty())
                lnk.getTarget().setAddress(address);
        }
    }

    void unsubscribe (Sender &lnk) {
        std::string address = lnk.getSource().getAddress();
        QMap::iterator it = queues.find(address);
        if (it != queues.end() && it->second->unsubscribe(lnk)) {
            delete it->second;
            queues.erase(it);
        }
    }

    void onLinkClosing(Event &e) {
        Link lnk = e.getLink();
        if (lnk.isSender()) {
            Sender s(lnk);
            unsubscribe(s);
        }
    }

    void onConnectionClosing(Event &e) {
        removeStaleConsumers(e.getConnection());
    }

    void onDisconnected(Event &e) {
        removeStaleConsumers(e.getConnection());
    }

    void removeStaleConsumers(Connection &connection) {
        Link l = connection.getLinkHead(Endpoint::REMOTE_ACTIVE);
        while (l) {
            if (l.isSender()) {
                Sender s(l);
                unsubscribe(s);
            }
            l = l.getNext(Endpoint::REMOTE_ACTIVE);
        }
    }

    void onSendable(Event &e) {
        Link lnk = e.getLink();
        Sender sender(lnk);
        std::string addr = lnk.getSource().getAddress();
        queue(addr).dispatch(&sender);
    }

    void onMessage(Event &e) {
        std::string addr = e.getLink().getTarget().getAddress();
        Message msg = e.getMessage();
        queue(addr).publish(msg);
    }
};

int main(int argc, char **argv) {
    std::string url = argc > 1 ? argv[1] : ":5672";
    Broker broker(url);
    try {
        Container(broker).run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
