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
#include <proton/sender_options.hpp>
#include <proton/types.hpp>

#include <iostream>
#include <map>
#include <string>

#include <atomic>
#include <chrono>
#include <thread>

class tx_send : public proton::messaging_handler {
  private:
    proton::sender sender;
    std::string conn_url_;
    std::string addr_;
    int total;
    int batch_size;
    int sent;
    int batch_index = 0;
    int current_batch = 0;
    int committed = 0;
    std::atomic<int> unique_msg_id;

  public:
    tx_send(const std::string& u, const std::string& a, int c, int b):
        conn_url_(u), addr_(a), total(c), batch_size(b), sent(0), unique_msg_id(10000) {}

    void on_container_start(proton::container &c) override {
        c.connect(conn_url_);
    }

    void on_connection_open(proton::connection& c) override {
        std::cout << "In this example we abort/commit transaction alternatively." << std::endl;
        sender = c.open_sender(addr_);
    }

    void on_session_open(proton::session& s) override {
        std::cout << "New session is open, declaring transaction now..." << std::endl;
        s.transaction_declare();
    }

    void on_session_transaction_declared(proton::session& s) override {
        std::cout << "Transaction is declared: " << s.transaction_id() << std::endl;
        send();
    }

    void on_session_error(proton::session &s) override {
        std::cout << "Session error: " << s.error().what() << std::endl;
        s.connection().close();
        exit(-1);
    }

    void on_session_transaction_error(proton::session &s) override {
        std::cout << "Transaction error!" << std::endl;
        s.connection().close();
        exit(-1);
    }

    void on_sendable(proton::sender&) override {
        send();
    }

    void send() {
        proton::session session = sender.session();
        while (session.transaction_is_declared() && sender.credit() &&
               (committed + current_batch) < total) {
            proton::message msg;
            std::map<std::string, int> m;
            m["sequence"] = committed + current_batch;

            msg.id(std::atomic_fetch_add(&unique_msg_id, 1));
            msg.body(m);
            std::cout << "Sending [sender batch " << batch_index << "]: " << msg << std::endl;
            sender.send(msg);
            current_batch += 1;
            if(current_batch == batch_size)
            {
                if (batch_index % 2 == 0) {
                    std::cout << "Commiting transaction..." << std::endl;
                    session.transaction_commit();
                } else {
                    std::cout << "Aborting transaction..." << std::endl;
                    session.transaction_abort();
                }
                batch_index++;
            }
        }
    }

    void on_session_transaction_committed(proton::session &s) override {
        committed += current_batch;
        current_batch = 0;
        std::cout << "Transaction commited" << std::endl;
        if(committed == total) {
            std::cout << "All messages committed, closing connection." << std::endl;
            s.connection().close();
        }
        else {
            std::cout << "Re-declaring transaction now..." << std::endl;
            s.transaction_declare();
        }
    }

    void on_session_transaction_aborted(proton::session &s) override {
        std::cout << "Transaction aborted!" << std::endl;
        std::cout << "Re-delaring transaction now..." << std::endl;
        current_batch = 0;
        s.transaction_declare();
    }

    void on_sender_close(proton::sender &s) override {
        current_batch = 0;
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

        tx_send send(conn_url, addr, message_count, batch_size);
        proton::container(send).run();

        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
