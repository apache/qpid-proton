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

#include <proton/controller.hpp>
#include <proton/work_queue.hpp>
#include <proton/url.hpp>

#include <proton/io/connection_engine.hpp>
#include <proton/io/default_controller.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <set>
#include <sstream>
#include <system_error>

// Linux native IO
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

// This is the only public function, the implementation is private.
std::unique_ptr<proton::controller> make_epoll_controller();

// Private implementation
namespace  {


using lock_guard = std::lock_guard<std::mutex>;

// Get string from errno
std::string errno_str(const std::string& msg) {
    return std::system_error(errno, std::system_category(), msg).what();
}

// Throw proton::error(errno_str(msg)) if result < 0
int check(int result, const std::string& msg) {
    if (result < 0)
        throw proton::error(errno_str(msg));
    return result;
}

// Wrapper for getaddrinfo() that cleans up in destructor.
class unique_addrinfo {
  public:
    unique_addrinfo(const std::string& addr) : addrinfo_(0) {
        proton::url u(addr);
        int result = ::getaddrinfo(char_p(u.host()), char_p(u.port()), 0, &addrinfo_);
        if (result)
            throw proton::error(std::string("bad address: ") + gai_strerror(result));
    }
    ~unique_addrinfo() { if (addrinfo_) ::freeaddrinfo(addrinfo_); }

    ::addrinfo* operator->() const { return addrinfo_; }

  private:
    static const char* char_p(const std::string& s) { return s.empty() ? 0 : s.c_str(); }
    ::addrinfo *addrinfo_;
};

// File descriptor wrapper that calls ::close in destructor.
class unique_fd {
  public:
    unique_fd(int fd) : fd_(fd) {}
    ~unique_fd() { if (fd_ >= 0) ::close(fd_); }
    operator int() const { return fd_; }
    int release() { int ret = fd_; fd_ = -1; return ret; }

  protected:
    int fd_;
};

class pollable;
class pollable_engine;
class pollable_listener;

class epoll_controller : public proton::controller {
  public:
    epoll_controller();
    ~epoll_controller();

    // Implemenet the proton::controller interface
    void connect(const std::string& addr,
                 proton::handler& h,
                 const proton::connection_options& opts) override;

    void listen(const std::string& addr,
                std::function<proton::handler*(const std::string&)> factory,
                const proton::connection_options& opts) override;

    void stop_listening(const std::string& addr) override;

    void options(const proton::connection_options& opts) override;
    proton::connection_options options() override;

    void run() override;

    void stop_on_idle() override;
    void stop(const proton::error_condition& err) override;
    void wait() override;

    // Functions used internally.

    void add_engine(proton::handler* h, proton::connection_options opts, int fd);
    void erase(pollable*);

  private:
    void idle_check(const lock_guard&);
    void interrupt();

    const unique_fd epoll_fd_;
    const unique_fd interrupt_fd_;

    mutable std::mutex lock_;

    proton::connection_options options_;
    std::map<std::string, std::unique_ptr<pollable_listener> > listeners_;
    std::map<pollable*, std::unique_ptr<pollable_engine> > engines_;

    std::condition_variable stopped_;
    std::atomic<size_t> threads_;
    bool stopping_;
    proton::error_condition stop_err_;
};

// Base class for pollable file-descriptors. Manages epoll interaction,
// subclasses implement virtual work() to do their serialized work.
class pollable {
  public:
    pollable(int fd, int epoll_fd) : fd_(fd), epoll_fd_(epoll_fd), notified_(false), working_(false)
    {
        int flags = check(::fcntl(fd, F_GETFL, 0), "non-blocking");
        check(::fcntl(fd, F_SETFL,  flags | O_NONBLOCK), "non-blocking");
        ::epoll_event ev = {0};
        ev.data.ptr = this;
        ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd_, &ev);
    }

    virtual ~pollable() {
        ::epoll_event ev = {0};
        ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd_, &ev); // Ignore errors.
    }

    bool do_work(uint32_t events) {
        {
            lock_guard g(lock_);
            if (working_)
                return true;         // Another thread is already working.
            working_ = true;
            notified_ = false;
        }
        uint32_t new_events = work(events);  // Serialized, outside the lock.
        if (new_events) {
            lock_guard g(lock_);
            rearm(notified_ ?  EPOLLIN|EPOLLOUT : new_events);
        }
        return new_events;
    }

    // Called by work_queue to notify that there are jobs.
    void notify() {
        lock_guard g(lock_);
        if (!notified_) {
            notified_ = true;
            if (!working_) // No worker thread, rearm now.
                rearm(EPOLLIN|EPOLLOUT);
        }
    }

  protected:

    // Subclass implements  work.
    // Returns epoll events to re-enable or 0 if finished.
    virtual uint32_t work(uint32_t events) = 0;

    const unique_fd fd_;
    const int epoll_fd_;

  private:

    void rearm(uint32_t events) {
        epoll_event ev;
        ev.data.ptr = this;
        ev.events = EPOLLONESHOT | events;
        check(::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd_, &ev), "re-arm epoll");
        working_ = false;
    }

    std::mutex lock_;
    bool notified_;
    bool working_;
};

class work_queue : public proton::work_queue {
  public:
    typedef std::vector<std::function<void()> > jobs;

    work_queue(pollable& p, proton::controller& c) :
        pollable_(p), closed_(false), controller_(c) {}

    bool push(std::function<void()> f) override {
        // Note this is an unbounded work queue.
        // A resource-safe implementation should be bounded.
        lock_guard g(lock_);
        if (closed_)
            return false;
        jobs_.push_back(f);
        pollable_.notify();
        return true;
    }

    jobs pop_all() {
        lock_guard g(lock_);
        return std::move(jobs_);
    }

    void close() {
        lock_guard g(lock_);
        closed_ = true;
    }

    proton::controller& controller() const override { return controller_; }

  private:
    std::mutex lock_;
    pollable& pollable_;
    jobs jobs_;
    bool closed_;
    proton::controller& controller_;
};

// Handle epoll wakeups for a connection_engine.
class pollable_engine : public pollable {
  public:

    pollable_engine(
        proton::handler* h, proton::connection_options opts, epoll_controller& c,
        int fd, int epoll_fd
    ) : pollable(fd, epoll_fd),
        engine_(*h, opts.link_prefix(std::to_string(fd)+":")),
        queue_(new work_queue(*this, c))
    {
        engine_.work_queue(queue_.get());
    }

    ~pollable_engine() {
        queue_->close();               // No calls to notify() after this.
        engine_.dispatch();            // Run any final events.
        try { write(); } catch(...) {} // Write connection close if we can.
        for (auto f : queue_->pop_all()) {// Run final queued work for side-effects.
            try { f(); } catch(...) {}
        }
    }

    uint32_t work(uint32_t events) {
        try {
            bool can_read = events & EPOLLIN, can_write = events && EPOLLOUT;
            do {
                can_write = can_write && write();
                can_read = can_read && read();
                for (auto f : queue_->pop_all()) // Run queued work
                    f();
                engine_.dispatch();
            } while (can_read || can_write);
            return (engine_.read_buffer().size ? EPOLLIN:0) |
                (engine_.write_buffer().size ? EPOLLOUT:0);
        } catch (const std::exception& e) {
            close(proton::error_condition("exception", e.what()));
        }
        return 0;               // Ending
    }

    void close(const proton::error_condition& err) {
        engine_.connection().close(err);
    }

  private:

    bool write() {
        if (engine_.write_buffer().size) {
            ssize_t n = ::write(fd_, engine_.write_buffer().data, engine_.write_buffer().size);
            while (n == EINTR)
                n = ::write(fd_, engine_.write_buffer().data, engine_.write_buffer().size);
            if (n > 0) {
                engine_.write_done(n);
                return true;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK)
                check(n, "write");
        }
        return false;
    }

    bool read() {
        if (engine_.read_buffer().size) {
            ssize_t n = ::read(fd_, engine_.read_buffer().data, engine_.read_buffer().size);
            while (n == EINTR)
                n = ::read(fd_, engine_.read_buffer().data, engine_.read_buffer().size);
            if (n > 0) {
                engine_.read_done(n);
                return true;
            }
            else if (n == 0)
                engine_.read_close();
            else if (errno != EAGAIN && errno != EWOULDBLOCK)
                check(n, "read");
        }
        return false;
    }

    proton::io::connection_engine engine_;
    std::shared_ptr<work_queue> queue_;
};

// A pollable listener fd that creates pollable_engine for incoming connections.
class pollable_listener : public pollable {
  public:
    pollable_listener(
        const std::string& addr,
        std::function<proton::handler*(const std::string&)> factory,
        int epoll_fd,
        epoll_controller& c,
        const proton::connection_options& opts
    ) :
        pollable(listen(addr), epoll_fd),
        addr_(addr),
        factory_(factory),
        controller_(c),
        opts_(opts)
    {}

    uint32_t work(uint32_t events) {
        if (events & EPOLLRDHUP)
            return 0;
        int accepted = check(::accept(fd_, NULL, 0), "accept");
        controller_.add_engine(factory_(addr_), opts_, accepted);
        return EPOLLIN;
    }

    std::string addr() { return addr_; }

  private:

    int listen(const std::string& addr) {
        std::string msg = "listen on "+addr;
        unique_addrinfo ainfo(addr);
        unique_fd fd(check(::socket(ainfo->ai_family, SOCK_STREAM, 0), msg));
        int yes = 1;
        check(::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)), msg);
        check(::bind(fd, ainfo->ai_addr, ainfo->ai_addrlen), msg);
        check(::listen(fd, 32), msg);
        return fd.release();
    }

    std::string addr_;
    std::function<proton::handler*(const std::string&)> factory_;
    epoll_controller& controller_;
    proton::connection_options opts_;
};


epoll_controller::epoll_controller()
    : epoll_fd_(check(epoll_create(1), "epoll_create")),
      interrupt_fd_(check(eventfd(1, 0), "eventfd")),
      stopping_(false), threads_(0)
{}

epoll_controller::~epoll_controller() {
    try {
        stop(proton::error_condition("exception", "controller shut-down"));
        wait();
    } catch (...) {}
}

void epoll_controller::add_engine(proton::handler* h, proton::connection_options opts, int fd) {
    lock_guard g(lock_);
    if (stopping_)
        throw proton::error("controller is stopping");
    std::unique_ptr<pollable_engine> e(new pollable_engine(h, opts, *this, fd, epoll_fd_));
    e->notify();
    engines_[e.get()] = std::move(e);
}

void epoll_controller::erase(pollable* e) {
    lock_guard g(lock_);
    if (!engines_.erase(e)) {
        pollable_listener* l = dynamic_cast<pollable_listener*>(e);
        if (l)
            listeners_.erase(l->addr());
    }
    idle_check(g);
}

void epoll_controller::idle_check(const lock_guard&) {
    if (stopping_  && engines_.empty() && listeners_.empty())
        interrupt();
}

void epoll_controller::connect(const std::string& addr,
                               proton::handler& h,
                               const proton::connection_options& opts)
{
    std::string msg = "connect to "+addr;
    unique_addrinfo ainfo(addr);
    unique_fd fd(check(::socket(ainfo->ai_family, SOCK_STREAM, 0), msg));
    check(::connect(fd, ainfo->ai_addr, ainfo->ai_addrlen), msg);
    add_engine(&h, options().update(opts), fd.release());
}

void epoll_controller::listen(const std::string& addr,
                              std::function<proton::handler*(const std::string&)> factory,
                              const proton::connection_options& opts)
{
    lock_guard g(lock_);
    if (!factory)
        throw proton::error("null function to listen on "+addr);
    if (stopping_)
        throw proton::error("controller is stopping");
    auto& l = listeners_[addr];
    l.reset(new pollable_listener(addr, factory, epoll_fd_, *this, options_.update(opts)));
    l->notify();
}

void epoll_controller::stop_listening(const std::string& addr) {
    lock_guard g(lock_);
    listeners_.erase(addr);
    idle_check(g);
}

void epoll_controller::options(const proton::connection_options& opts) {
    lock_guard g(lock_);
    options_ = opts;
}

proton::connection_options epoll_controller::options() {
    lock_guard g(lock_);
    return options_;
}

void epoll_controller::run() {
    ++threads_;
    try {
        epoll_event e;
        while(true) {
            check(::epoll_wait(epoll_fd_, &e, 1, -1), "epoll_wait");
            pollable* p = reinterpret_cast<pollable*>(e.data.ptr);
            if (!p)
                break;          // Interrupted
            if (!p->do_work(e.events))
                erase(p);
        }
    } catch (const std::exception& e) {
        stop(proton::error_condition("exception", e.what()));
    }
    if (--threads_ == 0)
        stopped_.notify_all();
}

void epoll_controller::stop_on_idle() {
    lock_guard g(lock_);
    stopping_ = true;
    idle_check(g);
}

void epoll_controller::stop(const proton::error_condition& err) {
    lock_guard g(lock_);
    stop_err_ = err;
    interrupt();
}

void epoll_controller::wait() {
    std::unique_lock<std::mutex> l(lock_);
    stopped_.wait(l, [this]() { return this->threads_ == 0; } );
    for (auto& eng : engines_)
        eng.second->close(stop_err_);
    listeners_.clear();
    engines_.clear();
}

void epoll_controller::interrupt() {
    // Add an always-readable fd with 0 data and no ONESHOT to interrupt all threads.
    epoll_event ev = {0};
    ev.events = EPOLLIN;
    check(epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, interrupt_fd_, &ev), "interrupt");
}

// Register make_epoll_controller() as proton::controller::create().
proton::io::default_controller instance(make_epoll_controller);

}

// This is the only public function.
std::unique_ptr<proton::controller> make_epoll_controller() {
    return std::unique_ptr<proton::controller>(new epoll_controller());
}
