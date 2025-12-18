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
#include <proton/target_options.hpp>
#include <proton/types.hpp>

#include <iostream>
#include <map>
#include <string>

class tx_send : public proton::messaging_handler {
  private:
    std::string conn_url_;
    std::string addr_;
    proton::sender sender;
    int total;
    int batch_size;
    int abort_message;
    int accepted = 0;
    int total_accepted = 0;
    int batch_index = 0;
    int current_batch = 0;
    int committed = 0;
    int msg_id = 10000;
    int err_id;

  public:
    tx_send(const std::string& u, const std::string& a, int t, int b, int c, int e):
        conn_url_(u), addr_(a), total(t), batch_size(b), abort_message(c), err_id(e ? msg_id+e : 0) {}

    void on_container_start(proton::container &c) override {
        c.connect(conn_url_);
    }

    void on_connection_open(proton::connection& c) override {
        c.open_session();
    }

    void on_session_open(proton::session& s) override {
        std::cout << "New session is open, declaring transaction now..." << std::endl;
        s.open_sender("", proton::sender_options{}.target(proton::target_options{}.anonymous(true)));
    }

    void on_sender_open(proton::sender& s) override {
        sender = s;
        s.session().transaction_declare();
    }

    void on_session_transaction_declared(proton::session& s) override {
        std::cout << "Transaction is declared: " << s.transaction_id() << std::endl;
        send(sender);
    }

    void on_session_error(proton::session &s) override {
        std::cout << "Session error: " << s.error().what() << std::endl;
        s.connection().close();
    }

    void on_session_transaction_error(proton::session &s) override {
        std::cout << "Transaction error!" << s.transaction_error().what() << std::endl;
        s.connection().close();
    }

    void on_sendable(proton::sender& sender) override {
        proton::session session = sender.session();
        send(sender);
    }

    void send(proton::sender& sender) {
        while (sender.session().transaction_is_declared() && sender.credit() &&
               current_batch < batch_size) {
            proton::message msg;

            msg.id(msg_id++);
            if (msg_id != err_id) {
                msg.to(addr_);
            }
            msg.body(std::map<std::string, int>{{"batch", batch_index}, {"index", current_batch}});
            std::cout << "Sending [" << batch_index << ", " << current_batch << "]: " << msg << std::endl;
            sender.send(msg);
            current_batch += 1;
        }
    }

    void on_transactional_accept(proton::tracker &t) override {
        accepted++;
        total_accepted++;
        if (total_accepted == abort_message) {
            t.session().transaction_abort();
        } else if (accepted == batch_size) {
            t.session().transaction_commit();
        }
    }

    void on_transactional_reject(proton::tracker &t) override {
        std::cout << "Delivery rejected!" << std::endl;
        t.session().transaction_abort();
    }

    void on_transactional_release(proton::tracker &t) override {
        std::cout << "Delivery released!" << std::endl;
        t.session().transaction_abort();
    }

    void on_session_transaction_committed(proton::session &s) override {
        committed += accepted;
        batch_index++;
        std::cout << "Transaction committed" << std::endl;
        if (committed >= total) {
            std::cout << committed << " messages committed, closing connection." << std::endl;
            s.connection().close();
        } else {
            current_batch = 0;
            accepted = 0;
            std::cout << "Re-declaring transaction now..." << std::endl;
            s.transaction_declare();
        }
    }

    // The committed messages will be settled by the broker, we should settle them too.
    void on_tracker_settle(proton::tracker &t) override {
        std::cout << "Broker settled tracker: " << t.tag() << std::endl;
    }

    void on_session_transaction_aborted(proton::session &s) override {
        std::cout << "Transaction aborted!" << std::endl;
        // Check if this was a failed commit
        auto error = s.transaction_error();
        if (error) {
            std::cout << "Transaction error: " << error.what() << std::endl;
            s.connection().close();
        }

        // Don't close the connection if we deliberately injected an abort
        if (abort_message == 0) {
            s.connection().close();
        } else {
            current_batch = 0;
            accepted = 0;
            std::cout << "Re-declaring transaction now..." << std::endl;
            s.transaction_declare();
        }
    }

    void on_sender_close(proton::sender &s) override {
        current_batch = 0;
    }

};

int main(int argc, char **argv) {
    std::string conn_url = "//127.0.0.1:5672";
    std::string addr = "examples";
    int message_count = 6;
    int batch_size = 3;
    int abort_message = 0;
    int error_message = 0;
    example::options opts(argc, argv);

    opts.add_value(conn_url, 'u', "url", "connect and send to URL", "URL");
    opts.add_value(addr, 'a', "address", "connect and send to address", "ADDR");
    opts.add_value(message_count, 'm', "messages", "number of messages to send", "COUNT");
    opts.add_value(batch_size, 'b', "batch_size", "number of messages in each transaction", "BATCH_SIZE");
    opts.add_value(abort_message, 'c', "abort_message", "message number to abort the transaction", "ABORT_MESSAGE");
    opts.add_value(error_message, 'e', "error_message", "message number to send in error", "ERROR_MESSAGE");

    try {
        opts.parse();

        tx_send send(conn_url, addr, message_count, batch_size, abort_message, error_message);
        proton::container(send).run();

        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
