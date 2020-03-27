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

/*
 * Service Bus example.
 *
 * This is an example of using "Service Bus sessions" (not the same thing as an
 * AMQP session) to selectively retrieve messages from a queue.  The queue must
 * be configured within Service Bus to support sessions.  Service Bus uses the
 * AMQP group_id message property to associate messages with a particular
 * Service Bus session.  It uses AMQP filters to specify which session is
 * associated with a receiver.
 *
 * The mechanics for sending and receiving to other types of service bus queue
 * are broadly the same, as long as the step using the
 * receiver.source().filters() is omitted.
 *
 * Other Service Bus notes: There is no drain support, hence the need to to use
 * timeouts in this example to detect the end of the message stream.  There is
 * no browse support when setting the AMQP link distribution mode to COPY.
 * Service Bus claims to support browsing, but it is unclear how to manage that
 * with an AMQP client.  Maximum message sizes (for body and headers) vary
 * between queue types and fee tier ranging from 64KB to 1MB.  Due to the
 * distributed nature of Service Bus, queues do not automatically preserve FIFO
 * order of messages unless the user takes steps to force the message stream to
 * a single partition of the queue or creates the queue with partitioning disabled.
 *
 * This example shows use of the simpler SAS (Shared Access Signature)
 * authentication scheme where the credentials are supplied on the connection.
 * Service Bus does not actually check these credentials when setting up the
 * connection, it merely caches the SAS key and policy (AKA key name) for later
 * access authorization when creating senders and receivers.  There is a second
 * authentication scheme that allows for multiple tokens and even updating them
 * within a long-lived connection which uses special management request-response
 * queues in Service Bus.  The format of this exchange may be documented
 * somewhere but is also available by working through the CbsAsyncExample.cs
 * program in the Amqp.Net Lite project.
 *
 * The sample output for this program is:

   sent message: message 0 in service bus session "red"
   sent message: message 1 in service bus session "green"
   sent message: message 2 in service bus session "blue"
   sent message: message 3 in service bus session "red"
   sent message: message 4 in service bus session "black"
   sent message: message 5 in service bus session "blue"
   sent message: message 6 in service bus session "yellow"
receiving messages with session identifier "green" from queue ses_q1
   received message: message 1 in service bus session "green"
receiving messages with session identifier "red" from queue ses_q1
   received message: message 0 in service bus session "red"
   received message: message 3 in service bus session "red"
receiving messages with session identifier "blue" from queue ses_q1
   received message: message 2 in service bus session "blue"
   received message: message 5 in service bus session "blue"
receiving messages with session identifier "black" from queue ses_q1
   received message: message 4 in service bus session "black"
receiving messages with session identifier "yellow" from queue ses_q1
   received message: message 6 in service bus session "yellow"
Done. No more messages.

 *
 */

#include "options.hpp"

#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/delivery.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver_options.hpp>
#include <proton/ssl.hpp>
#include <proton/sender.hpp>
#include <proton/sender_options.hpp>
#include <proton/source_options.hpp>
#include <proton/tracker.hpp>
#include <proton/work_queue.hpp>

#include <iostream>
#include <sstream>

#include "fake_cpp11.hpp"

using proton::source_options;
using proton::connection_options;
using proton::sender_options;
using proton::receiver_options;
using proton::ssl_client_options;

void do_next_sequence();

namespace {
void check_arg(const std::string &value, const std::string &name) {
    if (value.empty())
        throw std::runtime_error("missing argument for \"" + name + "\"");
}
}

/// Connect to Service Bus queue and retrieve messages in a particular session.
class session_receiver : public proton::messaging_handler {
  private:
    const std::string &connection_url;
    const std::string &entity;
    connection_options coptions;
    proton::value session_identifier; // AMQP null type by default, matches any Service Bus sequence identifier
    int message_count;
    bool closed;
    proton::duration read_timeout;
    proton::timestamp last_read;
    proton::container *container;
    proton::receiver receiver;

  public:
    session_receiver(const std::string &c, const std::string &e, const connection_options &co,
                     const char *sid) :
        connection_url(c), entity(e), coptions(co),
        message_count(0), closed(false), read_timeout(5000), last_read(0), container(0) {
        if (sid)
            session_identifier = std::string(sid);
        // session_identifier is now either empty/null or an AMQP string type.
        // If null, Service Bus will pick the first available message and create
        // a filter at its end with that message's session identifier.
        // Technically, an AMQP string is not a valid filter-set value unless it
        // is annotated as an AMQP described type, so this may change.

    }

    void run (proton::container &c) {
        message_count = 0;
        closed = false;
        c.connect(connection_url, coptions.handler(*this));
        container = &c;
    }

    void on_connection_open(proton::connection &connection) OVERRIDE {
        proton::source::filter_map sb_filter_map;
        proton::symbol key("com.microsoft:session-filter");
        sb_filter_map.put(key, session_identifier);
        receiver = connection.open_receiver(entity, receiver_options().source(source_options().filters(sb_filter_map)));

        // Start timeout processing here.  If Service Bus has no pending
        // messages, it may defer completing the receiver open until a message
        // becomes available (e.g. to be able to set the actual session
        // identifier if none was specified).
        last_read = proton::timestamp::now();
        // Call this->process_timeout after read_timeout.
        connection.work_queue().schedule(read_timeout, [this]() { this->process_timeout(); });
    }

    void on_receiver_open(proton::receiver &r) OVERRIDE {
        if (closed) return; // PROTON-1264
        proton::value actual_session_id = r.source().filters().get("com.microsoft:session-filter");
        std::cout << "receiving messages with session identifier \"" << actual_session_id
                  << "\" from queue " << entity << std::endl;
        last_read = proton::timestamp::now();
    }

    void on_message(proton::delivery &, proton::message &m) OVERRIDE {
        message_count++;
        std::cout << "   received message: " << m.body() << std::endl;
        last_read = proton::timestamp::now();
    }

    void process_timeout() {
        proton::timestamp deadline = last_read + read_timeout;
        proton::timestamp now = proton::timestamp::now();
        if (now >= deadline) {
            receiver.close();
            closed = true;
            receiver.connection().close();
            if (message_count)
                do_next_sequence();
            else
                std::cout << "Done. No more messages." << std::endl;
        } else {
            proton::duration next = deadline - now;
            receiver.work_queue().schedule(next, [this]() { this->process_timeout(); });
        }
    }
};


/// Connect to Service Bus queue and send messages divided into different sessions.
class session_sender : public proton::messaging_handler {
  private:
    const std::string &connection_url;
    const std::string &entity;
    connection_options coptions;
    int msg_count;
    int total;
    int accepts;

  public:
    session_sender(const std::string &c, const std::string &e, const connection_options &co) :
        connection_url(c), entity(e), coptions(co),
        msg_count(0), total(7), accepts(0) {}

    void run(proton::container &c) {
        c.open_sender(connection_url + "/" + entity, sender_options(), coptions.handler(*this));
    }

    void send_remaining_messages(proton::sender &s) {
        std::string gid;
        for (; msg_count < total && s.credit() > 0; msg_count++) {
            switch (msg_count) {
            case 0: gid = "red"; break;
            case 1: gid = "green"; break;
            case 2: gid = "blue"; break;
            case 3: gid = "red"; break;
            case 4: gid = "black"; break;
            case 5: gid = "blue"; break;
            case 6: gid = "yellow"; break;
            }

            std::ostringstream mbody;
            mbody << "message " << msg_count << " in service bus session \"" << gid << "\"";
            proton::message m(mbody.str());
            m.group_id(gid);  // Service Bus uses the group_id property to as the session identifier.
            s.send(m);
            std::cout << "   sent message: " << m.body() << std::endl;
        }
    }

    void on_sendable(proton::sender &s) OVERRIDE {
        send_remaining_messages(s);
    }

    void on_tracker_accept(proton::tracker &t) OVERRIDE {
        accepts++;
        if (accepts == total) {
            // upload complete
            t.sender().close();
            t.sender().connection().close();
            do_next_sequence();
        }
    }
};


/// Orchestrate the sequential actions of sending and receiving session-based messages.
class sequence : public proton::messaging_handler {
  private:
    proton::container *container;
    int sequence_no;
    session_sender snd;
    session_receiver rcv_red, rcv_green, rcv_null;

  public:
    static sequence *the_sequence;

    sequence (const std::string &c, const std::string &e, const connection_options &co) :
        container(0), sequence_no(0),
        snd(c, e, co), rcv_red(c, e, co, "red"), rcv_green(c, e, co, "green"), rcv_null(c, e, co, NULL) {
        the_sequence = this;
    }

    void on_container_start(proton::container &c) OVERRIDE {
        container = &c;
        next_sequence();
    }

    void next_sequence() {
        switch (sequence_no++) {
        // run these in order exactly once
        case 0: snd.run(*container); break;
        case 1: rcv_green.run(*container); break;
        case 2: rcv_red.run(*container); break;
        // Run this until the receiver decides there is no messages left to sequence through
        default: rcv_null.run(*container); break;
        }
    }
};

sequence *sequence::the_sequence = NULL;

void do_next_sequence() { sequence::the_sequence->next_sequence(); }


int main(int argc, char **argv) {
    std::string sb_namespace; // i.e. "foo.servicebus.windows.net"
    std::string sb_key_name;  // shared access key name for entity (AKA "Policy Name")
    std::string sb_key;       // shared access key
    std::string sb_entity;    // AKA the service bus queue.  Must enable
                              // sessions on it for this example.

    example::options opts(argc, argv);
    opts.add_value(sb_namespace, 'n', "namespace", "Service Bus full namespace", "NAMESPACE");
    opts.add_value(sb_key_name, 'p', "policy", "policy name that specifies access rights (key name)", "POLICY");
    opts.add_value(sb_key, 'k', "key", "secret key for the policy", "key");
    opts.add_value(sb_entity, 'e', "entity", "entity path (queue name)", "ENTITY");

    try {
        opts.parse();
        check_arg(sb_namespace, "namespace");
        check_arg(sb_key_name, "policy");
        check_arg(sb_key, "key");
        check_arg(sb_entity, "entity");
        std::string connection_string("amqps://" + sb_namespace);

        sequence seq(connection_string, sb_entity,
                     connection_options()
                     .user(sb_key_name)
                     .password(sb_key)
                     .sasl_allowed_mechs("PLAIN"));
        proton::container(seq).run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
