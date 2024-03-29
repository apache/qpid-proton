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


#include "test_bits.hpp"

#include "proton/connection.hpp"
#include "proton/connection_options.hpp"
#include "proton/container.hpp"
#include "proton/listener.hpp"
#include "proton/listen_handler.hpp"
#include "proton/messaging_handler.hpp"
#include "proton/receiver.hpp"
#include "proton/receiver_options.hpp"
#include "proton/sender.hpp"
#include "proton/sender_options.hpp"
#include "proton/work_queue.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <string>
#include <cstdio>
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace {

std::string make_url(std::string host, int port) {
    std::ostringstream url;
    url << "//" << host << ":" << port;
    return url.str();
}

struct test_listen_handler : public proton::listen_handler {
    bool on_open_, on_accept_, on_close_;
    std::string on_error_;
    std::string host_;
    proton::connection_options opts_;

    test_listen_handler(const std::string& host=std::string(),
                        const proton::connection_options& opts=proton::connection_options()
    ) : on_open_(false), on_accept_(false), on_close_(false), host_(host), opts_(opts) {}

    proton::connection_options on_accept(proton::listener&) override {
        on_accept_ = true;
        return proton::connection_options();
    }
    void on_open(proton::listener& l) override {
        on_open_ = true;
        ASSERT(!on_accept_);
        ASSERT(on_error_.empty());
        ASSERT(!on_close_);
        l.container().connect(make_url(host_, l.port()), opts_);
    }

    void on_close(proton::listener&) override {
        on_close_ = true;
        ASSERT(on_open_ || on_error_.size());
    }

    void on_error(proton::listener&, const std::string& e) override {
        on_error_ = e;
        ASSERT(!on_close_);
    }
};

class test_handler : public proton::messaging_handler {
  public:
    bool closing;
    bool done;

    std::string peer_vhost;
    std::string peer_container_id;
    std::vector<proton::symbol> peer_offered_capabilities;
    std::vector<proton::symbol> peer_desired_capabilities;
    std::map<proton::symbol, proton::value> peer_properties;
    proton::listener listener;
    test_listen_handler listen_handler;

    test_handler(const std::string h, const proton::connection_options& c_opts)
        : closing(false), done(false), listen_handler(h, c_opts)
    {}

    void on_container_start(proton::container &c) override {
        listener = c.listen("//:0", listen_handler);
    }

    void on_connection_open(proton::connection &c) override {
        ASSERT(listen_handler.on_open_);
        ASSERT(!listen_handler.on_close_);
        ASSERT(listen_handler.on_error_.empty());
        // First call is the incoming server-side of the connection, that we are interested in.
        // Second call is for the response to the client, ignore that.
        if (!closing) {
            peer_vhost = c.virtual_host();
            peer_container_id = c.container_id();
            peer_offered_capabilities = c.offered_capabilities();
            peer_desired_capabilities = c.desired_capabilities();
            peer_properties = c.properties();
            c.close();
        }
        closing = true;
    }

    void on_connection_close(proton::connection &) override {
        if (!done) listener.stop();
        done = true;
    }
};

int test_container_default_container_id() {
    proton::connection_options opts;
    test_handler th("", opts);
    proton::container(th).run();
    ASSERT(!th.peer_container_id.empty());
    ASSERT(th.listen_handler.on_error_.empty());
    ASSERT(th.listen_handler.on_close_);
    return 0;
}

int test_container_vhost() {
    proton::connection_options opts;
    opts.virtual_host("a.b.c");
    test_handler th("", opts);
    proton::container(th).run();
    ASSERT_EQUAL(th.peer_vhost, "a.b.c");
    return 0;
}

int test_container_default_vhost() {
    proton::connection_options opts;
    test_handler th("127.0.0.1", opts);
    proton::container(th).run();
    ASSERT_EQUAL(th.peer_vhost, "127.0.0.1");
    return 0;
}

int test_container_no_vhost() {
    // explicitly setting an empty virtual-host will cause the Open
    // performative to be sent without a hostname field present.
    // Sadly whether or not a 'hostname' field was received cannot be
    // determined from here, so just exercise the code
    proton::connection_options opts;
    opts.virtual_host("");
    test_handler th("127.0.0.1", opts);
    proton::container(th).run();
    ASSERT_EQUAL(th.peer_vhost, "");
    return 0;
}

std::vector<proton::symbol> make_caps(const std::string& s) {
    std::vector<proton::symbol> caps;
    caps.push_back(s);
    return caps;
}

int test_container_capabilities() {
    proton::connection_options opts;
    opts.offered_capabilities(make_caps("offered"));
    opts.desired_capabilities(make_caps("desired"));
    test_handler th("", opts);
    proton::container(th).run();
    ASSERT_EQUAL(th.peer_offered_capabilities.size(), 1u);
    ASSERT_EQUAL(th.peer_offered_capabilities[0], proton::symbol("offered"));
    ASSERT_EQUAL(th.peer_desired_capabilities.size(), 1u);
    ASSERT_EQUAL(th.peer_desired_capabilities[0], proton::symbol("desired"));
    ASSERT_EQUAL(th.peer_properties.size(), 0u);
    return 0;
}

int test_container_properties() {
    proton::connection_options opts;
    std::map<proton::symbol, proton::value> props;
    props["qpid.client_process"] = "test_process";
    props["qpid.client_pid"] = 123;
    opts.properties(props);
    test_handler th("", opts);
    proton::container(th).run();
    ASSERT_EQUAL(th.peer_properties.size(), 2u);
    ASSERT_EQUAL(th.peer_properties["qpid.client_process"], "test_process");
    ASSERT_EQUAL(th.peer_properties["qpid.client_pid"], 123);
    return 0;
}

int test_container_bad_address() {
    // Listen on a bad address, check for leaks
    // Regression test for https://issues.apache.org/jira/browse/PROTON-1217

    proton::container c;
    // Default fixed-option listener. Valgrind for leaks.
    c.listen("999.666.999.666:0");
    c.run();
    // Dummy listener.
    test_listen_handler l;
    test_handler h2("999.999.999.666", proton::connection_options());
    c.listen("999.666.999.666:0", l);
    c.run();
    ASSERT(!l.on_open_);
    ASSERT(!l.on_accept_);
    ASSERT(l.on_close_);
    ASSERT(!l.on_error_.empty()); // proton:io: Name or service not known
    return 0;
}

class stop_tester : public proton::messaging_handler {
    proton::listener listener;
    test_listen_handler listen_handler;

    // Set up a listener which would block forever
    void on_container_start(proton::container& c) override {
        ASSERT(state==0);
        listener = c.listen("//:0", listen_handler);
        c.auto_stop(false);
        state = 1;
    }

    // Get here twice - once for listener, once for connector
    void on_connection_open(proton::connection &c) override {
        c.close();
        state++;
    }

    void on_connection_close(proton::connection &c) override {
        ASSERT(state==3);
        c.container().stop();
        state = 4;
    }
    void on_container_stop(proton::container & ) override {
        ASSERT(state==4);
        state = 5;
    }

    void on_transport_error(proton::transport & t) override {
        // Do nothing - ignore transport errors - we're going to get one when
        // the container stops.
    }

public:
    stop_tester(): state(0) {}

    int state;
};

int test_container_stop() {
    stop_tester t;
    proton::container(t).run();
    ASSERT(t.state==5);
    return 0;
}


struct hang_tester : public proton::messaging_handler {
    proton::listener listener;
    bool done = false;

    hang_tester() = default;

    void on_container_start(proton::container& c) override {
        listener = c.listen("//:0");
        c.schedule(proton::duration(250), [&](){ c.connect(make_url("", listener.port())); });
    }

    void on_connection_open(proton::connection& c) override {
        c.close();
    }

    void on_connection_close(proton::connection& c) override {
        if (!done) {
            done = true;
            listener.stop();
        }
    }
};

int test_container_schedule_nohang() {
    hang_tester t;
    proton::container(t).run();
    return 0;
}

class immediate_stop_tester : public proton::messaging_handler {
public:
    void on_container_start(proton::container &c) override {
        c.stop();
    }
};

int test_container_immediate_stop() {
    immediate_stop_tester t;
    proton::container(t).run();  // Should return after on_container_start
    return 0;
}

int test_container_pre_stop() {
    proton::container c;
    c.stop();
    c.run();                    // Should return immediately
    return 0;
}


struct schedule_tester : public proton::messaging_handler {
    void on_container_start(proton::container& c) override {
        c.schedule(proton::duration(250), [&](){ c.stop(); });
    }
};

int test_container_schedule_stop() {
    schedule_tester tester;
    proton::container c(tester);
    c.auto_stop(false);
    c.run();
    return 0;
}

class link_test_handler : public proton::messaging_handler {//, public proton::listen_handler {
  public:
    bool had_receiver;
    bool had_sender;

    test_listen_handler listen_handler;
    proton::listener listener;

    proton::receiver_options receiver_options;
    proton::sender_options sender_options;

    std::map<proton::symbol, proton::value> peer_receiver_properties;
    std::map<proton::symbol, proton::value> peer_sender_properties;

    link_test_handler(const proton::receiver_options &r_opts=proton::receiver_options(),
                      const proton::sender_options &s_opts=proton::sender_options())
        : had_receiver(false),
          had_sender(false),
          receiver_options(r_opts),
          sender_options(s_opts)
    {}

    void on_container_start(proton::container &c) override {
        listener = c.listen("//:0", listen_handler);
    }

    void on_connection_open(proton::connection &c) override {
        if (c.uninitialized()) {
            proton::messaging_handler::on_connection_open(c);
            c.open_receiver("", receiver_options);
            c.open_sender("", sender_options);
        }
    }


    void check_close(proton::link &l) {
        if (had_receiver && had_sender) {
            l.connection().close();
            listener.stop();
        }
    }

    void on_receiver_open(proton::receiver &l) override {
        had_receiver = true;
        // When a client creates a sender then the server is notified about it as a receiver
        peer_sender_properties = l.properties();
        check_close(l);
    }

    void on_sender_open(proton::sender &l) override {
        had_sender = true;
        // When a client creates a receiver then the server is notified about it as a sender
        peer_receiver_properties = l.properties();
        check_close(l);
    }
};

int test_container_links_no_properties() {
    link_test_handler th;
    proton::container(th).run();
    ASSERT(th.had_receiver);
    ASSERT(th.had_sender);
    ASSERT_EQUAL(th.peer_receiver_properties.size(), 0u);
    ASSERT_EQUAL(th.peer_sender_properties.size(), 0u);
    return 0;
}

int test_container_links_properties() {
    proton::receiver_options r_opts;
    std::map<proton::symbol, proton::value> r_props;
    r_props["recv.prop_str"] = "receiver_string";
    r_opts.properties(r_props);

    proton::sender_options s_opts;
    std::map<proton::symbol, proton::value> s_props;
    s_props["send.prop_str"] = "sender_string";
    s_props["send.prop_int"] = 123456789;
    s_opts.properties(s_props);

    link_test_handler th(r_opts, s_opts);
    proton::container(th).run();

    ASSERT(th.had_receiver);
    ASSERT(th.had_sender);
    ASSERT_EQUAL(th.peer_receiver_properties.size(), 1u);
    ASSERT_EQUAL(th.peer_receiver_properties["recv.prop_str"], "receiver_string");
    ASSERT_EQUAL(th.peer_sender_properties.size(), 2u);
    ASSERT_EQUAL(th.peer_sender_properties["send.prop_str"], "sender_string");
    ASSERT_EQUAL(th.peer_sender_properties["send.prop_int"], 123456789);
    return 0;
}

class test_mt_handler : public proton::messaging_handler {
  public:
    std::mutex lock_;
    std::condition_variable cond_;
    std::string str_;
    proton::error_condition err_;

    void set(const std::string& s) {
        std::lock_guard<std::mutex> l(lock_);
        str_ = s;
        cond_.notify_one();
    }

    std::string wait() {
        std::unique_lock<std::mutex> l(lock_);
        while (str_.empty()) cond_.wait(l);
        std::string s = str_;
        str_.clear();
        return s;
    }

    proton::error_condition error() const { return err_; }
    void on_container_start(proton::container &) override { set("start"); }
    void on_connection_open(proton::connection &) override { set("open"); }

    // Catch errors and save.
    void on_error(const proton::error_condition& e) override { err_ = e; }
};

class container_runner {
    proton::container& c_;

  public:
    container_runner(proton::container& c) : c_(c) {}

    void operator()() {c_.run();}
};

void test_container_mt_stop_empty() {
    test_mt_handler th;
    proton::container c(th);
    c.auto_stop( false );
    container_runner runner(c);
    auto t = std::thread(runner);
    // Must ensure that thread is joined
    try {
        ASSERT_EQUAL("start", th.wait());
        c.stop();
        t.join();
        ASSERT_EQUAL("", th.error().name());
    } catch (const std::exception &e) {
        std::cerr << FAIL_MSG(e.what()) << std::endl;
        // If join hangs, let the test die by timeout.  We cannot
        // detach and continue: deleting the container while it is
        // still alive will put the process in an undefined state.
        t.join();
        throw;
    }
}

void test_container_mt_stop() {
    test_mt_handler th;
    proton::container c(th);
    c.auto_stop(false);
    container_runner runner(c);
    auto t = std::thread(runner);
    // Must ensure that thread is joined
    try {
        test_listen_handler lh;
        ASSERT_EQUAL("start", th.wait());
        c.listen("//:0", lh);       //  Also opens a connection
        ASSERT_EQUAL("open", th.wait());
        c.stop();
        t.join();
    } catch (const std::exception& e) {
        std::cerr << FAIL_MSG(e.what()) << std::endl;
        // If join hangs, let the test die by timeout.  We cannot
        // detach and continue: deleting the container while t is
        // still alive will put the process in an undefined state.
        t.join();
        throw;
    }
}

class test_mt_handler_wq : public test_mt_handler {
public:
    proton::work_queue *wq_;
    proton::work call_do_close_;
    proton::connection connection_;
    std::mutex wqlock_;

    test_mt_handler_wq() : wq_(0) {}

    void on_connection_open(proton::connection &c) override {
        {
            std::unique_lock<std::mutex> l(wqlock_);
            // Just record first connection side, inbound or outbound
            if (!connection_) {
                connection_ = c;
                wq_ = &c.work_queue();
                call_do_close_ = make_work(&test_mt_handler_wq::do_close, this);
            }
            else
                return;
        }
        test_mt_handler::on_connection_open(c);
    }
    void initiate_close() {
        std::unique_lock<std::mutex> l(wqlock_);
        wq_->add(call_do_close_);
    }
    void do_close() { connection_.close(); }
    void on_connection_close(proton::connection &) override { set("closed"); }
};

void test_container_mt_close_race() {
    test_mt_handler_wq th;
    proton::container c(th);
    c.auto_stop(false);
    container_runner runner(c);
    auto t = std::thread(runner);
    // Must ensure that thread is joined
    try {
        test_listen_handler lh;
        ASSERT_EQUAL("start", th.wait());
        c.listen("//:0", lh);       //  Also opens a connection
        ASSERT_EQUAL("open", th.wait());
        th.initiate_close();
        ASSERT_EQUAL("closed", th.wait());
        // The two sides of the connection are closing, each with its
        // own connection context.  Start a proactor disconnect to run
        // competing close cleanup in a third context.  PROTON-2027.
        c.stop();
        t.join();
    } catch (const std::exception& e) {
        std::cerr << FAIL_MSG(e.what()) << std::endl;
        // If join hangs, let the test die by timeout.  We cannot
        // detach and continue: deleting the container while t is
        // still alive will put the process in an undefined state.
        t.join();
        throw;
    }
}

class schedule_cancel : public proton::messaging_handler {
    proton::listener listener;
    test_listen_handler listen_handler;
    proton::work_handle w4_handle;
    proton::work_handle w5_handle;

public:
    int w1_state = 0;
    int w2_state = 0;
    int w3_state = 0;
    int w4_state = 0;
    int w5_state = 0;

    void on_container_start(proton::container& c) override {
        listener = c.listen("//:0", listen_handler);

        // We will cancel this scheduled task before its execution.
        auto w1_handle = c.schedule(proton::duration(250),
            [this](){
                w1_state = 1;
            });

        // We will cancel this scheduled task before its execution and will try to cancel it again.
        auto w2_handle = c.schedule(proton::duration(260),
            [this](){
                w2_state = 1;
            });

        // Attempt to make sure that we can cancel a task from a previous task even if the
        // previous task gets delayed and scheduled in the same batch as the task to be cancelled.

        // Set up task to cancel
        auto w3_handle = c.schedule(proton::duration(40),
            [this](){
                w3_state = 3;
            });

        // This should successfully cancel the first scheduled task and so leave w3_state at 2
        c.schedule(proton::duration(35),
            [&c, w3_handle, this](){
                ASSERT(w3_state==1);
                w3_state = 2;
                c.cancel(w3_handle);
            });

        // This task overruns and so forces the next 2 tasks to run (the ones above) to be scheduled together
        c.schedule(proton::duration(30),
                   [this](){
                     w3_state = 1;
                     std::this_thread::sleep_for(std::chrono::milliseconds(30));
                   });

        // We will try to cancel this task before its execution from different thread i.e connection thread.
        w4_handle = c.schedule(proton::duration(270),
            [this](){
                w4_state = 1;
            });

        // We will try to cancel this task after its execution from different thread i.e. connection thread.
        w5_handle = c.schedule(proton::duration(0),
            [this](){
                w5_state = 1;
            });

        // Cancel the first scheduled task.
        c.cancel(w1_handle);

        // Try cancelling the second scheduled task two times.
        c.cancel(w2_handle);
        c.cancel(w2_handle);

        // Try cancelling invalid work handle.
        c.cancel(-1);
        c.auto_stop(false);
    }

    // Get here twice - once for listener, once for connector
    void on_connection_open(proton::connection &c) override {
        c.close();
    }

    void on_connection_close(proton::connection &c) override {
        // Cross-thread cancelling

        ASSERT(w4_state==0);
        // Cancel the fourth task before its execution.
        c.container().cancel(w4_handle);

        ASSERT(w5_state==1);
        // Cancel the already executed fifth task.
        c.container().cancel(w5_handle);

        c.container().stop();
    }

    void on_transport_error(proton::transport & t) override {
        // Do nothing - ignore transport errors - we're going to get one when
        // the container stops.
    }

    schedule_cancel() = default;
};

int test_container_schedule_cancel() {
    schedule_cancel t;

    ASSERT(t.w1_state==0);
    ASSERT(t.w2_state==0);
    ASSERT(t.w3_state==0);
    ASSERT(t.w4_state==0);
    ASSERT(t.w5_state==0);

    proton::container(t).run(2);

    ASSERT(t.w1_state==0); // The value of w1_state remained 0 because we cancelled the associated task before its execution.
    ASSERT(t.w2_state==0); // The value of w2_state remained 0 because we cancelled the associated task before its execution.
    ASSERT(t.w3_state==2); // The value of w3_state changed to 2 because we set this in the second callback, but the third was cancelled
    ASSERT(t.w4_state==0); // The value of w4_state remained 0 because we cancelled the associated task before its execution.
    ASSERT(t.w5_state==1); // The value of w5_state changed to 1 because the task was already executed before we cancelled it.
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    int failed = 0;
    RUN_ARGV_TEST(failed, test_container_default_container_id());
    RUN_ARGV_TEST(failed, test_container_vhost());
    RUN_ARGV_TEST(failed, test_container_capabilities());
    RUN_ARGV_TEST(failed, test_container_properties());
    RUN_ARGV_TEST(failed, test_container_default_vhost());
    RUN_ARGV_TEST(failed, test_container_no_vhost());
    RUN_ARGV_TEST(failed, test_container_bad_address());
    RUN_ARGV_TEST(failed, test_container_stop());
    RUN_ARGV_TEST(failed, test_container_schedule_nohang());
    RUN_ARGV_TEST(failed, test_container_immediate_stop());
    RUN_ARGV_TEST(failed, test_container_pre_stop());
    RUN_ARGV_TEST(failed, test_container_schedule_stop());
    RUN_ARGV_TEST(failed, test_container_links_no_properties());
    RUN_ARGV_TEST(failed, test_container_links_properties());
    RUN_ARGV_TEST(failed, test_container_mt_stop_empty());
    RUN_ARGV_TEST(failed, test_container_mt_stop());
    RUN_ARGV_TEST(failed, test_container_mt_close_race());
    RUN_ARGV_TEST(failed, test_container_schedule_cancel());
    return failed;
}
