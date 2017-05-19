#ifndef PROTON_WORK_QUEUE_HPP
#define PROTON_WORK_QUEUE_HPP

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

#include "./duration.hpp"
#include "./fwd.hpp"
#include "./function.hpp"
#include "./internal/config.hpp"
#include "./internal/export.hpp"
#include "./internal/pn_unique_ptr.hpp"

#include <functional>

struct pn_connection_t;
struct pn_session_t;
struct pn_link_t;

namespace proton {

/// **Experimental** - A work queue for serial execution.
///
/// Event handler functions associated with a single proton::connection are called in sequence.
/// The connection's @ref work_queue allows you to "inject" extra @ref work from any thread,
/// and have it executed in the same sequence.
///
/// You may also create arbitrary @ref work_queue objects backed by a @ref container that allow
/// other objects to have their own serialised work queues that can have work injected safely
/// from other threads. The @ref container ensures that the work is correctly serialised.
///
/// The @ref work class represents the work to be queued and can be created from a function
/// that takes no parameters and returns no value.
///

class work {
  public:
#if PN_CPP_HAS_STD_FUNCTION
    work(void_function0& f): item_( [&f]() { f(); }) {}
    template <class T>
    work(T f): item_(f) {}

    void operator()() { item_(); }
#else
    work(void_function0& f): item_(&f) {}

    void operator()() { (*item_)(); }
#endif
    ~work() {}


  private:
#if PN_CPP_HAS_STD_FUNCTION
    std::function<void()> item_;
#else
    void_function0* item_;
#endif
};

class PN_CPP_CLASS_EXTERN work_queue {
    /// @cond internal
    class impl;
    work_queue& operator=(impl* i);
    /// @endcond

  public:
    /// Create work_queue
    PN_CPP_EXTERN work_queue();
    PN_CPP_EXTERN work_queue(container&);

    PN_CPP_EXTERN ~work_queue();

    /// Add work to the work queue: f() will be called serialised with other work in the queue:
    /// deferred and possibly in another thread.
    ///
    /// @return true if f() has or will be called, false if the event_loop is ended
    /// or f() cannot be injected for any other reason.
    PN_CPP_EXTERN bool add(work f);

    /// Add work to the work queue after duration: f() will be called after the duration
    /// serialised with other work in the queue: possibly in another thread.
    ///
    /// The scheduled execution is "best effort" and it is possible that after the elapsed duration
    /// the work will not be able to be injected into the serialised context - there will be no
    /// indication of this.
    PN_CPP_EXTERN void schedule(duration, work);

  private:
    PN_CPP_EXTERN static work_queue& get(pn_connection_t*);
    PN_CPP_EXTERN static work_queue& get(pn_session_t*);
    PN_CPP_EXTERN static work_queue& get(pn_link_t*);

    internal::pn_unique_ptr<impl> impl_;

    /// @cond INTERNAL
  friend class container;
  friend class io::connection_driver;
  template <class T> friend class thread_safe;
    /// @endcond
};

// Utilities to make injecting functions/member functions palatable in C++03
// Lots of repetition to handle functions with up to 3 arguments
#if !PN_CPP_HAS_CPP11
template <class R>
struct work0 : public proton::void_function0 {
    R (* fn_)();

    work0(R (* f)()) :
        fn_(f) {}

    void operator()() {
        (*fn_)();
        delete this;
    }
};

template <class R, class A>
struct work1 : public proton::void_function0 {
    R (* fn_)(A);
    A a_;

    work1(R (* t)(A), A a) :
        fn_(t), a_(a) {}

    void operator()() {
        (*fn_)(a_);
        delete this;
    }
};

template <class R, class A, class B>
struct work2 : public proton::void_function0 {
    R (* fn_)(A, B);
    A a_;
    B b_;

    work2(R (* t)(A, B), A a, B b) :
        fn_(t), a_(a), b_(b) {}

    void operator()() {
        (*fn_)(a_, b_);
        delete this;
    }
};

template <class R, class A, class B, class C>
struct work3 : public proton::void_function0 {
    R (* fn_)(A, B, C);
    A a_;
    B b_;
    C c_;

    work3(R (* t)(A, B, C), A a, B b, C c) :
        fn_(t), a_(a), b_(b), c_(c) {}

    void operator()() {
        (*fn_)(a_, b_, c_);
        delete this;
    }
};

template <class R, class T>
struct work_pmf0 : public proton::void_function0 {
    T& holder_;
    R (T::* fn_)();

    work_pmf0(R (T::* a)(), T& h) :
        holder_(h), fn_(a) {}

    void operator()() {
        (holder_.*fn_)();
        delete this;
    }
};

template <class R, class T, class A>
struct work_pmf1 : public proton::void_function0 {
    T& holder_;
    R (T::* fn_)(A);
    A a_;

    work_pmf1(R (T::* t)(A), T& h, A a) :
        holder_(h), fn_(t), a_(a) {}

    void operator()() {
        (holder_.*fn_)(a_);
        delete this;
    }
};

template <class R, class T, class A, class B>
struct work_pmf2 : public proton::void_function0 {
    T& holder_;
    R (T::* fn_)(A, B);
    A a_;
    B b_;

    work_pmf2(R (T::* t)(A, B), T& h, A a, B b) :
        holder_(h), fn_(t), a_(a), b_(b) {}

    void operator()() {
        (holder_.*fn_)(a_, b_);
        delete this;
    }
};

template <class R, class T, class A, class B, class C>
struct work_pmf3 : public proton::void_function0 {
    T& holder_;
    R (T::* fn_)(A, B, C);
    A a_;
    B b_;
    C c_;

    work_pmf3(R (T::* t)(A, B, C), T& h, A a, B b, C c) :
        holder_(h), fn_(t), a_(a), b_(b), c_(c) {}

    void operator()() {
        (holder_.*fn_)(a_, b_, c_);
        delete this;
    }
};

/// make_work is the equivalent of C++11 std::bind for C++03
/// It will bind both free functions and pointers to member functions
template <class R, class T>
void_function0& make_work(R (T::*f)(), T* t) {
    return *new work_pmf0<R, T>(f, *t);
}

template <class R, class T, class A>
void_function0& make_work(R (T::*f)(A), T* t, A a) {
    return *new work_pmf1<R, T, A>(f, *t, a);
}

template <class R, class T, class A, class B>
void_function0& make_work(R (T::*f)(A, B), T* t, A a, B b) {
    return *new work_pmf2<R, T, A, B>(f, *t, a, b);
}

template <class R, class T, class A, class B, class C>
void_function0& make_work(R (T::*f)(A, B, C), T* t, A a, B b, C c) {
    return *new work_pmf3<R, T, A, B, C>(f, *t, a, b, c);
}

template <class R>
void_function0& make_work(R (*f)()) {
    return *new work0<R>(f);
}

template <class R, class A>
void_function0& make_work(R (*f)(A), A a) {
    return *new work1<R, A>(f, a);
}

template <class R, class A, class B>
void_function0& make_work(R (*f)(A, B), A a, B b) {
    return *new work2<R, A, B>(f, a, b);
}

template <class R, class A, class B, class C>
void_function0& make_work(R (*f)(A, B, C), A a, B b, C c) {
    return *new work3<R, A, B, C>(f, a, b, c);
}

namespace {
template <class T>
bool schedule_work_helper(T t, void_function0& w) {
    bool r = t->add(w);
    if (!r) delete &w;
    return r;
}
}

/// schedule_work is a convenience that is used for C++03 code to defer function calls
/// to a work_queue
template <class WQ, class F>
bool schedule_work(WQ wq, F f) {
    return schedule_work_helper(wq, make_work(f));
}

template <class WQ, class F, class A>
bool schedule_work(WQ wq, F f, A a) {
    return schedule_work_helper(wq, make_work(f, a));
}

template <class WQ, class F, class A, class B>
bool schedule_work(WQ wq, F f, A a, B b) {
    return schedule_work_helper(wq, make_work(f, a, b));
}

template <class WQ, class F, class A, class B, class C>
bool schedule_work(WQ wq, F f, A a, B b, C c) {
    return schedule_work_helper(wq, make_work(f, a, b, c));
}

template <class WQ, class F, class A, class B, class C, class D>
bool schedule_work(WQ wq, F f, A a, B b, C c, D d) {
    return schedule_work_helper(wq, make_work(f, a, b, c, d));
}

template <class WQ, class F>
void schedule_work(WQ wq, duration dn, F f) {
    wq->schedule(dn, make_work(f));
}

template <class WQ, class F, class A>
void schedule_work(WQ wq, duration dn, F f, A a) {
    wq->schedule(dn, make_work(f, a));
}

template <class WQ, class F, class A, class B>
void schedule_work(WQ wq, duration dn, F f, A a, B b) {
    wq->schedule(dn, make_work(f, a, b));
}

template <class WQ, class F, class A, class B, class C>
void schedule_work(WQ wq, duration dn, F f, A a, B b, C c) {
    wq->schedule(dn, make_work(f, a, b, c));
}

template <class WQ, class F, class A, class B, class C, class D>
void schedule_work(WQ wq, duration dn, F f, A a, B b, C c, D d) {
    wq->schedule(dn, make_work(f, a, b, c, d));
}

/// This version of proton::schedule_work schedule_works calling a free function to an arbitrary work queue
#else
// The C++11 version is *much* simpler and even so more general!
// These definitions encompass everything in the C++03 section
template <class WQ, class... Rest>
bool schedule_work(WQ wq, Rest&&... r) {
    return wq->add(std::bind(std::forward<Rest>(r)...));
}

template <class WQ, class... Rest>
void schedule_work(WQ wq, duration d, Rest&&... r) {
    wq->schedule(d, std::bind(std::forward<Rest>(r)...));
}

template <class... Rest>
work make_work(Rest&&... r) {
    return std::bind(std::forward<Rest>(r)...);
}
#endif

} // proton

#endif // PROTON_WORK_QUEUE_HPP
