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
#include <proton/duration.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver.hpp>
#include <proton/receiver_options.hpp>
#include <proton/sender.hpp>
#include <proton/sender_options.hpp>
#include <proton/session.hpp>
#include <proton/target_options.hpp>
#include <proton/tracker.hpp>
#include <proton/transfer.hpp>
#include <proton/types.hpp>
#include <proton/work_queue.hpp>

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#  include <windows.h>
#else
#  include <csignal>
#endif

// Sentinel queue name: omit message 'to' address so the peer may reject the message.
constexpr std::string_view NO_TO_ADDRESS = "<none>";

// Interactive tester for AMQP transactions: declare/commit/abort transactions, receive
// messages (with optional timeout), send to the default or a given queue (or omit 'to'
// for rejection testing), wait for disposition updates, and release unsettled. Type
// 'help' for commands. Received messages are accepted on receipt.

class tx_recv_interactive : public proton::messaging_handler {
  private:
    std::string conn_url_;
    std::string addr_;

    proton::connection connection_;
    proton::receiver receiver_;
    proton::sender sender_;
    proton::session session_;
    proton::work_queue* work_queue_ = nullptr;

    int send_pending_ = 0;
    int send_next_id_ = 0;
    std::string send_to_addr_;

    mutable std::mutex wait_mutex_;
    std::condition_variable wait_cv_;
    bool ready_ = false;
    bool sleep_done_ = true;
    bool timed_out_ = false;
    int fetch_expected_ = 0;
    int fetch_received_ = 0;
    bool fetch_done_ = false;
    int settled_expected_ = 0;
    int settled_received_ = 0;
    bool settled_done_ = false;
    bool interrupt_requested_ = false;
    std::string last_error_;

    /// Run f() in a try block; on error, record the message for later reporting.
    template <typename F>
    void catch_any_error(F&& f) {
        try {
            f();
        } catch (const std::exception& e) {
            auto l = std::lock_guard(wait_mutex_);
            last_error_ = e.what();
        }
    }

    std::mutex sync_mutex_;
    std::condition_variable sync_cv_;
    bool sync_done_ = false;

    void timeout_fired() {
        auto l = std::lock_guard(wait_mutex_);
        timed_out_ = true;
        wait_cv_.notify_all();
    }

    void do_declare() {
        catch_any_error([this]() { session_.transaction_declare(); });
    }

    void do_fetch(int n) {
        receiver_.add_credit(n);
    }

    void do_commit() {
        catch_any_error([this]() { session_.transaction_commit(); });
    }

    void do_abort() {
        catch_any_error([this]() { session_.transaction_abort(); });
    }

    void do_release() {
        for (auto rcv : session_.receivers()) {
            for (auto d : rcv.unsettled_deliveries()) {
                d.release();
            }
        }
    }

    void do_quit() {
        connection_.close();
    }

  public:
    tx_recv_interactive(const std::string& url, const std::string& addr)
        : conn_url_(url), addr_(addr) {}

    void on_container_start(proton::container& c) override {
        c.connect(conn_url_);
    }

    void on_connection_open(proton::connection& conn) override {
        connection_ = conn;
        work_queue_ = &conn.work_queue();
        // credit_window(0) so we control flow via "fetch"
        receiver_ = conn.open_receiver(addr_, proton::receiver_options().credit_window(0));
        sender_ = conn.open_sender("", proton::sender_options{}.target(proton::target_options{}.anonymous(true)));
    }

    void on_sender_open(proton::sender& s) override {
        sender_ = s;
    }

    void on_session_open(proton::session& s) override {
        session_ = s;
        {
            auto l = std::lock_guard(wait_mutex_);
            ready_ = true;
        }
        wait_cv_.notify_all();
    }

    void on_session_transaction_declared(proton::session& s) override {
        std::cout << "transaction declared: " << s.transaction_id() << std::endl;
    }

    void on_session_transaction_committed(proton::session& s) override {
        std::cout << "transaction committed" << std::endl;
    }

    void on_session_transaction_aborted(proton::session& s) override {
        std::cout << "transaction aborted: " << s.transaction_error().what() << std::endl;
    }

    void on_session_transaction_error(proton::session& s) override {
        std::cout << "transaction error: " << s.transaction_error().what() << std::endl;
    }

    void on_sendable(proton::sender&) override {
        try_send();
    }

    void try_send() {
        int to_send = 0;
        std::string to_addr;
        {
            auto l = std::lock_guard(wait_mutex_);
            to_send = send_pending_;
            to_addr = send_to_addr_;
        }
        int sent = 0;
        while (sender_ && sender_.credit() > 0 && to_send > 0) {
            proton::message msg;
            msg.id(send_next_id_);
            if (to_addr != NO_TO_ADDRESS)
                msg.to(to_addr);
            msg.body(std::map<std::string, int>{{"message", send_next_id_}});
            sender_.send(msg);
            ++send_next_id_;
            ++sent;
            --to_send;
        }
        if (sent > 0) {
            auto l = std::lock_guard(wait_mutex_);
            send_pending_ -= sent;
            if (send_pending_ == 0)
                wait_cv_.notify_all();
        }
    }

    void on_message(proton::delivery& d, proton::message& msg) override {
        std::cout << d.tag() << ": " << msg.body() << std::endl;
        d.accept();
        {
            auto l = std::lock_guard(wait_mutex_);
            if (fetch_expected_ > 0) {
                ++fetch_received_;
                if (fetch_received_ >= fetch_expected_) {
                    fetch_done_ = true;
                    wait_cv_.notify_all();
                }
            }
            // Pre-settled deliveries never trigger on_delivery_settle; count them here for wait_settled
            if (settled_expected_ > 0 && d.settled()) {
                ++settled_received_;
                if (settled_received_ >= settled_expected_) {
                    settled_done_ = true;
                    wait_cv_.notify_all();
                }
            }
        }
    }

    void on_delivery_settle(proton::delivery&) override {
        auto l = std::lock_guard(wait_mutex_);
        if (settled_expected_ > 0) {
            ++settled_received_;
            if (settled_received_ >= settled_expected_) {
                settled_done_ = true;
                wait_cv_.notify_all();
            }
        }
    }

    void on_transactional_accept(proton::tracker& t) override {
        std::cout << "disposition: accepted: " << t.tag() << " (transactional: " << t.session().transaction_id() << ")" << std::endl;
    }

    void on_transactional_reject(proton::tracker& t) override {
        std::cout << "disposition: rejected: " << t.tag() << " (transactional: " << t.session().transaction_id() << ")" << std::endl;
    }

    void on_transactional_release(proton::tracker& t) override {
        std::cout << "disposition: released: " << t.tag() << " (transactional: " << t.session().transaction_id() << ")" << std::endl;
    }

    void on_tracker_accept(proton::tracker& t) override {
        std::cout << "disposition: accepted: " << t.tag() << std::endl;
    }

    void on_tracker_reject(proton::tracker& t) override {
        std::cout << "disposition: rejected: " << t.tag() << std::endl;
    }

    void on_tracker_release(proton::tracker& t) override {
        std::cout << "disposition: released: " << t.tag() << std::endl;
    }

    void on_tracker_settle(proton::tracker& t) override {
        std::cout << "disposition: settled: " << t.tag() << std::endl;
    }

    void on_session_error(proton::session& s) override {
        std::cout << "Session error: " << s.error().what() << std::endl;
        s.connection().close();
    }

    /// Called from the SIGINT-handling thread to wake any current wait.
    void request_interrupt() {
        {
            auto l = std::lock_guard(wait_mutex_);
            interrupt_requested_ = true;
            wait_cv_.notify_all();
        }
        {
            auto l = std::lock_guard(sync_mutex_);
            sync_cv_.notify_all();
        }
    }

    /// True if the last wait was exited due to Ctrl-C.
    bool interrupted() const {
        auto l = std::lock_guard(wait_mutex_);
        return interrupt_requested_;
    }

    /// Clear the interrupt flag (e.g. after the command loop has handled it).
    void clear_interrupt() {
        auto l = std::lock_guard(wait_mutex_);
        interrupt_requested_ = false;
    }

    // Thread-safe: wait until handler is ready to accept commands
    void wait_ready() {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        wait_cv_.wait(l, [this] { return ready_ || interrupt_requested_; });
    }

    /// Wait until the connection thread has processed all work queued so far.
    /// There is no "loop is idle" callback in the Proton API; this adds a
    /// sentinel work item and returns when it runs (so the connection thread
    /// has caught up). Use before showing the prompt so the command loop stays
    /// in sync with background connection-thread activity.
    void sync_with_connection_thread() {
        {
            auto l = std::lock_guard(wait_mutex_);
            interrupt_requested_ = false;
        }
        {
            auto l = std::lock_guard(sync_mutex_);
            sync_done_ = false;
        }
        work_queue_->add([this]() {
            auto l = std::lock_guard(sync_mutex_);
            sync_done_ = true;
            sync_cv_.notify_all();
        });
        auto l = std::unique_lock(sync_mutex_);
        sync_cv_.wait(l, [this] {
            if (sync_done_) return true;
            auto w = std::lock_guard(wait_mutex_);
            return interrupt_requested_;
        });
    }

    /// Schedule a callback on the connection thread after a delay (seconds).
    /// wait_sleep_done() blocks until that callback has run.
    void sleep(double seconds) {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        sleep_done_ = false;
        l.unlock();
        auto ms = static_cast<proton::duration::numeric_type>(seconds * 1000);
        work_queue_->schedule(proton::duration(ms), [this]() {
            auto l = std::lock_guard(wait_mutex_);
            sleep_done_ = true;
            wait_cv_.notify_all();
        });
        l.lock();
        wait_cv_.wait(l, [this] { return sleep_done_ || interrupt_requested_; });
    }

    /// Start fetching n messages; optionally timeout after timeout_seconds (0 = no timeout).
    /// wait_fetch_done() blocks until n messages received or timeout (whichever first).
    void fetch(int n, double timeout_seconds) {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        timed_out_ = false;
        fetch_expected_ = n;
        fetch_received_ = 0;
        fetch_done_ = (n <= 0);
        l.unlock();
        work_queue_->add([this, n]() { do_fetch(n); });
        if (timeout_seconds > 0) {
            auto ms = static_cast<proton::duration::numeric_type>(timeout_seconds * 1000);
            work_queue_->schedule(proton::duration(ms), [this]() { timeout_fired(); });
        }
        l.lock();
        wait_cv_.wait(l, [this] { return fetch_done_ || timed_out_ || interrupt_requested_; });
        fetch_expected_ = 0;
    }
    int fetch_received_count() const {
        auto l = std::lock_guard(wait_mutex_);
        return fetch_received_;
    }
    bool timed_out() const {
        auto l = std::lock_guard(wait_mutex_);
        return timed_out_;
    }

    /// Clear the timed-out flag (e.g. after the command loop has handled it).
    void clear_timed_out() {
        auto l = std::lock_guard(wait_mutex_);
        timed_out_ = false;
    }

    /// If a proton::error was recorded, assign its message to out, clear it, and return true.
    bool take_last_error(std::string& out) {
        auto l = std::lock_guard(wait_mutex_);
        if (last_error_.empty())
            return false;
        out = std::move(last_error_);
        last_error_.clear();
        return true;
    }

    /// Wait until N disposition settlements (on_delivery_settle) have been received,
    /// or timeout_seconds (0 = no timeout). Same pattern as fetch.
    void wait_settled(int n, double timeout_seconds) {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        timed_out_ = false;
        settled_expected_ = n;
        settled_received_ = 0;
        settled_done_ = (n <= 0);
        l.unlock();
        if (n <= 0)
            return;
        if (timeout_seconds > 0) {
            auto ms = static_cast<proton::duration::numeric_type>(timeout_seconds * 1000);
            work_queue_->schedule(proton::duration(ms), [this]() { timeout_fired(); });
        }
        l.lock();
        wait_cv_.wait(l, [this] { return settled_done_ || timed_out_ || interrupt_requested_; });
        settled_expected_ = 0;
    }
    int settled_received_count() const {
        auto l = std::lock_guard(wait_mutex_);
        return settled_received_;
    }

    // Thread-safe: schedule work on the container thread
    void declare() {
        work_queue_->add([this]() { do_declare(); });
    }
    void commit() {
        work_queue_->add([this]() { do_commit(); });
    }
    void abort() {
        work_queue_->add([this]() { do_abort(); });
    }
    void release() {
        work_queue_->add([this]() { do_release(); });
    }
    void send(int n, const std::string& to_addr) {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        send_pending_ = n;
        send_to_addr_ = to_addr.empty() ? addr_ : to_addr;
        l.unlock();
        work_queue_->add([this]() { try_send(); });
        l.lock();
        wait_cv_.wait(l, [this] { return send_pending_ == 0 || interrupt_requested_; });
    }
    void list_unsettled() {
        auto count = std::size_t(0);
        for (auto rcv : session_.receivers()) {
            for (auto d : rcv.unsettled_deliveries()) {
                (void)d;
                ++count;
            }
        }
        std::cout << count << " unsettled delivery(ies)" << std::endl;
        for (auto rcv : session_.receivers()) {
            for (auto d : rcv.unsettled_deliveries()) {
                std::cout << "  " << d.tag() << std::endl;
            }
        }
    }
    void quit() {
        work_queue_->add([this]() { do_quit(); });
    }
};

using command_fn = bool (*)(tx_recv_interactive& recv, const std::vector<std::string>& args);

static bool cmd_declare(tx_recv_interactive& recv, const std::vector<std::string>&) {
    recv.declare();
    return false;
}
static bool cmd_fetch(tx_recv_interactive& recv, const std::vector<std::string>& args) {
    auto n = 1;
    auto timeout_seconds = 0.0;
    if (!args.empty()) {
        try {
            n = std::stoi(args[0]);
            if (n < 1) n = 1;
        } catch (...) {
            std::cout << "fetch: expected positive number, got '" << args[0] << "'" << std::endl;
            return false;
        }
    }
    if (args.size() >= 2) {
        try {
            timeout_seconds = std::stof(args[1]);
            if (timeout_seconds < 0) timeout_seconds = 0;
        } catch (...) {
            std::cout << "fetch: expected timeout in seconds, got '" << args[1] << "'" << std::endl;
            return false;
        }
    }
    recv.fetch(n, timeout_seconds);
    std::cout << "fetch: received " << recv.fetch_received_count() << " message(s)" << std::endl;
    return false;
}
static bool cmd_commit(tx_recv_interactive& recv, const std::vector<std::string>&) {
    recv.commit();
    return false;
}
static bool cmd_abort(tx_recv_interactive& recv, const std::vector<std::string>&) {
    recv.abort();
    return false;
}
static bool cmd_unsettled(tx_recv_interactive& recv, const std::vector<std::string>&) {
    recv.list_unsettled();
    return false;
}
static bool cmd_release(tx_recv_interactive& recv, const std::vector<std::string>&) {
    recv.release();
    return false;
}
static bool cmd_send(tx_recv_interactive& recv, const std::vector<std::string>& args) {
    auto n = 1;
    auto to_addr = std::string();
    if (!args.empty()) {
        try {
            n = std::stoi(args[0]);
            if (n < 1) n = 1;
        } catch (...) {
            std::cout << "send: expected positive number, got '" << args[0] << "'" << std::endl;
            return false;
        }
    }
    if (args.size() >= 2)
        to_addr = args[1];
    recv.send(n, to_addr);
    std::cout << "send: sent " << n << " message(s) to " << (to_addr.empty() ? "(default address)" : to_addr) << std::endl;
    return false;
}
static bool cmd_wait_settled(tx_recv_interactive& recv, const std::vector<std::string>& args) {
    auto n = 1;
    auto timeout_seconds = 0.0;
    if (!args.empty()) {
        try {
            n = std::stoi(args[0]);
            if (n < 0) n = 0;
        } catch (...) {
            std::cout << "wait_settled: expected non-negative count, got '" << args[0] << "'" << std::endl;
            return false;
        }
    }
    if (args.size() >= 2) {
        try {
            timeout_seconds = std::stof(args[1]);
            if (timeout_seconds < 0) timeout_seconds = 0;
        } catch (...) {
            std::cout << "wait_settled: expected timeout in seconds, got '" << args[1] << "'" << std::endl;
            return false;
        }
    }
    recv.wait_settled(n, timeout_seconds);
    std::cout << "wait_settled: " << recv.settled_received_count() << " settlement(s)" << std::endl;
    return false;
}
static bool cmd_sleep(tx_recv_interactive& recv, const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cout << "sleep: expected duration in seconds (e.g. sleep 1.5)" << std::endl;
        return false;
    }
    float seconds;
    try {
        seconds = std::stof(args[0]);
        if (seconds < 0) seconds = 0;
    } catch (...) {
        std::cout << "sleep: expected number of seconds, got '" << args[0] << "'" << std::endl;
        return false;
    }
    recv.sleep(static_cast<double>(seconds));
    return false;
}
static bool cmd_quit(tx_recv_interactive&, const std::vector<std::string>&) {
    return true;
}

static bool cmd_help(tx_recv_interactive&, const std::vector<std::string>&);

struct command_entry {
    const char* name;
    const char* description;
    command_fn fn;
};

// Lexicographically sorted by name for std::lower_bound lookup
static constexpr command_entry COMMAND_TABLE[] = {
    {"abort", "Abort the current transaction", cmd_abort},
    {"commit", "Commit the current transaction", cmd_commit},
    {"declare", "Start a transaction", cmd_declare},
    {"fetch", "Receive n messages (optional timeout in seconds)", cmd_fetch},
    {"help", "Show this list of commands", cmd_help},
    {"quit", "Exit the program", cmd_quit},
    {"release", "Release all unsettled deliveries", cmd_release},
    {"send", "Send n messages to queue (optional to address; use <none> to omit)", cmd_send},
    {"sleep", "Sleep for given seconds", cmd_sleep},
    {"unsettled", "List unsettled deliveries", cmd_unsettled},
    {"wait_settled", "Wait for n disposition updates (optional timeout)", cmd_wait_settled},
};
static constexpr std::size_t COMMAND_TABLE_SIZE = sizeof(COMMAND_TABLE) / sizeof(COMMAND_TABLE[0]);

static bool cmd_help(tx_recv_interactive&, const std::vector<std::string>&) {
    for (std::size_t i = 0; i < COMMAND_TABLE_SIZE; ++i) {
        std::cout << "  " << COMMAND_TABLE[i].name << " - " << COMMAND_TABLE[i].description << "\n";
    }
    return false;
}

/// Split a string into words (by whitespace).
static std::vector<std::string> split_args(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream is(line);
    for (std::string word; is >> word;) out.push_back(word);
    return out;
}

/// Parsed command: first element is command name, remaining elements are arguments.
using parsed_command = std::vector<std::string>;

/// Parse a line into zero or more commands separated by ';'. Each segment is
/// split into words (command name + args). Empty segments are skipped.
/// Returns a sequence that can be iterated to run each command.
static std::vector<parsed_command> parse_command_line(const std::string& line) {
    std::vector<parsed_command> commands;
    std::istringstream is(line);
    for (std::string segment; std::getline(is, segment, ';');) {
        auto args = split_args(segment);
        if (!args.empty())
            commands.push_back(std::move(args));
    }
    return commands;
}

static const command_entry* find_command(std::string_view name) {
    auto it = std::lower_bound(std::begin(COMMAND_TABLE), std::end(COMMAND_TABLE), name,
        [](const command_entry& e, std::string_view s) {
            return std::string_view(e.name) < s;
        });
    if (it != std::end(COMMAND_TABLE) && std::string_view(it->name) == name)
        return &*it;
    return nullptr;
}

static bool execute_command(tx_recv_interactive& recv, const command_entry& cmd, const std::vector<std::string>& args) {
    auto cmd_args = std::vector<std::string>(args.begin() + 1, args.end());
    return cmd.fn(recv, cmd_args);
}

#if defined(_WIN32) || defined(_WIN64)
static void block_interrupt() {
}

static HANDLE g_ctrl_c_event = nullptr;

static BOOL WINAPI win_ctrl_handler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT && g_ctrl_c_event != nullptr)
        SetEvent(g_ctrl_c_event);
    return TRUE;
}

static void interrupt_thread(tx_recv_interactive* recv) {
    HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (ev == nullptr)
        return;
    g_ctrl_c_event = ev;
    if (!SetConsoleCtrlHandler(win_ctrl_handler, TRUE)) {
        CloseHandle(ev);
        return;
    }
    for (;;) {
        if (WaitForSingleObject(ev, INFINITE) == WAIT_OBJECT_0 && recv != nullptr) {
            recv->request_interrupt();
            ResetEvent(ev);
        }
    }
}
#else
static void block_interrupt() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, nullptr);
}

static void interrupt_thread(tx_recv_interactive* recv) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    for (;;) {
        int sig = 0;
        if (sigwait(&set, &sig) == 0 && sig == SIGINT && recv != nullptr)
            recv->request_interrupt();
    }
}
#endif

int main(int argc, char** argv) {
    block_interrupt();

    auto conn_url = std::string("//127.0.0.1:5672");
    auto addr = std::string("examples");
    auto initial_commands = std::string();
    auto opts = example::options(argc, argv);

    opts.add_value(conn_url, 'u', "url", "connection URL", "URL");
    opts.add_value(addr, 'a', "address", "address to receive messages from", "ADDR");
    opts.add_value(initial_commands, 'c', "commands", "commands to run before interactive mode (e.g. declare; fetch 2; quit)", "COMMANDS");

    try {
        opts.parse();

        auto recv = tx_recv_interactive(conn_url, addr);
        auto container = proton::container(recv);
        auto container_thread = std::thread([&container]() { container.run(); });

        std::thread interrupt_th(interrupt_thread, &recv);
        interrupt_th.detach();

        recv.wait_ready();

        auto line = initial_commands;
        bool quit_requested = false;
        while (!quit_requested) {
            if (line.empty()) {
                std::cout << "> " << std::flush;
                if (!std::getline(std::cin, line))
                    break;
            }
            for (const auto& args : parse_command_line(line)) {
                auto* cmd = find_command(args[0]);
                if (cmd) {
                    if (execute_command(recv, *cmd, args)) {
                        quit_requested = true;
                        break;
                    }
                    if (recv.interrupted()) {
                        std::cout << "[interrupted]" << std::endl;
                        recv.clear_interrupt();
                    }
                    if (recv.timed_out()) {
                        std::cout << "[timed out]" << std::endl;
                        recv.clear_timed_out();
                    }
                    recv.sync_with_connection_thread();
                    if (std::string err; recv.take_last_error(err)) {
                        std::cout << "[error: " << err << "]" <<std::endl;
                    }
                } else {
                    std::cout << "Unknown command. Type 'help' for a list of commands." << std::endl;
                }
            }
            line.clear();
        }

        recv.quit();
        container_thread.join();
        return 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 1;
}
