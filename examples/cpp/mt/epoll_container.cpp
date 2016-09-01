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

#include "mt_container.hpp"

#include <proton/default_container.hpp>
#include <proton/event_loop.hpp>
#include <proton/listen_handler.hpp>
#include <proton/url.hpp>

#include <proton/io/container_impl_base.hpp>
#include <proton/io/connection_engine.hpp>
#include <proton/io/link_namer.hpp>

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

#include "../fake_cpp11.hpp"

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

class epoll_container : public proton::io::container_impl_base {
  public:
    epoll_container(const std::string& id);
    ~epoll_container();

    // Pull in base class functions here so that name search finds all the overloads
    using standard_container::stop;
    using standard_container::connect;
    using standard_container::listen;

    proton::returned<proton::connection> connect(
        const std::string& addr, const proton::connection_options& opts) OVERRIDE;

    proton::listener listen(const std::string& addr, proton::listen_handler&) OVERRIDE;

    void stop_listening(const std::string& addr) OVERRIDE;

    void run() OVERRIDE;
    void auto_stop(bool) OVERRIDE;
    void stop(const proton::error_condition& err) OVERRIDE;

    std::string id() const OVERRIDE { return id_; }

    // Functions used internally.
    proton::connection add_engine(proton::connection_options opts, int fd, bool server);
    void erase(pollable*);

    // Link names must be unique per container.
    // Generate unique names with a simple atomic counter.
    class atomic_link_namer : public proton::io::link_namer {
      public:
        std::string link_name() {
            std::ostringstream o;
            o << std::hex << ++count_;
            return o.str();
        }
      private:
        std::atomic<int> count_;
    };

     // FIXME aconway 2016-06-07: Unfinished
    void schedule(proton::duration, std::function<void()>) OVERRIDE { throw std::logic_error("FIXME"); }
    void schedule(proton::duration, proton::void_function0&) OVERRIDE { throw std::logic_error("FIXME"); }
    atomic_link_namer link_namer;

  private:
    template <class T> void store(T& v, const T& x) const { lock_guard g(lock_); v = x; }

    void idle_check(const lock_guard&);
    void interrupt();
    void wait();

    const std::string id_;
    const unique_fd epoll_fd_;
    const unique_fd interrupt_fd_;

    mutable std::mutex lock_;

    proton::connection_options options_;
    std::map<std::string, std::unique_ptr<pollable_listener> > listeners_;
    std::map<pollable*, std::unique_ptr<pollable_engine> > engines_;

    std::condition_variable stopped_;
    bool stopping_;
    proton::error_condition stop_err_;
    std::atomic<size_t> threads_;
};

// Base class for pollable file-descriptors. Manages epoll interaction,
// subclasses implement virtual work() to do their serialized work.
class pollable {
  public:
    pollable(int fd, int epoll_fd) : fd_(fd), epoll_fd_(epoll_fd), notified_(false), working_(false)
    {
        int flags = check(::fcntl(fd, F_GETFL, 0), "non-blocking");
        check(::fcntl(fd, F_SETFL,  flags | O_NONBLOCK), "non-blocking");
        ::epoll_event ev = {};
        ev.data.ptr = this;
        ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd_, &ev);
    }

    virtual ~pollable() {
        ::epoll_event ev = {};
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

    // Called from any thread to wake up the connection handler.
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

class epoll_event_loop : public proton::event_loop {
  public:
    typedef std::vector<std::function<void()> > jobs;

    epoll_event_loop(pollable& p) : pollable_(p), closed_(false) {}

    bool inject(std::function<void()> f) OVERRIDE {
        // Note this is an unbounded work queue.
        // A resource-safe implementation should be bounded.
        lock_guard g(lock_);
        if (closed_)
            return false;
        jobs_.push_back(f);
        pollable_.notify();
        return true;
    }

    bool inject(proton::void_function0& f) OVERRIDE {
        return inject([&f]() { f(); });
    }

    jobs pop_all() {
        lock_guard g(lock_);
        return std::move(jobs_);
    }

    void close() {
        lock_guard g(lock_);
        closed_ = true;
    }

  private:
    std::mutex lock_;
    pollable& pollable_;
    jobs jobs_;
    bool closed_;
};

// Handle epoll wakeups for a connection_engine.
class pollable_engine : public pollable {
  public:
    pollable_engine(epoll_container& c, int fd, int epoll_fd) :
        pollable(fd, epoll_fd),
        loop_(new epoll_event_loop(*this)),
        engine_(c, loop_)
    {
        proton::connection conn = engine_.connection();
        proton::io::set_link_namer(conn, c.link_namer);
    }

    ~pollable_engine() {
        loop_->close();                // No calls to notify() after this.
        engine_.dispatch();            // Run any final events.
        try { write(); } catch(...) {} // Write connection close if we can.
        for (auto f : loop_->pop_all()) {// Run final queued work for side-effects.
            try { f(); } catch(...) {}
        }
    }

    uint32_t work(uint32_t events) {
        try {
            bool can_read = events & EPOLLIN, can_write = events & EPOLLOUT;
            do {
                can_write = can_write && write();
                can_read = can_read && read();
                for (auto f : loop_->pop_all()) // Run queued work
                    f();
                engine_.dispatch();
            } while (can_read || can_write);
            return (engine_.read_buffer().size ? EPOLLIN:0) |
                (engine_.write_buffer().size ? EPOLLOUT:0);
        } catch (const std::exception& e) {
            engine_.disconnected(proton::error_condition("exception", e.what()));
        }
        return 0;               // Ending
    }

    proton::io::connection_engine& engine() { return engine_; }

  private:
    static bool try_again(int e) {
        // These errno values from read or write mean "try again"
        return (e == EAGAIN || e == EWOULDBLOCK || e == EINTR);
    }

    bool write() {
        proton::io::const_buffer wbuf(engine_.write_buffer());
        if (wbuf.size) {
            ssize_t n = ::write(fd_, wbuf.data, wbuf.size);
            if (n > 0) {
                engine_.write_done(n);
                return true;
            } else if (n < 0 && !try_again(errno)) {
                check(n, "write");
            }
        }
        return false;
    }

    bool read() {
        proton::io::mutable_buffer rbuf(engine_.read_buffer());
        if (rbuf.size) {
            ssize_t n = ::read(fd_, rbuf.data, rbuf.size);
            if (n > 0) {
                engine_.read_done(n);
                return true;
            }
            else if (n == 0)
                engine_.read_close();
            else if (!try_again(errno))
                check(n, "read");
        }
        return false;
    }

    // Lifecycle note: loop_ belongs to the proton::connection, which can live
    // longer than the engine if the application holds a reference to it, we
    // disconnect ourselves with loop_->close() in ~connection_engine()
    epoll_event_loop* loop_;
    proton::io::connection_engine engine_;
};

// A pollable listener fd that creates pollable_engine for incoming connections.
class pollable_listener : public pollable {
  public:
    pollable_listener(
        const std::string& addr,
        proton::listen_handler& l,
        int epoll_fd,
        epoll_container& c
    ) :
        pollable(socket_listen(addr), epoll_fd),
        addr_(addr),
        container_(c),
        listener_(l)
    {}

    uint32_t work(uint32_t events) {
        if (events & EPOLLRDHUP) {
            try { listener_.on_close(); } catch (...) {}
            return 0;
        }
        try {
            int accepted = check(::accept(fd_, NULL, 0), "accept");
            container_.add_engine(listener_.on_accept(), accepted, true);
            return EPOLLIN;
        } catch (const std::exception& e) {
            listener_.on_error(e.what());
            return 0;
        }
    }

    std::string addr() { return addr_; }

  private:

    static int socket_listen(const std::string& addr) {
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
    std::function<proton::connection_options(const std::string&)> factory_;
    epoll_container& container_;
    proton::connection_options opts_;
    proton::listen_handler& listener_;
};


epoll_container::epoll_container(const std::string& id)
    : id_(id),                       epoll_fd_(check(epoll_create(1), "epoll_create")),
      interrupt_fd_(check(eventfd(1, 0), "eventfd")),
      stopping_(false), threads_(0)
{}

epoll_container::~epoll_container() {
    try {
        stop(proton::error_condition("exception", "container shut-down"));
        wait();
    } catch (...) {}
}

proton::connection epoll_container::add_engine(proton::connection_options opts, int fd, bool server)
{
    lock_guard g(lock_);
    if (stopping_)
        throw proton::error("container is stopping");
    std::unique_ptr<pollable_engine> eng(new pollable_engine(*this, fd, epoll_fd_));
    if (server)
        eng->engine().accept(opts);
    else
        eng->engine().connect(opts);
    proton::connection c = eng->engine().connection();
    eng->notify();
    engines_[eng.get()] = std::move(eng);
    return c;
}

void epoll_container::erase(pollable* e) {
    lock_guard g(lock_);
    if (!engines_.erase(e)) {
        pollable_listener* l = dynamic_cast<pollable_listener*>(e);
        if (l)
            listeners_.erase(l->addr());
    }
    idle_check(g);
}

void epoll_container::idle_check(const lock_guard&) {
    if (stopping_  && engines_.empty() && listeners_.empty())
        interrupt();
}

proton::returned<proton::connection> epoll_container::connect(
    const std::string& addr, const proton::connection_options& opts)
{
    std::string msg = "connect to "+addr;
    unique_addrinfo ainfo(addr);
    unique_fd fd(check(::socket(ainfo->ai_family, SOCK_STREAM, 0), msg));
    check(::connect(fd, ainfo->ai_addr, ainfo->ai_addrlen), msg);
    return make_thread_safe(add_engine(opts, fd.release(), false));
}

proton::listener epoll_container::listen(const std::string& addr, proton::listen_handler& lh) {
    lock_guard g(lock_);
    if (stopping_)
        throw proton::error("container is stopping");
    auto& l = listeners_[addr];
    try {
        l.reset(new pollable_listener(addr, lh, epoll_fd_, *this));
        l->notify();
        return proton::listener(*this, addr);
    } catch (const std::exception& e) {
        lh.on_error(e.what());
        lh.on_close();
        throw;
    }
}

void epoll_container::stop_listening(const std::string& addr) {
    lock_guard g(lock_);
    listeners_.erase(addr);
    idle_check(g);
}

void epoll_container::run() {
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

void epoll_container::auto_stop(bool set) {
    lock_guard g(lock_);
    stopping_ = set;
}

void epoll_container::stop(const proton::error_condition& err) {
    lock_guard g(lock_);
    stop_err_ = err;
    interrupt();
}

void epoll_container::wait() {
    std::unique_lock<std::mutex> l(lock_);
    stopped_.wait(l, [this]() { return this->threads_ == 0; } );
    for (auto& eng : engines_)
        eng.second->engine().disconnected(stop_err_);
    listeners_.clear();
    engines_.clear();
}

void epoll_container::interrupt() {
    // Add an always-readable fd with 0 data and no ONESHOT to interrupt all threads.
    epoll_event ev = {};
    ev.events = EPOLLIN;
    check(epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, interrupt_fd_, &ev), "interrupt");
}

}

// This is the only public function.
std::unique_ptr<proton::container> make_mt_container(const std::string& id) {
    return std::unique_ptr<proton::container>(new epoll_container(id));
}
