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
#include <proton/error_condition.hpp>
#include <proton/message.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/receiver.hpp>
#include <proton/receiver_options.hpp>
#include <proton/sender.hpp>
#include <proton/sender_options.hpp>
#include <proton/session.hpp>
#include <proton/target_options.hpp>
#include <proton/transport.hpp>
#include <proton/tracker.hpp>
#include <proton/transfer.hpp>
#include <proton/types.hpp>
#include <proton/work_queue.hpp>

#include <proton/logger.h>
#include <algorithm>
#include <any>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <ostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#else
#  include <csignal>
#endif

using namespace std::literals;

// Sentinel queue name: omit message 'to' address so the peer may reject the message.
inline constexpr auto NO_TO_ADDRESS = "<none>"sv;

/// After `quit()`, block the main thread up to this long for `on_container_stop` (container `auto_stop`
/// after the connection closes) before `container.stop()` in the destructor.
inline constexpr auto CONTAINER_STOP_GRACE = 1000ms;

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace script {

/// Shared origin for relative timestamps across deferred output groups.
class logging_epoch {
    using steady_clock = std::chrono::steady_clock;
    using milliseconds = std::chrono::milliseconds;

    static inline const auto program_epoch_ = steady_clock::now();

    mutable std::mutex mutex_;
    std::decay_t<decltype(program_epoch_)> epoch_{program_epoch_};

public:
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        epoch_ = steady_clock::now();
    }

    auto offset_ms() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::chrono::duration_cast<milliseconds>(steady_clock::now() - epoch_);
    }
};

/// Thread-safe queue of timestamped text lines (one of several deferred output groups sharing a `logging_epoch`).
class logging_buffer {
    logging_epoch& epoch_;
    mutable std::mutex mutex_;
    std::vector<std::string> lines_;

public:
    static std::string timestamp_prefix(std::chrono::milliseconds offset) {
        return "+" + std::to_string(offset.count()) + "ms: ";
    }

    explicit logging_buffer(logging_epoch& epoch) : epoch_(epoch) {}

    void append_line(std::string line) {
        std::lock_guard<std::mutex> lock(mutex_);
        lines_.push_back(std::move(line));
    }

    void append_line(std::string_view line) {
        append_line(std::string(line));
    }

    void append_timestamped_line(std::string line) {
        append_timestamped_line(epoch_.offset_ms(), std::move(line));
    }

    void append_timestamped_line(std::chrono::milliseconds offset, std::string line) {
        std::string stamped;
        stamped.reserve(16 + line.size());
        stamped += timestamp_prefix(offset);
        stamped += std::move(line);
        append_line(std::move(stamped));
    }

    void flush(std::ostream& out) {
        std::vector<std::string> batch;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            batch.swap(lines_);
        }
        for (const auto& line : batch) {
            out << line << '\n';
        }
    }
};

struct pending {}; /// Returned only by blocking `wait` subcommands.
struct exit {};
struct ok {};
struct status_only{}; /// The commmand did not perform an action.
struct interrupted {};
struct timed_out {};
struct proton_err {
    std::string what;
};
/// Script-layer failure: unknown command or variable.
struct script_error {
    std::string what;
};
/// Failed `expect` assertion.
struct assertion_success {};
struct assertion_failure {
    std::string message;
};

using outcome =
    std::variant<ok, pending, interrupted, timed_out, proton_err, exit, script_error, status_only, assertion_success, assertion_failure>;

inline std::ostream& operator<<(std::ostream& os, const outcome& c) {
    std::visit(
        overloaded{
            [&](const pending&) { os << "pending"; },
            [&](const exit&) { os << "exit"; },
            [&](const ok&) { os << "ok"; },
            [&](const status_only&) { os << "status_only"; },
            [&](const interrupted&) { os << "interrupted"; },
            [&](const timed_out&) { os << "timed_out"; },
            [&](const proton_err& e) { os << "proton_error: " << e.what; },
            [&](const script_error& e) { os << "script_error: " << e.what; },
            [&](const assertion_success&) { os << "assertion_success"; },
            [&](const assertion_failure& a) { os << "assertion_failure: " << a.message; },
        },
        c);
    return os;
}

/// True when `run_line`'s outcome should end the REPL (`quit` or failed `expect`).
inline bool should_exit(const outcome& o) noexcept {
    return std::holds_alternative<exit>(o) || std::holds_alternative<assertion_failure>(o);
}

inline bool scripting_only(const script::outcome& o) {
    return
        std::holds_alternative<script::exit>(o) ||
        std::holds_alternative<script::script_error>(o) ||
        std::holds_alternative<script::status_only>(o) ||
        std::holds_alternative<script::assertion_success>(o) ||
        std::holds_alternative<script::assertion_failure>(o);
}

/// Variable name (without `$`) and expanded value for each `$name` token in a command line.
using variable_substitutions = std::vector<std::pair<std::string, std::string>>;

struct cmd_result_log {
    std::chrono::milliseconds offset;
    std::string command;
    variable_substitutions substitutions;
    outcome result;
};

class host {
  public:
    virtual ~host() = default;

    virtual outcome command_hook(class runner& runner, outcome from_command) { return from_command; }
    virtual outcome line_hook(class runner& runner, outcome from_line) { return from_line; }
    virtual void interrupt_hook(class runner& runner) {}
};

struct command_entry {
    const char* name;
    const char* description;
    outcome (*fn)(runner& r, const std::vector<std::string_view>& params);
};

/// Same ordering as `std::string_view(a) < std::string_view(b)` for null-terminated names (C++17-safe constexpr).
constexpr int command_name_compare(const char* a, const char* b) noexcept {
    while (*a != '\0' && *b != '\0') {
        if (*a < *b) return -1;
        if (*a > *b) return 1;
        ++a;
        ++b;
    }
    if (*a == '\0' && *b == '\0') return 0;
    return *a == '\0' ? -1 : 1;
}

template <std::size_t N>
constexpr bool command_names_sorted(const command_entry (&table)[N]) noexcept {
    for (std::size_t i = 1; i < N; ++i) {
        if (command_name_compare(table[i - 1].name, table[i].name) >= 0) return false;
    }
    return true;
}

/// Parses and dispatches `;`-separated command lines (REPL, `-c` initial script, completion log).
/// The concrete app is held in `std::any` (this example stores `tx_tester&`); `app<App>()` performs `any_cast`.
/// Commands are free functions receiving `script::runner&`.
/// Per-command and per-line policy (variable refresh, container sync, interrupt) lives in `host` hooks.
class runner {
  public:
    template <typename App>
    explicit runner(App& app, logging_buffer& output, host& h):
        app_(std::ref(app)), output_(output), host_(h) {}

    /// Forwarded from the host process (e.g. SIGINT handler thread); calls `host::interrupt_hook`.
    void request_interrupt() { host_.interrupt_hook(*this); }

    template <typename App>
    App& app() {
        return std::any_cast<std::reference_wrapper<App>>(app_);
    }

    template <typename App>
    const App& app() const {
        return std::any_cast<std::reference_wrapper<App>>(app_);
    }

    /// Runs one input line as a command list. Returns the last command's outcome for this line
    /// (empty line → `ok{}`). Use `should_exit` to decide if the REPL should stop (`exit` or `assertion_failure`).
    template <int N>
    outcome run_line(std::string_view line, const command_entry(&table)[N], logging_epoch& epoch);

    /// Stores a script variable (use as `$name` in commands). Value is substituted verbatim for each token.
    void set_variable(std::string name, std::string value);

    /// Sorted variables as `name = quoted(value)` entries. After each pair except the last, appends `between`
    /// (default newline). Callers add any leading indent (e.g. `status` uses `"  "` + `between` `"\n  "`).
    std::string format_variables(std::string_view between = "\n") const;

    void log(std::string line) { output_.append_timestamped_line(std::move(line)); }

  private:
    void flush_result_log();
    static std::string join_command(const std::vector<std::string_view>& command);

    static std::string format_result(const std::string& command,
        const variable_substitutions& substitutions, const outcome& outcome);

    void record_outcome(logging_epoch& epoch, const std::vector<std::string_view>& command,
        variable_substitutions substitutions, outcome state);

    outcome execute_command(const std::vector<std::string_view>& command, const command_entry* table,
        std::size_t table_size);

    /// Split `text` on any character in `delimiters`; empty segments are dropped.
    static std::vector<std::string_view> split_tokens(std::string_view text, std::string_view delimiters);
    /// One input line may contain several `;`-separated commands; each command is whitespace-separated tokens.
    /// Text from `#` to the end of a segment is ignored; segments that are empty or comment-only are skipped.
    static std::vector<std::vector<std::string_view>> commands_from_line(std::string_view line);

    /// Tokens beginning with `$` (rest is the variable name) are replaced with stored values; other tokens unchanged.
    /// On success returns `ok{}` (outputs only); on failure returns `script_error` (`out` / `expanded_storage`
    /// and `substitutions` may contain entries from tokens processed before the failure).
    /// Each successful `$name` substitution appends `{name, value}` to `substitutions` (cleared at entry; on
    /// `script_error`, entries from tokens expanded before the failure are left in `substitutions`).
    outcome expand_variable_tokens(const std::vector<std::string_view>& tokens,
        std::vector<std::string>& expanded_storage, std::vector<std::string_view>& out,
        variable_substitutions& substitutions) const;

    std::any app_;
    logging_buffer& output_;
    host& host_;
    std::unordered_map<std::string, std::string> variables_;
    mutable std::mutex completion_mutex_;
    std::vector<cmd_result_log> completion_log_;
};

inline void append_quoted_value(std::string& out, const std::string& value) noexcept {
    out.push_back('"');
    for (const char c : value) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    out.push_back('"');
}

std::string runner::format_result(const std::string& command,
    const variable_substitutions& substitutions, const outcome& outcome) {
    std::ostringstream os;
    os << "CMD: " << command;
    if (!substitutions.empty()) {
        os << " [";
        for (std::size_t j = 0; j < substitutions.size(); ++j) {
            if (j) os << ", ";
            const auto& [name, value] = substitutions[j];
            os << name << "=" << std::quoted(value);
        }
        os << ']';
    }
    os << " -> " << outcome;
    return os.str();
}

void runner::set_variable(std::string name, std::string value) {
    variables_[std::move(name)] = std::move(value);
}

std::string runner::format_variables(std::string_view between) const {
    std::vector<std::pair<std::string, std::string>> pairs(variables_.begin(), variables_.end());
    std::sort(pairs.begin(), pairs.end(),
        [](const std::pair<std::string, std::string>& a, const std::pair<std::string, std::string>& b) {
            return a.first < b.first;
        });

    std::string out;
    auto seps = pairs.size()-1;
    for (const auto& [name, value]: pairs) {
        out += name;
        out += "=";
        append_quoted_value(out, value);
        if (seps-- > 0) out.append(between);
    }
    return out;
}

outcome runner::expand_variable_tokens(const std::vector<std::string_view>& tokens,
    std::vector<std::string>& expanded_storage, std::vector<std::string_view>& out,
    variable_substitutions& substitutions) const {
    expanded_storage.clear();
    out.clear();
    substitutions.clear();
    expanded_storage.reserve(tokens.size());
    out.reserve(tokens.size());
    for (std::string_view tok : tokens) {
        if (!tok.empty() && tok.front() == '$') {
            const std::string_view name = tok.substr(1);
            if (name.empty()) {
                return script_error{"empty variable name"};
            }
            if (const auto it = variables_.find(std::string(name)); it != variables_.end()) {
                substitutions.emplace_back(std::string(name), it->second);
                expanded_storage.push_back(it->second);
                out.emplace_back(expanded_storage.back());
            } else {
                return script_error{"unknown variable '" + std::string(name) + "'"};
            }
        } else {
            out.push_back(tok);
        }
    }
    return ok{};
}
std::vector<std::string_view> runner::split_tokens(std::string_view text, std::string_view delimiters) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    const char* const base = text.data();
    const auto n = text.size();
    for (std::size_t pos = 0; pos <= n; ++pos) {
        if (pos < n && delimiters.find(text[pos]) == std::string_view::npos) continue;
        if (pos > start) parts.emplace_back(base + start, pos - start);
        start = pos + 1;
    }
    return parts;
}

static std::string_view strip_comment(std::string_view text) noexcept {
    if (const auto pos = text.find('#'); pos != std::string_view::npos) {
        return text.substr(0, pos);
    }
    return text;
}

std::vector<std::vector<std::string_view>> runner::commands_from_line(std::string_view line) {
    std::vector<std::vector<std::string_view>> command_list;
    for (std::string_view raw : split_tokens(line, ";")) {
        if (auto tokens = split_tokens(strip_comment(raw), " \n\t"); !tokens.empty()) {
            command_list.push_back(std::move(tokens));
        }
    }
    return command_list;
}

std::string runner::join_command(const std::vector<std::string_view>& command) {
    std::string s;
    for (std::size_t i = 0; i < command.size(); ++i) {
        if (i) s += ' ';
        s.append(command[i].data(), command[i].size());
    }
    return s;
}

const script::command_entry* find_command(std::string_view name, const command_entry* table, std::size_t n) {
    auto it = std::lower_bound(table, table + n, name, [](const script::command_entry& e, std::string_view s) {
        return std::string_view(e.name) < s;
    });
    if (it != table + n && std::string_view(it->name) == name) return it;
    return nullptr;
}

outcome script::runner::execute_command(
    const std::vector<std::string_view>& command, const script::command_entry* table, std::size_t table_size) {
    if (command.size() < 1) {
        return script::script_error{"empty command"};
    }
    if (const auto* cmd = find_command(command[0], table, table_size); !cmd) {
        return script::script_error{
            "unknown command '" + std::string(command[0]) + "'. Type 'help' for a list of commands."};
    } else {
        script::outcome from_command = cmd->fn(*this, std::vector<std::string_view>(command.begin() + 1, command.end()));
        return host_.command_hook(*this, from_command);
    }
}

void runner::record_outcome(logging_epoch& epoch, const std::vector<std::string_view>& command,
    variable_substitutions substitutions, outcome state) {
    const auto offset = epoch.offset_ms();
    auto l = std::lock_guard(completion_mutex_);
    completion_log_.push_back(
        cmd_result_log{offset, join_command(command), std::move(substitutions), std::move(state)});
}

void runner::flush_result_log() {
    std::vector<cmd_result_log> rows;
    {
        auto l = std::lock_guard(completion_mutex_);
        if (completion_log_.empty()) {
            return;
        }
        rows = std::move(completion_log_);
        completion_log_.clear();
    }
    for (const auto& row : rows) {
        output_.append_timestamped_line(
            row.offset, format_result(row.command, row.substitutions, row.result));
    }
}

template <int N>
outcome runner::run_line(std::string_view line, const command_entry(&table)[N], logging_epoch& epoch) {
    outcome result = ok{};
    const auto command_list = commands_from_line(line);
    for (const auto& command : command_list) {
        std::vector<std::string> expanded_storage;
        std::vector<std::string_view> substituted;
        variable_substitutions substitutions;
        if (const outcome expand_result =
                expand_variable_tokens(command, expanded_storage, substituted, substitutions);
            std::holds_alternative<script_error>(expand_result)) {
            std::ostringstream os;
            os << expand_result;
            log(os.str());
            record_outcome(epoch, command, std::move(substitutions), std::move(expand_result));
            continue;
        }
        const outcome completion = execute_command(substituted, table, N);
        record_outcome(epoch, command, std::move(substitutions), completion);
        if (should_exit(completion)) {
            result = completion;
            break;
        }
    }
    flush_result_log();
    return host_.line_hook(*this, result);
}

} // namespace script

// Interactive tester for AMQP transactions: declare/commit/abort transactions, receive
// messages via `fetch` / `wait fetched`, `wait incoming` / `wait outgoing` for settlements, `send` then `wait sent`
// until queued sends reach sender_.send; default or a given queue (or omit 'to'
// for rejection testing), wait for declare completion or disposition updates, and
// accept/reject/modify/release unsettled. Type 'help' for commands.
//
// Script variables ($name) are refreshed after each command; use `status` and `expect` to assert them.
// Run against python/examples/broker.py using the build-tree venv Python, e.g.:
//   ${BLD}/python/pytest_env/bin/python python/examples/broker.py -t 2
// (plain `python` lacks the proton binding unless the venv is activated).

class tx_tester : public proton::messaging_handler {
  private:
    enum class txn_discharge { none, committed, aborted };
    enum class txn_declare_result { none, ok, error };

    static const char* txn_discharge_string(txn_discharge d) noexcept {
        switch (d) {
          case txn_discharge::none:
            return "none";
          case txn_discharge::committed:
            return "committed";
          case txn_discharge::aborted:
            return "aborted";
        }
        return "none";
    }

    static const char* txn_declare_result_string(txn_declare_result r) noexcept {
        switch (r) {
          case txn_declare_result::none:
            return "none";
          case txn_declare_result::ok:
            return "ok";
          case txn_declare_result::error:
            return "error";
        }
        return "none";
    }

    static std::string binary_script_string(const proton::binary& id) {
        std::ostringstream os;
        os << id;
        const std::string s = os.str();
        // proton::binary operator<< formats as b"<payload>"; strip wrapper for script variables.
        if (s.size() >= 3 && s[0] == 'b' && s[1] == '"' && s.back() == '"') {
            return s.substr(2, s.size() - 3);
        }
        return s;
    }

    std::string connection_url_;
    std::string receiver_addr_;

    proton::container* container_ = nullptr;
    proton::connection connection_;
    proton::receiver receiver_;
    proton::sender sender_;
    proton::session session_;
    proton::work_queue* work_queue_ = nullptr;
    script::logging_buffer& output_;

    void log(std::string line) { output_.append_timestamped_line(std::move(line)); }

    template <typename... Args>
    void log_format(Args&&... args) {
        std::ostringstream os;
        (os << ... << std::forward<Args>(args));
        log(os.str());
    }

    mutable std::mutex wait_mutex_;
    std::condition_variable wait_cv_;
    std::string last_error_;
    std::string send_to_addr_;
    int send_pending_ = 0;
    int send_next_id_ = 0;
    int fetch_expected_ = 0;
    int fetch_received_ = 0;
    int messages_received_ = 0;
    int settled_expected_ = 0;
    int settled_received_ = 0;
    int outgoing_expected_ = 0;
    int outgoing_received_ = 0;
    bool outgoing_done_ = false;
    bool session_ready_ = false;
    bool timed_out_ = false;
    proton::work_handle wait_timeout_handle_ = 0;
    bool wait_timeout_scheduled_ = false;
    bool interrupt_requested_ = false;
    bool fetch_done_ = false;
    bool settled_done_ = false;
    bool declared_done_ = false;
    bool discharged_done_ = false;
    bool container_stopped_ = false;

    txn_discharge last_discharge_ = txn_discharge::none;
    txn_declare_result last_declare_ = txn_declare_result::none;
    proton::binary callback_txn_id_;
    int provisional_accept_ = 0;
    int provisional_reject_ = 0;
    int provisional_release_ = 0;
    int tracker_accept_ = 0;
    int tracker_reject_ = 0;
    int tracker_release_ = 0;
    int tracker_settle_ = 0;

    std::mutex sync_mutex_;
    std::condition_variable sync_cv_;
    bool sync_done_ = false;

    /// Run f() in a try block; on error, record the message for later reporting.
    template <typename F>
    void catch_any_error(F&& f) {
        try {
            f();
        } catch (const std::exception& e) {
            auto l = std::lock_guard(wait_mutex_);
            last_error_ = e.what();
            wait_cv_.notify_all();
        }
    }

    /// True when a blocking `wait` should return (timeout, interrupt, or recorded endpoint error).
    /// Call only with `wait_mutex_` held.
    bool wait_end_requested() const noexcept {
        return timed_out_ || interrupt_requested_ || !last_error_.empty();
    }

    /// Format a proton `error_condition` for `take_last_error` / `$error`.
    static std::string format_endpoint_error(std::string_view context, const proton::error_condition& ec) {
        std::string msg;
        msg.reserve(context.size() + 2 + ec.what().size());
        msg.append(context);
        msg.append(": ");
        msg.append(ec.what());
        return msg;
    }

    void do_declare() {
        {
            auto l = std::lock_guard(wait_mutex_);
            declared_done_ = false;
            last_discharge_ = txn_discharge::none;
        }
        catch_any_error([this]() { session_.transaction_declare(); });
    }

    void do_fetch(int n) {
        receiver_.add_credit(n);
    }

    void do_commit() {
        {
            auto l = std::lock_guard(wait_mutex_);
            discharged_done_ = false;
        }
        catch_any_error([this]() { session_.transaction_commit(); });
    }

    void do_abort() {
        {
            auto l = std::lock_guard(wait_mutex_);
            discharged_done_ = false;
        }
        catch_any_error([this]() { session_.transaction_abort(); });
    }

    void do_release() {
        for (auto rcv : session_.receivers()) {
            for (auto d : rcv.unsettled_deliveries()) {
                d.release();
            }
        }
    }

    void do_accept() {
        for (auto rcv : session_.receivers()) {
            for (auto d : rcv.unsettled_deliveries()) {
                d.accept();
            }
        }
    }

    void do_reject() {
        for (auto rcv : session_.receivers()) {
            for (auto d : rcv.unsettled_deliveries()) {
                d.reject();
            }
        }
    }

    void do_modify() {
        for (auto rcv : session_.receivers()) {
            for (auto d : rcv.unsettled_deliveries()) {
                d.modify();
            }
        }
    }

    int do_unsettled_delivery_count() {
        int n = 0;
        for (auto rcv : session_.receivers()) {
            for (auto d : rcv.unsettled_deliveries()) {
                ++n;
            }
        }
        return n;
    }

    int do_unsettled_tracker_count() {
        int n = 0;
        for (auto snd : session_.senders()) {
            for (auto t : snd.unsettled_trackers()) {
                ++n;
            }
        }
        return n;
    }

    void do_incoming() {
        log(std::to_string(do_unsettled_delivery_count()) + " unsettled delivery(ies)");
        for (auto rcv : session_.receivers()) {
            for (auto d : rcv.unsettled_deliveries()) {
                log_format("  ", d.tag());
            }
        }
    }

    void do_outgoing() {
        log(std::to_string(do_unsettled_tracker_count()) + " unsettled tracker(s)");
        for (auto snd : session_.senders()) {
            for (auto t : snd.unsettled_trackers()) {
                log_format("  ", t.tag());
            }
        }
    }

    void do_quit() {
        connection_.close();
    }

    void do_send(int add_count, std::string to_addr) {
        {
            auto l = std::lock_guard(wait_mutex_);
            interrupt_requested_ = false;
            send_pending_ += add_count;
            send_to_addr_ = std::move(to_addr);
        }
        try_send();
    }

    void on_container_start(proton::container& c) override {
        container_ = &c;
        c.enable_quiescent_callback(true);
        c.connect(connection_url_);
    }

    void on_container_quiescent(proton::container&) override {
        auto l = std::lock_guard(sync_mutex_);
        sync_done_ = true;
        sync_cv_.notify_all();
    }

    void on_container_stop(proton::container&) override {
        {
            auto l = std::lock_guard(wait_mutex_);
            container_stopped_ = true;
            wait_cv_.notify_all();
        }
        {
            auto l = std::lock_guard(sync_mutex_);
            sync_done_ = true;
            sync_cv_.notify_all();
        }
    }

    void on_connection_open(proton::connection& conn) override {
        connection_ = conn;
        work_queue_ = &conn.work_queue();
        // credit_window(0) so we control flow via "fetch", explicit acknowledgement
        receiver_ = conn.open_receiver(receiver_addr_, proton::receiver_options().credit_window(0).auto_accept(false));
        sender_ = conn.open_sender("", proton::sender_options{}.target(proton::target_options{}.anonymous(true)));
    }

    void on_sender_open(proton::sender& s) override {
        sender_ = s;
    }

    void on_session_open(proton::session& s) override {
        session_ = s;
        auto l = std::lock_guard(wait_mutex_);
        session_ready_ = true;
        wait_cv_.notify_all();
    }

    void on_session_transaction_declared(proton::session& s) override {
        log_format("transaction declared: ", s.transaction_id());
        auto l = std::lock_guard(wait_mutex_);
        callback_txn_id_ = s.transaction_id();
        declared_done_ = true;
        last_declare_ = txn_declare_result::ok;
        wait_cv_.notify_all();
    }

    void on_session_transaction_committed(proton::session& s) override {
        log_format("transaction committed: ", s.transaction_id());
        auto l = std::lock_guard(wait_mutex_);
        callback_txn_id_ = s.transaction_id();
        discharged_done_ = true;
        last_discharge_ = txn_discharge::committed;
        wait_cv_.notify_all();
    }

    void on_session_transaction_aborted(proton::session& s) override {
        const auto ec = s.transaction_error();
        log_format("transaction aborted: ", s.transaction_id(), " (", ec.what(), ")");
        auto l = std::lock_guard(wait_mutex_);
        callback_txn_id_ = s.transaction_id();
        discharged_done_ = true;
        last_discharge_ = txn_discharge::aborted;
        if (!ec.empty()) {
            last_error_ = format_endpoint_error("transaction", ec);
        }
        wait_cv_.notify_all();
    }

    void on_session_transaction_error(proton::session& s) override {
        const auto ec = s.transaction_error();
        log_format("transaction error: ", ec.what());
        auto l = std::lock_guard(wait_mutex_);
        callback_txn_id_ = s.transaction_id();
        declared_done_ = true;
        last_declare_ = txn_declare_result::error;
        if (!ec.empty()) {
            last_error_ = format_endpoint_error("transaction", ec);
        }
        wait_cv_.notify_all();
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
            if (to_addr != NO_TO_ADDRESS) {
                msg.to(to_addr);
            }
            msg.body(std::map<std::string, int>{{"message", send_next_id_}});
            sender_.send(msg);
            ++send_next_id_;
            ++sent;
            --to_send;
        }
        if (sent > 0) {
            auto l = std::lock_guard(wait_mutex_);
            send_pending_ -= sent;
            wait_cv_.notify_all();
        }
    }

    void on_message(proton::delivery& d, proton::message& msg) override {
        log_format(d.tag(), ": ", msg.body());
        auto l = std::lock_guard(wait_mutex_);
        ++messages_received_;
        if (fetch_expected_ > 0) {
            ++fetch_received_;
            if (fetch_received_ >= fetch_expected_) {
                fetch_done_ = true;
            }
        }
        // Pre-settled deliveries never trigger on_delivery_settle; count them here for `wait incoming`
        if (settled_expected_ > 0 && d.settled()) {
            ++settled_received_;
            if (settled_received_ >= settled_expected_) {
                settled_done_ = true;
            }
        }
        wait_cv_.notify_all();
    }

    void on_delivery_settle(proton::delivery&) override {
        auto l = std::lock_guard(wait_mutex_);
        if (settled_expected_ > 0) {
            ++settled_received_;
            if (settled_received_ >= settled_expected_) {
                settled_done_ = true;
            }
        }
        wait_cv_.notify_all();
    }

    void on_transactional_accept(proton::tracker& t) override {
        log_format("tracker: ", t.tag(), " provisional accepted:  (txn: ", t.session().transaction_id(), ")");
        auto l = std::lock_guard(wait_mutex_);
        ++provisional_accept_;
    }

    void on_transactional_reject(proton::tracker& t) override {
        log_format("tracker: ", t.tag(), " provisional rejected:  (txn: ", t.session().transaction_id(), ")");
        auto l = std::lock_guard(wait_mutex_);
        ++provisional_reject_;
    }

    void on_transactional_release(proton::tracker& t) override {
        log_format("tracker: ", t.tag(), " provisional released:  (txn: ", t.session().transaction_id(), ")");
        auto l = std::lock_guard(wait_mutex_);
        ++provisional_release_;
    }

    void on_tracker_accept(proton::tracker& t) override {
        log_format("tracker: ", t.tag(), " accepted: ");
        auto l = std::lock_guard(wait_mutex_);
        ++tracker_accept_;
    }

    void on_tracker_reject(proton::tracker& t) override {
        log_format("tracker: ", t.tag(), " rejected: ");
        auto l = std::lock_guard(wait_mutex_);
        ++tracker_reject_;
    }

    void on_tracker_release(proton::tracker& t) override {
        log_format("tracker: ", t.tag(), " released: ");
        auto l = std::lock_guard(wait_mutex_);
        ++tracker_release_;
    }

    void on_tracker_settle(proton::tracker& t) override {
        log_format("tracker: ", t.tag(), " settled: ", t.state());
        auto l = std::lock_guard(wait_mutex_);
        ++tracker_settle_;
        if (outgoing_expected_ > 0) {
            ++outgoing_received_;
            if (outgoing_received_ >= outgoing_expected_) {
                outgoing_done_ = true;
            }
        }
        wait_cv_.notify_all();
    }

    void on_session_error(proton::session& s) override {
        const auto ec = s.error();
        log_format("Session error: ", ec.what());
        s.connection().close();
        auto l = std::lock_guard(wait_mutex_);
        if (!ec.empty()) {
            last_error_ = format_endpoint_error("session", ec);
        }
        wait_cv_.notify_all();
    }

    /// Default `messaging_handler::on_transport_error` throws `proton::error`; record for `$error` instead.
    void on_transport_error(proton::transport& t) override {
        const auto ec = t.error();
        log_format("transport error: ", ec.what());
        auto l = std::lock_guard(wait_mutex_);
        if (!ec.empty()) {
            last_error_ = format_endpoint_error("transport", ec);
        }
        wait_cv_.notify_all();
    }

    void on_connection_error(proton::connection& c) override {
        const auto ec = c.error();
        log_format("connection error: ", ec.what());
        c.close();
        auto l = std::lock_guard(wait_mutex_);
        if (!ec.empty()) {
            last_error_ = format_endpoint_error("connection", ec);
        }
        wait_cv_.notify_all();
    }

    void on_receiver_error(proton::receiver& l) override {
        const auto ec = l.error();
        log_format("receiver error: ", ec.what());
        l.connection().close();
        auto lk = std::lock_guard(wait_mutex_);
        if (!ec.empty()) {
            last_error_ = format_endpoint_error("receiver", ec);
        }
        wait_cv_.notify_all();
    }

    void on_sender_error(proton::sender& l) override {
        const auto ec = l.error();
        log_format("sender error: ", ec.what());
        l.connection().close();
        auto lk = std::lock_guard(wait_mutex_);
        if (!ec.empty()) {
            last_error_ = format_endpoint_error("sender", ec);
        }
        wait_cv_.notify_all();
    }

    /// Run f on the connection thread; block the caller until it finishes.
    template <typename F>
    void run_on_connection_thread(F&& f) {
        if (!work_queue_) {
            auto l = std::lock_guard(wait_mutex_);
            if (last_error_.empty()) {
                last_error_ = "connection not ready";
            }
            wait_cv_.notify_all();
            return;
        }
        std::mutex done_mutex;
        std::condition_variable done_cv;
        bool done = false;
        work_queue_->add([&f, &done_mutex, &done_cv, &done]() {
            f();
            auto l = std::lock_guard(done_mutex);
            done = true;
            done_cv.notify_one();
        });
        std::unique_lock l(done_mutex);
        done_cv.wait(l, [&done] { return done; });
    }

  public:
    tx_tester(const std::string& url, const std::string& addr, script::logging_buffer& script_output)
        : connection_url_(url), receiver_addr_(addr), output_(script_output) {}

    /// Called from the SIGINT-handling thread to wake any current wait.
    void request_interrupt() {
        auto l = std::lock_guard(wait_mutex_);
        interrupt_requested_ = true;
        wait_cv_.notify_all();
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
        wait_cv_.wait(l, [this] { return session_ready_ || wait_end_requested(); });
    }

    /// Wait until the container thread is quiescent (all queued work and events
    /// processed). Use before showing the prompt so the command loop stays in
    /// sync with background container-thread activity.
    script::outcome sync_with_container_thread(script::outcome from_command) {
        {
            auto wl = std::lock_guard(wait_mutex_);
            if (container_stopped_) {
                return script::exit{};
            }
        }
        std::unique_lock<std::mutex> l(sync_mutex_);
        sync_done_ = false;
        if (!sync_cv_.wait_for(l, CONTAINER_STOP_GRACE, [this] { return sync_done_; })) {
            log("...");
        }
        return from_command;
    }

    /// Cancel the active wait timeout (`cmd::wait` calls this when the wait finishes).
    void cancel_wait_timeout() {
        auto l = std::lock_guard(wait_mutex_);
        if (!wait_timeout_scheduled_ || !container_) {
            return;
        }
        container_->cancel(wait_timeout_handle_);
        wait_timeout_scheduled_ = false;
    }

    /// If `seconds` > 0, schedule a timeout on the container after that delay (`cmd::wait` only).
    void schedule_wait_timeout(double seconds) {
        if (seconds <= 0) {
            return;
        }
        auto l = std::unique_lock(wait_mutex_);
        timed_out_ = false;
        auto ms = seconds * 1000;
        wait_timeout_handle_ = container_->schedule(proton::duration(ms), [this]() {
            auto l = std::unique_lock(wait_mutex_);
            wait_timeout_scheduled_ = false;
            timed_out_ = true;
            wait_cv_.notify_all();
        });
        wait_timeout_scheduled_ = true;
    }

    /// Block until a container-scheduled wait timeout or interrupt.
    void sleep() {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        wait_cv_.wait(l, [this] { return wait_end_requested(); });
    }

    /// Issue receiver credit for n messages (does not block).
    void add_credit(int n) {
        work_queue_->add([this, n]() { do_fetch(n); });
    }

    /// Wait until n messages are received. Timeout is scheduled by `wait fetched` via
    /// `schedule_wait_timeout` before this runs. Does not add credit (use `fetch` first).
    void wait_fetched(int n) {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        fetch_expected_ = n;
        fetch_received_ = 0;
        fetch_done_ = (n <= 0);
        wait_cv_.wait(l, [this] { return fetch_done_ || wait_end_requested(); });
        fetch_expected_ = 0;
    }
    int fetch_received_count() const {
        auto l = std::lock_guard(wait_mutex_);
        return fetch_received_;
    }
    /// Return the message-received total and reset it to zero.
    int take_messages_received_count() {
        auto l = std::lock_guard(wait_mutex_);
        const int n = messages_received_;
        messages_received_ = 0;
        return n;
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

    /// If a handler- or API-recorded error message is pending, assign it to `out`, clear it, and return true.
    bool take_last_error(std::string& out) {
        auto l = std::lock_guard(wait_mutex_);
        if (last_error_.empty()) {
            return false;
        }
        out = std::move(last_error_);
        last_error_.clear();
        return true;
    }

    /// Count unsettled incoming deliveries (for `wait incoming` default).
    int unsettled_incoming_delivery_count() {
        int n = 0;
        run_on_connection_thread([this, &n]() { n = do_unsettled_delivery_count(); });
        return n;
    }

    void wait_incoming(int n) {
        auto l = std::unique_lock(wait_mutex_);
        settled_expected_ = n;
        settled_received_ = 0;
        settled_done_ = (n <= 0);
        interrupt_requested_ = false;
        if (settled_done_) {
            settled_done_ = false;
            return;
        }
        wait_cv_.wait(l, [this] { return settled_done_ || wait_end_requested(); });
        settled_done_ = false;
        settled_expected_ = 0;
    }
    int settled_received_count() const {
        auto l = std::lock_guard(wait_mutex_);
        return settled_received_;
    }

    void wait_declared() {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        if (declared_done_) {
            declared_done_ = false;
            return;
        }
        wait_cv_.wait(l, [this] { return declared_done_ || wait_end_requested(); });
        declared_done_ = false;
    }

    /// Wait until commit or abort completes (`on_session_transaction_committed` or
    /// `on_session_transaction_aborted`), or timeout / SIGINT. `discharged_done_` is cleared in
    /// `do_commit` / `do_abort` before the operation is sent.
    void wait_discharged() {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        if (discharged_done_) {
            discharged_done_ = false;
            return;
        }
        wait_cv_.wait(l, [this] { return discharged_done_ || wait_end_requested(); });
        discharged_done_ = false;
    }

    // Run synchronously on the connection thread (see `run_on_connection_thread`).
    void declare() {
        run_on_connection_thread([this]() { do_declare(); });
    }
    void commit() {
        run_on_connection_thread([this]() { do_commit(); });
    }
    void abort() {
        run_on_connection_thread([this]() { do_abort(); });
    }
    void release() {
        run_on_connection_thread([this]() { do_release(); });
    }
    void accept() {
        run_on_connection_thread([this]() { do_accept(); });
    }
    void reject() {
        run_on_connection_thread([this]() { do_reject(); });
    }
    void modify() {
        run_on_connection_thread([this]() { do_modify(); });
    }
    /// Queue `n` more messages for sending (subject to link credit). Merges with any pending count on the connection
    /// thread (`send_pending_ += n`, `send_to_addr_` overwritten per batch — best-effort for overlapping sends).
    void queue_send(int n, const std::string& to_addr) {
        std::string addr = to_addr.empty() ? receiver_addr_ : to_addr;
        work_queue_->add([this, n, addr = std::move(addr)]() mutable { do_send(n, std::move(addr)); });
    }

    int unsettled_outgoing_tracker_count() {
        int n = 0;
        run_on_connection_thread([this, &n]() { n = do_unsettled_tracker_count(); });
        return n;
    }

    /// Wait until all queued `send` work has been passed to `sender_.send` (see `try_send`),
    /// or timeout / interrupt. Not related to tracker settlement.
    void wait_queued_send_complete() {
        auto l = std::unique_lock(wait_mutex_);
        interrupt_requested_ = false;
        wait_cv_.wait(l, [this] { return send_pending_ == 0 || wait_end_requested(); });
    }

    /// Wait for `n` outgoing tracker settlements (`on_tracker_settle`), or timeout / interrupt.
    void wait_outgoing(int n) {
        auto l = std::unique_lock(wait_mutex_);
        outgoing_expected_ = n;
        outgoing_received_ = 0;
        outgoing_done_ = (n <= 0);
        interrupt_requested_ = false;
        if (outgoing_done_) {
            outgoing_done_ = false;
            outgoing_expected_ = 0;
            return;
        }
        wait_cv_.wait(l, [this] { return outgoing_done_ || wait_end_requested(); });
        outgoing_done_ = false;
        outgoing_expected_ = 0;
    }

    int outgoing_received_count() const {
        auto l = std::lock_guard(wait_mutex_);
        return outgoing_received_;
    }

    int send_pending_count() const {
        auto l = std::lock_guard(wait_mutex_);
        return send_pending_;
    }

    int messages_received_count() const {
        auto l = std::lock_guard(wait_mutex_);
        return messages_received_;
    }

    bool transaction_is_declared() {
        bool declared = false;
        run_on_connection_thread([this, &declared]() { declared = session_.transaction_is_declared(); });
        return declared;
    }

    proton::binary transaction_id() {
        proton::binary id;
        run_on_connection_thread([this, &id]() { id = session_.transaction_id(); });
        return id;
    }

    /// Reset lifecycle flags and disposition counters (script variables refreshed separately).
    void reset_script_status() {
        auto l = std::lock_guard(wait_mutex_);
        last_discharge_ = txn_discharge::none;
        last_declare_ = txn_declare_result::none;
        callback_txn_id_ = proton::binary{};
        provisional_accept_ = 0;
        provisional_reject_ = 0;
        provisional_release_ = 0;
        tracker_accept_ = 0;
        tracker_reject_ = 0;
        tracker_release_ = 0;
        tracker_settle_ = 0;
    }

    /// Publish handler state as script variables (use as `$name` in commands and `expect`).
    void publish_script_variables(script::runner& runner) {
        txn_discharge last_discharge = txn_discharge::none;
        txn_declare_result last_declare = txn_declare_result::none;
        proton::binary callback_txn_id;
        int messages = 0;
        int fetch = 0;
        int settled = 0;
        int outgoing = 0;
        int send_pending = 0;
        int provisional_accept = 0;
        int provisional_reject = 0;
        int provisional_release = 0;
        int tracker_accept = 0;
        int tracker_reject = 0;
        int tracker_release = 0;
        int tracker_settle = 0;
        {
            auto l = std::lock_guard(wait_mutex_);
            last_discharge = last_discharge_;
            last_declare = last_declare_;
            callback_txn_id = callback_txn_id_;
            messages = messages_received_;
            fetch = fetch_received_;
            settled = settled_received_;
            outgoing = outgoing_received_;
            send_pending = send_pending_;
            provisional_accept = provisional_accept_;
            provisional_reject = provisional_reject_;
            provisional_release = provisional_release_;
            tracker_accept = tracker_accept_;
            tracker_reject = tracker_reject_;
            tracker_release = tracker_release_;
            tracker_settle = tracker_settle_;
        }
        const int unsettled_in = unsettled_incoming_delivery_count();
        const int unsettled_out = unsettled_outgoing_tracker_count();
        const bool txn_declared = transaction_is_declared();
        const proton::binary txn_id = transaction_id();

        runner.set_variable("last_discharge", txn_discharge_string(last_discharge));
        runner.set_variable("last_declare", txn_declare_result_string(last_declare));
        runner.set_variable("txn_id", binary_script_string(txn_id));
        runner.set_variable("callback_txn_id", binary_script_string(callback_txn_id));
        runner.set_variable("txn_declared", txn_declared ? "true" : "false");
        runner.set_variable("messages", std::to_string(messages));
        runner.set_variable("fetch", std::to_string(fetch));
        runner.set_variable("settled", std::to_string(settled));
        runner.set_variable("outgoing", std::to_string(outgoing));
        runner.set_variable("unsettled_in", std::to_string(unsettled_in));
        runner.set_variable("unsettled_out", std::to_string(unsettled_out));
        runner.set_variable("send_pending", std::to_string(send_pending));
        runner.set_variable("provisional_accept", std::to_string(provisional_accept));
        runner.set_variable("provisional_reject", std::to_string(provisional_reject));
        runner.set_variable("provisional_release", std::to_string(provisional_release));
        runner.set_variable("tracker_accept", std::to_string(tracker_accept));
        runner.set_variable("tracker_reject", std::to_string(tracker_reject));
        runner.set_variable("tracker_release", std::to_string(tracker_release));
        runner.set_variable("tracker_settle", std::to_string(tracker_settle));
    }

    void incoming() {
        run_on_connection_thread([this]() { do_incoming(); });
    }
    void outgoing() {
        run_on_connection_thread([this]() { do_outgoing(); });
    }
    void quit() {
        {
            auto l = std::lock_guard(wait_mutex_);
            container_stopped_ = false;
        }
        work_queue_->add([this]() { do_quit(); });
    }

    /// After `quit()`, wait up to `CONTAINER_STOP_GRACE` for `on_container_stop` (typically once
    /// `auto_stop` stops the container after the connection closes). If the deadline passes, return anyway
    /// so `container.stop()` can run.
    void wait_container_stop() {
        std::unique_lock<std::mutex> lk(wait_mutex_);
        if (container_stopped_) {
            return;
        }
        if (!wait_cv_.wait_for(lk, CONTAINER_STOP_GRACE, [this] { return container_stopped_; })) {
            std::cerr << "quit: waited " << CONTAINER_STOP_GRACE.count()
                      << "ms for container stop; on_container_stop did not arrive, continuing shutdown\n";
        }
    }
};

namespace cmd {

std::optional<int> parse_int(std::string_view s) noexcept {
    try {
        return std::stoi(std::string(s));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> try_parse_nonneg_double_token(std::string_view s) noexcept {
    try {
        std::size_t idx = 0;
        if (const double v = std::stod(std::string(s), &idx); idx == s.size()) {
            return std::max(0.0, v);
        }
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::string_view> arg(const std::vector<std::string_view>& a, size_t i) {
    if (i >= a.size()) return std::nullopt;
    return std::string_view(a[i]);
}

bool expect_int(script::runner& runner, const char* cmd, const char* name,
    std::optional<std::string_view> text, int& out, int if_absent, int at_least) {
    if (!text) {
        out = if_absent;
        return true;
    }
    if (auto n = parse_int(*text)) {
        out = std::max(at_least, *n);
        return true;
    }
    std::ostringstream os;
    os << cmd << ": invalid " << name << " '" << *text << "'";
    runner.log(os.str());
    return false;
}

script::outcome abort(script::runner& runner, const std::vector<std::string_view>&) {
    runner.app<tx_tester>().abort();
    return script::ok{};
}
script::outcome accept(script::runner& runner, const std::vector<std::string_view>&) {
    runner.app<tx_tester>().accept();
    return script::ok{};
}
script::outcome commit(script::runner& runner, const std::vector<std::string_view>&) {
    runner.app<tx_tester>().commit();
    return script::ok{};
}
script::outcome declare(script::runner& runner, const std::vector<std::string_view>&) {
    runner.app<tx_tester>().declare();
    return script::ok{};
}
script::outcome expect(script::runner& runner, const std::vector<std::string_view>& params) {
    constexpr auto k_not = "@not"sv;
    constexpr auto k_empty = "@empty"sv;

    std::size_t i = 0;
    const bool neg = !params.empty() && params[0] == k_not;
    if (neg) ++i;

    const std::size_t n = params.size() - i;
    bool result = false;

    // No more arguments - treat as true
    if (n==0) {
        result = true;
    } else if (const auto head = params[i]; !head.empty() && head.front() == '@') {
        // Any token beginning with `@` here is a predicate name;
        if (head == k_empty) {
            if (params.size() < i + 2) {
                return script::script_error{"expect: @empty requires one argument"};
            }
            const std::string_view v = params[i + 1];
            result = v.empty();
        } else {
            const std::string err = "unknown expect predicate " + std::string(head);
            return script::script_error{err};
        }
    } else if (n==1) {
        // Treat a single argument as a boolean value.
        result = params[i] == "true";
    } else if (n >= 2) {
        result = (params[i] == params[i + 1]);
    }
    if (result == !neg) return script::assertion_success{}; // ok if result is opposite of neg - only status no action
    return script::assertion_failure{std::string("[") + runner.format_variables(", ") + "]"};
}
script::outcome incoming(script::runner& runner, const std::vector<std::string_view>&) {
    runner.app<tx_tester>().incoming();
    return script::ok{};
}
script::outcome modify(script::runner& runner, const std::vector<std::string_view>&) {
    runner.app<tx_tester>().modify();
    return script::ok{};
}
script::outcome received(script::runner& runner, const std::vector<std::string_view>&) {
    runner.log("received: " + std::to_string(runner.app<tx_tester>().take_messages_received_count())
        + " message(s)");
    return script::status_only{};
}
script::outcome quit(script::runner&, const std::vector<std::string_view>&) {
    return script::exit{};
}
script::outcome reject(script::runner& runner, const std::vector<std::string_view>&) {
    runner.app<tx_tester>().reject();
    return script::ok{};
}
script::outcome release(script::runner& runner, const std::vector<std::string_view>&) {
    runner.app<tx_tester>().release();
    return script::ok{};
}
script::outcome reset_status(script::runner& runner, const std::vector<std::string_view>&) {
    runner.set_variable("error", "");
    runner.set_variable("interrupted", "false");
    runner.set_variable("timedout", "false");
    runner.app<tx_tester>().reset_script_status();
    runner.app<tx_tester>().publish_script_variables(runner);
    return script::status_only{};
}
script::outcome outgoing(script::runner& runner, const std::vector<std::string_view>&) {
    runner.app<tx_tester>().outgoing();
    return script::ok{};
}
script::outcome send(script::runner& runner, const std::vector<std::string_view>& params) {
    int n = 0;
    if (!expect_int(runner, "send", "message count (positive integer)", arg(params, 0), n, 1, 1)) {
        return script::ok{};
    }
    std::string to_addr;
    if (params.size() >= 2) to_addr = std::string{params[1]};
    runner.app<tx_tester>().queue_send(n, to_addr);
    runner.log("send: queued " + std::to_string(n) + " message(s) to "
        + (to_addr.empty() ? "(default address)" : to_addr));
    return script::ok{};
}

script::outcome status(script::runner& runner, const std::vector<std::string_view>&) {
    runner.log(std::string("  ") + runner.format_variables("\n  "));
    return script::status_only{};
}

script::outcome wait_sleep(script::runner& runner, const std::vector<std::string_view>&) {
    runner.app<tx_tester>().sleep();
    return script::pending{};
}
script::outcome wait_declared(script::runner& runner, const std::vector<std::string_view>&) {
    runner.app<tx_tester>().wait_declared();
    return script::pending{};
}
script::outcome wait_discharged(script::runner& runner, const std::vector<std::string_view>&) {
    runner.app<tx_tester>().wait_discharged();
    return script::pending{};
}
script::outcome wait_sent(script::runner& runner, const std::vector<std::string_view>&) {
    runner.app<tx_tester>().wait_queued_send_complete();
    return script::pending{};
}
script::outcome fetch(script::runner& runner, const std::vector<std::string_view>& params) {
    int n = 0;
    if (!expect_int(runner, "fetch", "credit count (positive integer)", arg(params, 0), n, 1, 1)) {
        return script::ok{};
    }
    runner.app<tx_tester>().add_credit(n);
    runner.log("fetch: added credit for " + std::to_string(n) + " message(s)");
    return script::ok{};
}
script::outcome wait_fetched(script::runner& runner, const std::vector<std::string_view>& params) {
    int n = 0;
    if (!expect_int(runner, "wait fetched", "message count (positive integer)", arg(params, 0), n, 1, 1)) {
        return script::ok{};
    }
    runner.app<tx_tester>().wait_fetched(n);
    runner.log("wait fetched: received "
        + std::to_string(runner.app<tx_tester>().fetch_received_count()) + " message(s)");
    return script::pending{};
}
script::outcome wait_incoming(script::runner& runner, const std::vector<std::string_view>& params) {
    int n = 0;
    if (!arg(params, 0)) {
        n = runner.app<tx_tester>().unsettled_incoming_delivery_count();
    } else {
        if (!expect_int(runner, "wait incoming", "settlement count (non-negative integer)", arg(params, 0), n, 0, 0)) {
            return script::ok{};
        }
    }
    runner.app<tx_tester>().wait_incoming(n);
    runner.log("wait incoming: "
        + std::to_string(runner.app<tx_tester>().settled_received_count()) + " settlement(s)");
    return script::pending{};
}
script::outcome wait_outgoing(script::runner& runner, const std::vector<std::string_view>& params) {
    int n = 0;
    if (!arg(params, 0)) {
        n = runner.app<tx_tester>().unsettled_outgoing_tracker_count();
    } else {
        if (!expect_int(runner, "wait outgoing", "settlement count (non-negative integer)", arg(params, 0), n, 0, 0)) {
            return script::ok{};
        }
    }
    runner.app<tx_tester>().wait_outgoing(n);
    runner.log("wait outgoing: "
        + std::to_string(runner.app<tx_tester>().outgoing_received_count()) + " tracker settlement(s)");
    return script::pending{};
}

script::outcome wait_help(script::runner& runner, const std::vector<std::string_view>&);

inline constexpr script::command_entry wait_command_table[] = {
    {"declared", "Wait for declare", wait_declared},
    {"discharged", "Wait for commit or abort to finish", wait_discharged},
    {"fetched", "Wait for n messages to arrive (default: 1)", wait_fetched},
    {"help", "Show wait subcommands", wait_help},
    {"incoming", "Wait for n incoming delivery settlements (default: unsettled incoming count)", wait_incoming},
    {"outgoing", "Wait for n outgoing tracker settlements (default: unsettled outgoing count)", wait_outgoing},
    {"sent", "Wait for queued send work to be sent (default: link credit)", wait_sent},
};
static_assert(script::command_names_sorted(wait_command_table),
    "wait_command_table must be strictly sorted by name (find_command uses std::lower_bound)");

script::outcome wait_help(script::runner& runner, const std::vector<std::string_view>&) {
    runner.log(std::string(
        "wait [timeout] <subcommand> ... — optional leading timeout (seconds) applies to that wait;\n"
        "bare `wait` or `wait <timeout>` sleeps until timeout or interrupt;"));
    constexpr std::size_t n = sizeof(wait_command_table) / sizeof(wait_command_table[0]);
    for (std::size_t i = 0; i < n; ++i) {
        runner.log("  " + std::string(wait_command_table[i].name) + " - " + wait_command_table[i].description);
    }
    return script::status_only{};
}

/// `wait` sub-dispatch and outcome folding (interrupt / timeout / error vs `Pending` from blocking subcommands).
script::outcome wait(script::runner& runner, const std::vector<std::string_view>& params) {
    auto pit = params.begin();
    double timeout_seconds = 0.0;
    if (pit != params.end()) {
        if (auto t = try_parse_nonneg_double_token(*pit)) {
            timeout_seconds = *t;
            ++pit;
        }
    }

    script::outcome (*fn)(script::runner&, const std::vector<std::string_view>&) = nullptr;
    if (pit == params.end()) {
        fn = wait_sleep;
    } else if (const auto* wcmd =
                   script::find_command(*pit, wait_command_table, std::size(wait_command_table));
               wcmd) {
        fn = wcmd->fn;
        ++pit;
    } else {
        std::string expl = "wait: unknown subcommand (";
        const char* sep = "";
        constexpr std::size_t wn = sizeof(wait_command_table) / sizeof(wait_command_table[0]);
        for (std::size_t i = 0; i < wn; ++i) {
            expl += sep;
            expl += wait_command_table[i].name;
            sep = ", ";
        }
        expl += ')';
        return script::script_error{std::move(expl)};
    }

    runner.app<tx_tester>().schedule_wait_timeout(timeout_seconds);
    auto result = fn(runner, std::vector<std::string_view>{pit, params.end()});
    runner.app<tx_tester>().cancel_wait_timeout();
    return result;
}

script::outcome help(script::runner&, const std::vector<std::string_view>&);

inline constexpr script::command_entry command_table[] = {
    {"abort", "Abort the current transaction", abort},
    {"accept", "Accept all unsettled deliveries", accept},
    {"commit", "Commit the current transaction", commit},
    {"declare", "Start a transaction (not again until commit or abort)", declare},
    {"expect", "Script assertion; failure exits the REPL", expect},
    {"fetch", "Add receiver credit for n messages (default 1)", fetch},
    {"help", "Show this list of commands", help},
    {"incoming", "List unsettled incoming deliveries", incoming},
    {"modify", "Modify all unsettled deliveries", modify},
    {"outgoing", "List unsettled outgoing trackers", outgoing},
    {"quit", "Exit the program", quit},
    {"received", "Print messages received since last 'received', then reset counter", received},
    {"reject", "Reject all unsettled deliveries", reject},
    {"release", "Release all unsettled deliveries", release},
    {"reset_status", "Clear $error, $interrupted, $timedout; reset txn/disposition script variables", reset_status},
    {"send", "Queue n messages to send (optional to address; use <none> to omit)", send},
    {"status", "Show script variable names and values", status},
    {"wait",
        "Block until an event: wait [timeout] declared | discharged | fetched n | incoming [n] | outgoing [n] | sent | help",
        wait},
};
static_assert(script::command_names_sorted(command_table),
    "command_table must be strictly sorted by name (find_command uses std::lower_bound)");

script::outcome help(script::runner& runner, const std::vector<std::string_view>&) {
    constexpr std::size_t n = sizeof(command_table) / sizeof(command_table[0]);
    for (std::size_t i = 0; i < n; ++i) {
        runner.log("  " + std::string(command_table[i].name) + " - " + command_table[i].description);
    }
    runner.log(
        "Script variables (refreshed after each command; use with expect and status):\n"
        "  $error $interrupted $timedout — wait outcome\n"
        "  $txn_declared $last_declare $last_discharge $txn_id $callback_txn_id — transaction lifecycle\n"
        "  $messages $fetch $settled $outgoing $unsettled_in $unsettled_out $send_pending — flow\n"
        "  $provisional_accept $provisional_reject $provisional_release — transactional tracker events\n"
        "  $tracker_accept $tracker_reject $tracker_release $tracker_settle — final tracker events");
    return script::status_only{};
}

class tx_tester_host : public script::host {
  public:
    explicit tx_tester_host(tx_tester& app, bool interactive): app_(app), interactive_(interactive) {}

    script::outcome command_hook(script::runner& runner, script::outcome from_command) override {
        // On entry the only possible outcomes from commands are currently:
        // ok{} which is the usual completion status of non waiting commands, however there could have been an error that occured
        //      so that should be translated to proton_err{}.
        // pending{} which is the outcome of waiting commands, which could be interrupted{}, timed_out{} or proton_err{}, or ok{}
        //      if the wait was successful.
        // exit{} which currently only comes from the exit command and tells the loop to exit.
        // assertion_failure{} which currently only comes from the expect command and is also currently treated as a main loop exit.
        // 
        // status_only{} which comes from commands that take no action and so there is no need to sync with the event loop.
        // script_err{} can occur if the command is invalid in some way or a variable doesn't exist.
        // 
        // Currently proton_err{}, interrupted{}, timed_out{} are never returned directly from a command and must be synthesised here.
        //

        if (scripting_only(from_command)) {
            return from_command;
        }

        needs_sync = true;

        std::string err;
        const bool has_err = app_.take_last_error(err);

        app_.publish_script_variables(runner);

        if (has_err) {
            runner.set_variable("error", err);
            return script::proton_err{std::move(err)};
        }

        // At this point only ok{} and pending{} are left as possibilities.
        if (!std::holds_alternative<script::pending>(from_command)) {
            return from_command;
        }

        const bool interrupted = app_.interrupted();
        const bool timed_out = app_.timed_out();

        app_.clear_interrupt();
        app_.clear_timed_out();

        if (interrupted) {
            runner.set_variable("interrupted", "true");
            return script::interrupted{};
        }
        if (timed_out) {
            runner.set_variable("timedout", "true");
            return script::timed_out{};
        }
        return script::ok{};
    }

    script::outcome line_hook(script::runner&, script::outcome from_line) override {
        if (interactive_ && needs_sync) {
            auto result = app_.sync_with_container_thread(from_line);
            needs_sync = false;
            return result;
        }
        return from_line;
    }

    void interrupt_hook(script::runner&) override { app_.request_interrupt(); }

  private:
    tx_tester& app_;
    bool interactive_;
    bool needs_sync = false;
};

} // namespace cmd

#if defined(_WIN32) || defined(_WIN64)
static void block_interrupt() {
}

static HANDLE g_ctrl_c_event = nullptr;

static BOOL WINAPI win_ctrl_handler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT && g_ctrl_c_event != nullptr) {
        SetEvent(g_ctrl_c_event);
    }
    return TRUE;
}

static void interrupt_thread(script::runner& runner) {
    HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (ev == nullptr) {
        return;
    }
    g_ctrl_c_event = ev;
    if (!SetConsoleCtrlHandler(win_ctrl_handler, TRUE)) {
        CloseHandle(ev);
        return;
    }
    for (;;) {
        if (WaitForSingleObject(ev, INFINITE) == WAIT_OBJECT_0) {
            runner.request_interrupt();
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

static void interrupt_thread(script::runner& runner) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    for (;;) {
        int sig = 0;
        if (sigwait(&set, &sig) == 0 && sig == SIGINT) {
            runner.request_interrupt();
        }
    }
}
#endif

static void library_log_sink(intptr_t context, pn_log_subsystem_t sub, pn_log_level_t sev,
                             const char* message) {
    script::logging_buffer* output_buffer = reinterpret_cast<script::logging_buffer*>(context);
    std::ostringstream line;
    line << '[' << pn_logger_subsystem_name(sub) << "]:"
         << pn_logger_level_name(sev) << ": " << (message ? message : "");
    output_buffer->append_timestamped_line(line.str());
}

void install_log_sink(script::logging_buffer& output_buffer) {
    pn_logger_set_log_sink(pn_default_logger(), library_log_sink, reinterpret_cast<intptr_t>(&output_buffer));
}

std::string interactive_prompt() {
    std::cout << "> " << std::flush;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

/// Runs `container.run()` on a worker thread. Destructor calls `container.stop()` (no-op if already
/// stopping or if `run()` has already returned) then `join()`. For orderly shutdown, `main` should call
/// `recv.quit()`, then `recv.wait_container_stop_after_quit()`, so `auto_stop` can end `run()` before `stop()`.
struct proton_container_runner {
    proton::container& container;
    std::thread thread;

    proton_container_runner(const proton_container_runner&) = delete;
    proton_container_runner& operator=(const proton_container_runner&) = delete;
    proton_container_runner(proton_container_runner&&) = delete;
    proton_container_runner& operator=(proton_container_runner&&) = delete;

    explicit proton_container_runner(proton::container& c):
        container(c), 
        thread([this]() { container.run(); }) 
    {}

    ~proton_container_runner() {
        container.stop();
        if (thread.joinable())
            thread.join();
    }
};

int main(int argc, char** argv) {
    block_interrupt();

    auto conn_url = std::string("//127.0.0.1:5672");
    auto addr = std::string("examples");
    auto initial_command_list = std::string();
    auto command_file = std::string();
    auto opts = example::options(argc, argv);

    opts.add_value(conn_url, 'u', "url", "connection URL", "URL");
    opts.add_value(addr, 'a', "address", "address to receive messages from", "ADDR");
    opts.add_value(initial_command_list, 'c', "commands",
        "command list to run before interactive mode (e.g. declare; fetch 2; wait fetched 2; quit)", "COMMANDS");
    opts.add_value(command_file, 'f', "file", "read commands from FILE ('-' for stdin; no interactive mode)", "FILE");

    try {
        opts.parse();

        std::ifstream command_file_stream;
        std::istream* command_input = nullptr;
        if (!command_file.empty()) {
            if (command_file == "-") {
                command_input = &std::cin;
            } else {
                command_file_stream.open(command_file);
                if (!command_file_stream) {
                    throw std::runtime_error("cannot open command file '" + command_file + "'");
                }
                command_input = &command_file_stream;
            }
        }
        
        script::logging_epoch epoch;
        script::logging_buffer script_output{epoch};

        install_log_sink(script_output);

        auto recv = tx_tester(conn_url, addr, script_output);
        auto container = proton::container(recv);
        proton_container_runner runner(container);

        cmd::tx_tester_host tester_host{recv, command_input == nullptr};
        script::runner script_runner{recv, script_output, tester_host};
        std::thread interrupt_th(interrupt_thread, std::ref(script_runner));
        interrupt_th.detach();

        recv.wait_ready();
        if (std::string startup_err; recv.take_last_error(startup_err)) {
            std::cerr << startup_err << std::endl;
            return 1;
        }

        cmd::reset_status(script_runner, {}); // Set up initial status values
        script::outcome outcome;
        auto line = initial_command_list;
        for (;;) {
            outcome = script_runner.run_line(line, cmd::command_table, epoch);
            script_output.flush(std::cout);
            if (script::should_exit(outcome)) break;
            if (command_input) {
                if (!std::getline(*command_input, line))
                    break;
            } else {
                line = interactive_prompt();
                if (!std::cin) break;
                epoch.reset();
            }
        }

        recv.quit();
        recv.wait_container_stop();
        script_output.flush(std::cout);
        return std::holds_alternative<script::assertion_failure>(outcome) ? 1 : 0;
    } catch (const example::bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::bad_any_cast& e) {
        std::cerr << "Internal error: script runner app type mismatch (bad any_cast). " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
    }

    return 1;
}
