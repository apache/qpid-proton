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
#include <proton/delivery.hpp>
#include <proton/message_id.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver_options.hpp>
#include <proton/source.hpp>
#include <proton/types.hpp>

#include <iostream>
#include <map>
#include <string>

#include <atomic>
#include <chrono>
#include <thread>

class tx_recv : public proton::messaging_handler {
  private:
    proton::receiver receiver;
    std::string conn_url_;
    std::string addr_;
    int total;
    int batch_size;
    int received = 0;
    int current_batch = 0;
    int batch_index = 0;

  public:
    tx_recv(const std::string& u, const std::string &a, int c, int b):
        conn_url_(u), addr_(a), total(c), batch_size(b) {}

    void on_container_start(proton::container &c) override {
        c.connect(conn_url_);
    }

   void on_connection_open(proton::connection& c) override {
        // NOTE:credit_window(0) disables automatic flow control.
        // We will use flow control to receive batches of messages in a transaction.
        std::cout << "In this example we abort/commit transaction alternatively." << std::endl;
        receiver = c.open_receiver(addr_, proton::receiver_options().credit_window(0));
    }

    void on_session_open(proton::session &s) override {
        std::cout << "New session is open" << std::endl;
        s.transaction_declare();
    }

    void on_session_error(proton::session &s) override {
        std::cout << "Session error: " << s.error().what() << std::endl;
        s.connection().close();
        exit(-1);
    }

    void on_session_transaction_declared(proton::session &s) override {
        std::cout << "Transaction is declared: " << s.transaction_id() << std::endl;
        receiver.add_credit(batch_size);
    }

    void on_session_transaction_committed(proton::session &s) override {
        std::cout << "Transaction commited" << std::endl;
        received += current_batch;
        current_batch = 0;
        if (received == total) {
            std::cout << "All received messages committed, closing connection." << std::endl;
            s.connection().close();
        }
        else {
            std::cout << "Re-declaring transaction now... to receive next batch." << std::endl;
            s.transaction_declare();
        }
    }

    void on_session_transaction_aborted(proton::session &s) override {
        std::cout << "Transaction aborted!" << std::endl;
        std::cout << "Re-declaring transaction now..." << std::endl;
        current_batch = 0;
        s.transaction_declare();
    }

    void on_message(proton::delivery &d, proton::message &msg) override {
        std::cout<<"# MESSAGE: " << msg.id() <<": "  << msg.body() << std::endl;
        auto session = d.session();
        d.accept();
        current_batch += 1;
        if (current_batch == batch_size) {
            // Batch complete
            if (batch_index % 2 == 1) {
                std::cout << "Commiting transaction..." << std::endl;
                session.transaction_commit();
            } else {
                std::cout << "Aborting transaction..." << std::endl;
                session.transaction_abort();
            }
            batch_index++;
        }
    }
};

int main(int argc, char **argv) {
    std::string conn_url = argc > 1 ? argv[1] : "//127.0.0.1:5672";
    std::string addr = argc > 2 ? argv[2] : "examples";
    int message_count = 6;
    int batch_size = 3;
    example::options opts(argc, argv);

    opts.add_value(conn_url, 'u', "url", "connect and send to URL", "URL");
    opts.add_value(addr, 'a', "address", "connect and send to address", "URL");
    opts.add_value(message_count, 'm', "messages", "number of messages to send", "COUNT");
    opts.add_value(batch_size, 'b', "batch_size", "number of messages in each transaction", "BATCH_SIZE");

    try {
        opts.parse();

        tx_recv recv(conn_url, addr, message_count, batch_size);
        proton::container(recv).run();

        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
