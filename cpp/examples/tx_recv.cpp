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
#include <proton/container.hpp>
#include <proton/message.hpp>
#include <proton/message_id.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/types.hpp>
#include <proton/transaction.hpp>

#include <iostream>
#include <map>
#include <string>

#include <chrono>
#include <thread>

class tx_recv : public proton::messaging_handler, proton::transaction_handler {
  private:
    proton::receiver receiver; 
    std::string url;
    int expected;
    int batch_size;
    int current_batch = 0;
    int committed = 0;

    proton::session session;
    // proton::transaction transaction;
  public:
    tx_recv(const std::string &s, int c, int b):
        url(s), expected(c), batch_size(b) {}

    void on_container_start(proton::container &c) override {
        receiver = c.open_receiver(url);
    }

    void on_session_open(proton::session &s) override {
        session = s;
        std::cout << "    [on_session_open] declare_txn started..." << std::endl;
        s.declare_transaction(*this);
        std::cout << "    [on_session_open] declare_txn ended..." << std::endl;
    }

    void on_transaction_declare_failed(proton::session) {}
    void on_transaction_commit_failed(proton::session s) {
        std::cout << "Transaction Commit Failed" << std::endl;
        s.connection().close();
        exit(-1);
    }

    void on_transaction_declared(proton::session s) override {
        std::cout << "[on_transaction_declared] txn called " << (&s)
                  << std::endl;
        // std::cout << "[on_transaction_declared] txn is_empty " << (t.is_empty())
        //           << "\t" << transaction.is_empty() << std::endl;
        receiver.add_credit(batch_size); 
        // transaction = t;
    }

    void on_message(proton::delivery &d, proton::message &msg) override {
        std::cout<<"# MESSAGE: " << msg.id() <<": "  << msg.body() << std::endl;
        session.txn_accept(d);
        current_batch += 1;
        if(current_batch == batch_size) {
            // transaction = proton::transaction(); // null
        }
    }

    void on_transaction_committed(proton::session s) override {
        committed += current_batch;
        current_batch = 0;
        std::cout<<"    [OnTxnCommitted] Committed:"<< committed<< std::endl;
        if(committed == expected) {
            std::cout << "All messages committed" << std::endl;
            s.connection().close();
        }
        else {
            session.declare_transaction(*this);
        }
    }

};

int main(int argc, char **argv) {
    std::string address("127.0.0.1:5672/examples");
    int message_count = 9;
    int batch_size = 3;
    example::options opts(argc, argv);

    opts.add_value(address, 'a', "address", "connect and send to URL", "URL");
    opts.add_value(message_count, 'm', "messages", "number of messages to send", "COUNT");
    opts.add_value(batch_size, 'b', "batch_size", "number of messages in each transaction", "BATCH_SIZE");

    try {
        opts.parse();

        tx_recv recv(address, message_count, batch_size);
        proton::container(recv).run();

        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
