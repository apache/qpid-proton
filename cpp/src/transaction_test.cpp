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
#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/listen_handler.hpp>
#include <proton/container.hpp>
#include <proton/delivery.hpp>
#include "proton/codec/decoder.hpp"
#include "proton/codec/encoder.hpp"
#include <proton/listener.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver.hpp>
#include <proton/sender.hpp>
#include <proton/session.hpp>
#include <proton/types.hpp>
#include "proton_bits.hpp"
#include <proton/target.hpp>
#include <proton/tracker.hpp>
#include <proton/value.hpp>
#include <proton/codec/decoder.hpp>
#include <proton/delivery.h>  // C++ API doesn't export disposition
#include "test_bits.hpp" // For RUN_ARGV_TEST and ASSERT_EQUAL
#include "types_internal.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace {
std::mutex m;
std::condition_variable cv;
bool listener_ready = false;
int listener_port;
const proton::binary fake_txn_id("prqs5678-abcd-efgh-1a2b-3c4d5e6f7g8e");
} // namespace

void wait_for_promise_or_fail(std::promise<void>& prom, const std::string& what) {
   if (prom.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
       FAIL("Test FAILED: Did not receive '" << what << "' in time.");
   }
}

class FakeBroker : public proton::messaging_handler {
 private:
   class listener_ready_handler : public proton::listen_handler {
       void on_open(proton::listener& l) override {
           std::lock_guard<std::mutex> lk(m);
           listener_port = l.port();
           listener_ready = true;
           cv.notify_one();
       }
   };
   std::string url;
   listener_ready_handler listen_handler;
   proton::receiver coordinator_link;

 public:
   proton::listener listener;
   std::map<proton::binary, std::vector<proton::message>> transactions_messages;
   std::promise<void> declare_promise;
   std::promise<void> commit_promise;
   std::promise<void> abort_promise;

   FakeBroker(const std::string& s) : url(s) {}

   void on_container_start(proton::container& c) override {
       listener = c.listen(url, listen_handler);
   }

   void on_connection_open(proton::connection& c) override {
       c.open(proton::connection_options{}.offered_capabilities({"ANONYMOUS-RELAY"}));
   }

   void on_receiver_open(proton::receiver& r) override {
       // Identify the transaction link.
       if(r.target().capabilities().size() > 0 &&
          r.target().capabilities()[0] == proton::symbol("amqp:local-transactions")) {
           coordinator_link = r;
       }
       r.open();
   }

   void on_message(proton::delivery& d, proton::message& m) override {
       if (coordinator_link.active() && d.receiver() == coordinator_link) {
           handle_transaction_control(d, m);
       } else {
           handle_application_message(d, m);
       }
   }

   void handle_application_message(proton::delivery& d, proton::message& m) {
       auto disp = pn_transactional_disposition(pn_delivery_remote(unwrap(d)));
       if (disp != NULL) {
           // transactional message
           proton::binary txn_id = proton::bin(pn_transactional_disposition_get_id(disp));
           transactions_messages[txn_id].push_back(m);
       }
   }

   void handle_transaction_control(proton::delivery& d, proton::message& m) {
       proton::codec::decoder dec(m.body());
       proton::symbol descriptor;
       proton::value _value;
       proton::type_id _t = dec.next_type();

       if (_t == proton::type_id::DESCRIBED) {
           proton::codec::start s;
           dec >> s >> descriptor >> _value >>  proton::codec::finish();
       } else {
           std::cerr << "[fake_broker] Invalid transaction control message format: " << to_string(m) << std::endl;
           d.reject();
           return;
       }

       if (descriptor == "amqp:declare:list") {
           pn_bytes_t txn_id = pn_bytes(fake_txn_id.size(), reinterpret_cast<const char*>(&fake_txn_id[0]));

           pn_delivery_t* pd = proton::unwrap(d);
           pn_disposition_t* disp = pn_delivery_local(pd);
           pn_declared_disposition_t* declared_disp = pn_declared_disposition(disp);
           pn_declared_disposition_set_id(declared_disp, txn_id);
           pn_delivery_settle(pd);

           std::cout << "[BROKER] transaction declared: " << fake_txn_id << std::endl;
           declare_promise.set_value();
       } else if (descriptor == "amqp:discharge:list") {
           // Commit / Abort transaction.
           std::vector<proton::value> vd;
           proton::get(_value, vd);
           ASSERT_EQUAL(vd.size(), 2u);
           proton::binary txn_id = vd[0].get<proton::binary>();
           bool is_abort = vd[1].get<bool>();
           if (!is_abort) {
               // Commit
               std::cout << "[BROKER] transaction commited:" << txn_id << std::endl;
               // As part of this test, we don't need to forward transactions_messages.
               // We are leaving the messages here to count them later on.
               commit_promise.set_value();
               d.accept();
           } else {
               // Abort
               std::cout << "[BROKER] transaction aborted:" << txn_id << std::endl;
               transactions_messages.erase(txn_id);
               abort_promise.set_value();
               d.accept();
           }
           // Closing the connection as we are testing till commit/abort.
           d.receiver().close();
           d.connection().close();
           listener.stop();
       }
   }
};

class test_client : public proton::messaging_handler {
 private:
   std::string server_address_;
   int messages_left_;
   bool is_commit_;

 public:
   proton::binary last_txn_id;
   proton::sender sender_;
   std::promise<void> block_declare_transaction_on_session;
   std::promise<void> transaction_finished_promise;

   test_client(const std::string& s) :
       server_address_(s), messages_left_(0) {}

   void on_container_start(proton::container& c) override {
       c.connect(server_address_);
   }

   void on_connection_open(proton::connection& c) override {
       sender_ = c.open_sender("/test");
   }

   void on_session_open(proton::session& s) override {
       if (!s.transaction_is_declared()) {
           wait_for_promise_or_fail(block_declare_transaction_on_session, "waiting on test to be ready");
           s.transaction_declare();
       } else {
           last_txn_id = s.transaction_id();
           std::cout << "Client: Transaction declared successfully: " << last_txn_id << std::endl;
           send();
       }
   }

   void schedule_messages_in_transaction(int count, bool is_commit) {
       messages_left_ = count;
       is_commit_ = is_commit;
   }

   void on_sendable(proton::sender&) override {
       send();
   }

   void send() {
       proton::session session = sender_.session();
       while (session.transaction_is_declared() && sender_.credit() &&
              messages_left_ > 0) {
           proton::message msg("hello");
           sender_.send(msg);
           messages_left_--;
           if (messages_left_ == 0) {
               if (is_commit_) {
                   std::cout << "Client: Committing transaction." << std::endl;
                   session.transaction_commit();
               } else {
                   std::cout << "Client: Aborting transaction." << std::endl;
                   session.transaction_abort();
               }
           }
       }
   }

   void on_session_transaction_committed(proton::session &s) override {
       std::cout << "Client: Transaction committed" << std::endl;
       transaction_finished_promise.set_value();
       s.connection().close();
   }

   void on_session_transaction_aborted(proton::session &s) override {
       std::cout << "Client: Transaction aborted" << std::endl;
       transaction_finished_promise.set_value();
       s.connection().close();
   }
};

void test_transaction_commit(FakeBroker &broker, test_client &client) {
   std::cout << "Starting test_transaction_commit..." << std::endl;

   const unsigned int messages_in_txn = 5;
   client.schedule_messages_in_transaction(messages_in_txn, true);
   client.block_declare_transaction_on_session.set_value();

   wait_for_promise_or_fail(broker.declare_promise, "declare in broker");
   wait_for_promise_or_fail(broker.commit_promise, "commit in broker");

   // Only one transaction
   ASSERT_EQUAL(broker.transactions_messages.size(), 1u);
   // Check message count inside broker
   ASSERT_EQUAL(broker.transactions_messages[fake_txn_id].size(), messages_in_txn);
}

void test_transaction_abort(FakeBroker &broker, test_client &client) {
   std::cout << "Starting test_transaction_abort..." << std::endl;

   const unsigned int messages_in_txn = 5;
   client.schedule_messages_in_transaction(messages_in_txn, false);
   client.block_declare_transaction_on_session.set_value();

   wait_for_promise_or_fail(broker.declare_promise, "declare in broker");
   wait_for_promise_or_fail(broker.commit_promise, "commit in broker");

   // Only zero transactions
   ASSERT_EQUAL(broker.transactions_messages.size(), 0u);
}

int main(int argc, char** argv) {
   int tests_failed = 0;

   std::string broker_address("127.0.0.1:0");
   FakeBroker broker(broker_address);

   proton::container broker_container(broker);
   std::thread broker_thread([&broker_container]() -> void { broker_container.run(); });

   // Wait for the listener
   std::unique_lock<std::mutex> lk(m);
   cv.wait(lk, [] { return listener_ready; });


   std::string server_address = "127.0.0.1:" + std::to_string(listener_port);
   test_client client(server_address);

   proton::container client_container(client);
   std::thread client_thread([&client_container]() -> void { client_container.run(); });

   RUN_ARGV_TEST(tests_failed, test_transaction_commit(broker, client));

   broker_thread.join();
   client_thread.join();

   return tests_failed;
}
