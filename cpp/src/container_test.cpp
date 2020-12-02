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
#include "proton/messaging_handler.hpp"
#include "proton/listener.hpp"
#include "proton/listen_handler.hpp"
#include "proton/work_queue.hpp"

#include <cstdlib>
#include <ctime>
#include <string>
#include <cstdio>
#include <sstream>

#if PN_CPP_SUPPORTS_THREADS
# include <thread>
# include <mutex>
# include <condition_variable>
#endif

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

    proton::connection_options on_accept(proton::listener&) PN_CPP_OVERRIDE {
        on_accept_ = true;
        return proton::connection_options();
    }
    void on_open(proton::listener& l) PN_CPP_OVERRIDE {
        on_open_ = true;
        ASSERT(!on_accept_);
        ASSERT(on_error_.empty());
        ASSERT(!on_close_);
        l.container().connect(make_url(host_, l.port()), opts_);
    }

    void on_close(proton::listener&) PN_CPP_OVERRIDE {
        on_close_ = true;
        ASSERT(on_open_ || on_error_.size());
    }

    void on_error(proton::listener&, const std::string& e) PN_CPP_OVERRIDE {
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

    void on_container_start(proton::container &c) PN_CPP_OVERRIDE {
        listener = c.listen("//:0", listen_handler);
    }

    void on_connection_open(proton::connection &c) PN_CPP_OVERRIDE {
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

    void on_connection_close(proton::connection &) PN_CPP_OVERRIDE {
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
    void on_container_start(proton::container& c) PN_CPP_OVERRIDE {
        ASSERT(state==0);
        listener = c.listen("//:0", listen_handler);
        c.auto_stop(false);
        state = 1;
    }

    // Get here twice - once for listener, once for connector
    void on_connection_open(proton::connection &c) PN_CPP_OVERRIDE {
        c.close();
        state++;
    }

    void on_connection_close(proton::connection &c) PN_CPP_OVERRIDE {
        ASSERT(state==3);
        c.container().stop();
        state = 4;
    }
    void on_container_stop(proton::container & ) PN_CPP_OVERRIDE {
        ASSERT(state==4);
        state = 5;
    }

    void on_transport_error(proton::transport & t) PN_CPP_OVERRIDE {
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
    bool done;

    hang_tester() : done(false) {}

    void connect(proton::container* c) {
        c->connect(make_url("", listener.port()));
    }

    void on_container_start(proton::container& c) PN_CPP_OVERRIDE {
        listener = c.listen("//:0");
        c.schedule(proton::duration(250), proton::make_work(&hang_tester::connect, this, &c));
    }

    void on_connection_open(proton::connection& c) PN_CPP_OVERRIDE {
        c.close();
    }

    void on_connection_close(proton::connection& c) PN_CPP_OVERRIDE {
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
    void on_container_start(proton::container &c) PN_CPP_OVERRIDE {
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
    void stop(proton::container* c) { c->stop(); }

    void on_container_start(proton::container& c) PN_CPP_OVERRIDE {
        c.schedule(proton::duration(250), proton::make_work(&schedule_tester::stop, this, &c));
    }
};

int test_container_schedule_stop() {
    schedule_tester tester;
    proton::container c(tester);
    c.auto_stop(false);
    c.run();
    return 0;
}


#if PN_CPP_SUPPORTS_THREADS // Tests that require thread support

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
    void on_container_start(proton::container &) PN_CPP_OVERRIDE { set("start"); }
    void on_connection_open(proton::connection &) PN_CPP_OVERRIDE { set("open"); }

    // Catch errors and save.
    void on_error(const proton::error_condition& e) PN_CPP_OVERRIDE { err_ = e; }
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

    void on_connection_open(proton::connection &c) PN_CPP_OVERRIDE {
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
    void on_connection_close(proton::connection &) PN_CPP_OVERRIDE { set("closed"); }
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

#endif

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
#if PN_CPP_SUPPORTS_THREADS
    RUN_ARGV_TEST(failed, test_container_mt_stop_empty());
    RUN_ARGV_TEST(failed, test_container_mt_stop());
    RUN_ARGV_TEST(failed, test_container_mt_close_race());
#endif
    return failed;
}
