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

class tx_send : public proton::messaging_handler, proton::transaction_handler {
  private:
    proton::sender sender;
    std::string url;
    int total;
    int batch_size;
    int sent;
    int batch_index = 0;
    int current_batch = 0;
    int committed = 0;

  public:
    tx_send(const std::string &s, int c, int b):
        url(s), total(c), batch_size(b), sent(0) {}

    void on_container_start(proton::container &c) override {
        sender = c.open_sender(url);
    }

    void on_session_open(proton::session &s) override {
        std::cout << "New session is open, declaring transaction now..." << std::endl;
        s.declare_transaction(*this);
    }

    void on_transaction_declare_failed(proton::session s) {
        std::cout << "Transaction declarion failed" << std::endl;
        s.connection().close();
        exit(-1);
    }

    void on_transaction_commit_failed(proton::session s) {
        std::cout << "Transaction commit failed!" << std::endl;
        s.connection().close();
        exit(-1);
    }

    void on_transaction_declared(proton::session s) override {
        std::cout << "Transaction is declared" << std::endl;
        send();
    }

    void on_sendable(proton::sender&) override {
        send();
    }

    void send() {
        static int unique_id = 10000;
        proton::session session = sender.session();
        while (session.txn_is_declared() && sender.credit() &&
               (committed + current_batch) < total) {
            proton::message msg;
            std::map<std::string, int> m;
            m["sequence"] = committed + current_batch;

            msg.id(unique_id++);
            msg.body(m);
            std::cout << "Sending: " << msg << std::endl;
            session.txn_send(sender, msg);
            current_batch += 1;
            if(current_batch == batch_size)
            {
                if (batch_index % 2 == 0) {
                    std::cout << "Commiting transaction..." << std::endl;
                    session.txn_commit();
                } else {
                    std::cout << "Aborting transaction..." << std::endl;
                    session.txn_abort();
                }
                batch_index++;
            }
        }
    }

    void on_transaction_committed(proton::session s) override {
        committed += current_batch;
        current_batch = 0;
        std::cout << "Transaction commited" << std::endl;
        if(committed == total) {
            std::cout << "All messages committed, closing connection." << std::endl;
            s.connection().close();
        }
        else {
            std::cout << "Re-declaring transaction now..." << std::endl;
            s.declare_transaction(*this);
        }
    }

    void on_transaction_aborted(proton::session s) override {
        std::cout << "Transaction aborted!" << std::endl;
        std::cout << "Re-delaring transaction now..." << std::endl;
        current_batch = 0;
        s.declare_transaction(*this);
    }

    void on_sender_close(proton::sender &s) override {
        current_batch = 0;
    }

};

int main(int argc, char **argv) {
    std::string address("127.0.0.1:5672/examples");
    int message_count = 6;
    int batch_size = 3;
    example::options opts(argc, argv);

    opts.add_value(address, 'a', "address", "connect and send to URL", "URL");
    opts.add_value(message_count, 'm', "messages", "number of messages to send", "COUNT");
    opts.add_value(batch_size, 'b', "batch_size", "number of messages in each transaction", "BATCH_SIZE");

    try {
        opts.parse();

        tx_send send(address, message_count, batch_size);
        proton::container(send).run();

        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
