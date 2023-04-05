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

#include "options.hpp"

#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/delivery.hpp>
#include <proton/error_condition.hpp>
#include <proton/listen_handler.hpp>
#include <proton/listener.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver_options.hpp>
#include <proton/sender_options.hpp>
#include <proton/source_options.hpp>
#include <proton/target.hpp>
#include <proton/target_options.hpp>
#include <proton/tracker.hpp>
#include <proton/transport.hpp>
#include <proton/work_queue.hpp>

#include <deque>
#include <iostream>
#include <map>
#include <string>
#include <thread>


// This is a simplified model for a message broker, that only allows for
// messages to go to a single receiver.
//
// This broker is multithread safe and if compiled with C++11 with a multithreaded Proton
// binding library will use as many threads as there are thread resources available (usually
// cores)
//
// Queues are only created and never destroyed
//
// Broker Entities (that need to be individually serialised)
// QueueManager - Creates new queues, finds queues
// Queue        - Queues msgs, records subscribers, sends msgs to subscribers
// Connection   - Receives Messages from network, sends messages to network.

// Work
// FindQueue(queueName, connection) - From a Connection to the QueueManager
//     This will create the queue if it doesn't already exist and send a BoundQueue
//     message back to the connection.
// BoundQueue(queue) - From the QueueManager to a Connection
//
// QueueMsg(msg)        - From a Connection (receiver) to a Queue
// Subscribe(sender)    - From a Connection (sender) to a Queue
// Flow(sender, credit) - From a Connection (sender) to a Queue
// Unsubscribe(sender)  - From a Connection (sender) to a Queue
//
// SendMsg(msg)   - From a Queue to a Connection (sender)
// Unsubscribed() - From a Queue to a Connection (sender)


// Simple debug output
bool verbose;
#define DOUT(x) do {if (verbose) {x};} while (false)

class Queue;
class Sender;

class Sender : public proton::messaging_handler {
    friend class connection_handler;

    proton::sender sender_;
    proton::work_queue& work_queue_;
    std::string queue_name_;
    Queue* queue_;
    int pending_credit_;

    // Messaging handlers
    void on_sendable(proton::sender &sender) override;
    void on_sender_close(proton::sender &sender) override;

public:
    Sender(proton::sender s) :
        sender_(s), work_queue_(s.work_queue()), queue_(0), pending_credit_(0)
    {
        s.user_data(this);
    }

    bool add(proton::work f) {
        return work_queue_.add(f);
    }

    static Sender* get(const proton::sender& s) {
        return reinterpret_cast<Sender*>(s.user_data());
    }
    void boundQueue(Queue* q, std::string qn);
    void sendMsg(proton::message m) {
        DOUT(std::cerr << "Sender:   " << this << " sending\n";);
        sender_.send(m);
    }
    void unsubscribed() {
        DOUT(std::cerr << "Sender:   " << this << " deleting\n";);
        sender_.user_data(nullptr);
        sender_.close();
        delete this;
    }
};

// Queue - round robin subscriptions
class Queue {
    proton::work_queue work_queue_;
    const std::string name_;
    std::deque<proton::message> messages_;
    typedef std::map<Sender*, int> subscriptions; // With credit
    subscriptions subscriptions_;
    subscriptions::iterator current_;

    void tryToSend() {
        DOUT(std::cerr << "Queue:    " << this << " tryToSend: " << subscriptions_.size(););
        // Starting at current_, send messages to subscriptions with credit:
        // After each send try to find another subscription; Wrap around;
        // Finish when we run out of messages or credit.
        size_t outOfCredit = 0;
        while (!messages_.empty() && outOfCredit<subscriptions_.size()) {
            // If we got the end (or haven't started yet) start at the beginning
            if (current_==subscriptions_.end()) {
                current_=subscriptions_.begin();
            }
            // If we have credit send the message
            DOUT(std::cerr << "(" << current_->second << ") ";);
            if (current_->second>0) {
                DOUT(std::cerr << current_->first << " ";);
                auto msg = messages_.front();
                auto sender = current_->first;
                sender->add([=]{sender->sendMsg(msg);});
                messages_.pop_front();
                --current_->second;
                ++current_;
            } else {
                ++outOfCredit;
            }
        }
        DOUT(std::cerr << "\n";);
    }

public:
    Queue(proton::container& c, const std::string& n) :
        work_queue_(c), name_(n), current_(subscriptions_.end())
    {}

    bool add(proton::work f) {
        return work_queue_.add(f);
    }

    void queueMsg(proton::message m) {
        DOUT(std::cerr << "Queue:    " << this << "(" << name_ << ") queueMsg\n";);
        messages_.push_back(m);
        tryToSend();
    }
    void flow(Sender* s, int c) {
        DOUT(std::cerr << "Queue:    " << this << "(" << name_ << ") flow: " << c << " to " << s << "\n";);
        subscriptions_[s] = c;
        tryToSend();
    }
    void subscribe(Sender* s) {
        DOUT(std::cerr << "Queue:    " << this << "(" << name_ << ") subscribe Sender: " << s << "\n";);
        subscriptions_[s] = 0;
    }
    void unsubscribe(Sender* s) {
        DOUT(std::cerr << "Queue:    " << this << "(" << name_ << ") unsubscribe Sender: " << s << "\n";);
        // If we're about to erase the current subscription move on
        if (current_ != subscriptions_.end() && current_->first==s) ++current_;
        subscriptions_.erase(s);
        s->add([=]{s->unsubscribed();});
    }
};

// We have credit to send a message.
void Sender::on_sendable(proton::sender &sender) {
    if (queue_) {
        auto credit = sender.credit();
        queue_->add([=]{queue_->flow(this, credit);});
    } else {
        pending_credit_ = sender.credit();
    }
}

void Sender::on_sender_close(proton::sender &sender) {
    if (queue_) {
        queue_->add([=]{queue_->unsubscribe(this);});
    } else {
        // TODO: Is it possible to be closed before we get the queue allocated?
        // If so, we should have a way to mark the sender deleted, so we can delete
        // on queue binding
    }
}

void Sender::boundQueue(Queue* q, std::string qn) {
    DOUT(std::cerr << "Sender:   " << this << " bound to Queue: " << q <<"(" << qn << ")\n";);
    queue_ = q;
    queue_name_ = qn;

    sender_.open(proton::sender_options()
        .source((proton::source_options().address(queue_name_)))
        .handler(*this));
    auto credit = pending_credit_;
    q->add([=]{
        q->subscribe(this);
        if (credit>0) {
            q->flow(this, credit);
        }
    });
    std::cout << "sending from " << queue_name_ << std::endl;
}

class QueueManager;

class Receiver : public proton::messaging_handler {
    friend class connection_handler;

    proton::receiver receiver_;
    proton::work_queue& work_queue_;
    Queue* queue_;
    QueueManager& queue_manager_;
    std::deque<proton::message> messages_;

    // A message is received.
    void on_message(proton::delivery &d, proton::message &m) override {
        // We allow anonymous relay behaviour always even if not requested
        auto to_address = m.to();
        if (queue_) {
            messages_.push_back(m);
            queueMsgs();
        } else if (!to_address.empty()) {
            queueMsgToNamedQueue(m, to_address);
        } else {
            // No bound link queue, no message 'to address' - reject message
            d.reject();
        }
    }

    void queueMsgs() {
        DOUT(std::cerr << "Receiver: " << this << " queueing " << messages_.size() << " msgs to: " << queue_ << "\n";);
        while (!messages_.empty()) {
            auto msg = messages_.front();
            queue_->add([=]{queue_->queueMsg(msg);});
            messages_.pop_front();
        }
    }

    void queueMsgToNamedQueue(proton::message& m, std::string address);

public:
    Receiver(proton::receiver r, QueueManager& qm) :
        receiver_(r), work_queue_(r.work_queue()), queue_(0), queue_manager_(qm)
    {}

    bool add(proton::work f) {
        return work_queue_.add(f);
    }

    void boundQueue(Queue* q, std::string qn) {
        DOUT(std::cerr << "Receiver: " << this << " bound to Queue: " << q << "(" << qn << ")\n";);
        queue_ = q;
        receiver_.open(proton::receiver_options()
            .source((proton::source_options().address(qn)))
            .handler(*this));
        std::cout << "receiving to " << qn << std::endl;

        queueMsgs();
    }
};

class QueueManager {
    proton::container& container_;
    proton::work_queue work_queue_;
    typedef std::map<std::string, Queue*> queues;
    queues queues_;
    int next_id_; // Use to generate unique queue IDs.

public:
    QueueManager(proton::container& c) :
        container_(c), work_queue_(c), next_id_(0)
    {}

    bool add(proton::work f) {
        return work_queue_.add(f);
    }

    template <class T>
    void findQueue(T& connection, std::string& qn) {
        if (qn.empty()) {
            // Dynamic queue creation
            std::ostringstream os;
            os << "_dynamic_" << next_id_++;
            qn = os.str();
        }
        Queue* q = 0;
        auto i = queues_.find(qn);
        if (i==queues_.end()) {
            q = new Queue(container_, qn);
            queues_[qn] = q;
        } else {
            q = i->second;
        }
        connection.add([=, &connection] {connection.boundQueue(q, qn);});
    }

    void queueMessage(proton::message m, std::string address) {
        Queue* q = 0;
        auto i = queues_.find(address);
        if (i==queues_.end()) {
            q = new Queue(container_, address);
            queues_[address] = q;
        } else {
            q = i->second;
        }
        q->add([=] {q->queueMsg(m);});
    }

    void findQueueSender(Sender* s, std::string qn) {
        findQueue(*s, qn);
    }

    void findQueueReceiver(Receiver* r, std::string qn) {
        findQueue(*r, qn);
    }
};

void Receiver::queueMsgToNamedQueue(proton::message& m, std::string address) {
    DOUT(std::cerr << "Receiver: " << this << " send msg to Queue: " << address << "\n";);
    queue_manager_.add([=]{queue_manager_.queueMessage(m, address);});
}

class connection_handler : public proton::messaging_handler {
    QueueManager& queue_manager_;

public:
    connection_handler(QueueManager& qm) :
        queue_manager_(qm)
    {}

    void on_connection_open(proton::connection& c) override {
        // Don't check whether the peer desires ANONYMOUS-RELAY: offer it anyway.
        // Accept the connection
        c.open(proton::connection_options{}
            .offered_capabilities({"ANONYMOUS-RELAY"}));
    }

    // A sender sends messages from a queue to a subscriber.
    void on_sender_open(proton::sender &sender) override {
        std::string qn = sender.source().dynamic() ? "" : sender.source().address();
        Sender* s = new Sender(sender);
        queue_manager_.add([=]{queue_manager_.findQueueSender(s, qn);});
    }

    // A receiver receives messages from a publisher to a queue.
    void on_receiver_open(proton::receiver &receiver) override {
        std::string qname = receiver.target().address();
        Receiver* r = new Receiver(receiver, queue_manager_);
        // Allow anonymous relay always
        if (qname.empty()) {
            receiver.open(proton::receiver_options{}
                .handler(*r));
        } else {
           queue_manager_.add([=]{queue_manager_.findQueueReceiver(r, qname);});
        }
    }

    void on_session_close(proton::session &session) override {
        // Unsubscribe all senders that belong to session.
        for (proton::sender_iterator i = session.senders().begin(); i != session.senders().end(); ++i) {
            Sender* s = Sender::get(*i);
            if (s && s->queue_) {
                auto q = s->queue_;
                s->queue_->add([=]{q->unsubscribe(s);});
            }
        }
    }

    void on_error(const proton::error_condition& e) override {
        std::cout << "protocol error: " << e.what() << std::endl;
    }

    // The container calls on_transport_close() last.
    void on_transport_close(proton::transport& t) override {
        // Unsubscribe all senders.
        for (proton::sender_iterator i = t.connection().senders().begin(); i != t.connection().senders().end(); ++i) {
            Sender* s = Sender::get(*i);
            if (s && s->queue_) {
                auto q = s->queue_;
                s->queue_->add([=]{q->unsubscribe(s);});
            }
        }
        delete this;            // All done.
    }
};

class broker {
  public:
    broker(const std::string addr) :
        container_("broker"), queues_(container_), listener_(queues_)
    {
        container_.listen(addr, listener_);
    }

    void run() {
        container_.run(std::thread::hardware_concurrency());
    }

  private:
    struct listener : public proton::listen_handler {
        listener(QueueManager& c) : queues_(c) {}

        proton::connection_options on_accept(proton::listener&) override{
            return proton::connection_options().handler(*(new connection_handler(queues_)));
        }

        void on_open(proton::listener& l) override {
            std::cout << "broker listening on " << l.port() << std::endl;
        }

        void on_error(proton::listener&, const std::string& s) override {
            std::cerr << "listen error: " << s << std::endl;
            throw std::runtime_error(s);
        }
        QueueManager& queues_;
    };

    proton::container container_;
    QueueManager queues_;
    listener listener_;
};

int main(int argc, char **argv) {
    // Command line options
    std::string address("0.0.0.0");
    example::options opts(argc, argv);

    opts.add_flag(verbose, 'v', "verbose", "verbose (debugging) output");
    opts.add_value(address, 'a', "address", "listen on URL", "URL");

    try {
        verbose = false;
        opts.parse();
        broker(address).run();
        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "broker shutdown: " << e.what() << std::endl;
    }
    return 1;
}
