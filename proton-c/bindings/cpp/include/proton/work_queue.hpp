#ifndef PROTON_EVENT_LOOP_HPP
#define PROTON_EVENT_LOOP_HPP

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

#if PN_CPP_HAS_EXPLICIT_CONVERSIONS
    /// When using C++11 (or later) you can use work_queue in a bool context
    /// to indicate if there is an event loop set.
    PN_CPP_EXTERN explicit operator bool() const { return bool(impl_); }
#endif

    /// No event loop set.
    PN_CPP_EXTERN bool operator !() const { return !impl_; }

    /// Add work to the work queue: f() will be called serialised with other work in the queue:
    /// deferred and possibly in another thread.
    ///
    /// @return true if f() has or will be called, false if the event_loop is ended
    /// and f() cannot be injected.
    PN_CPP_EXTERN bool add(work f);

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
struct work0 : public proton::void_function0 {
    void (* fn_)();

    work0(void (* f)()) :
        fn_(f) {}

    void operator()() {
        (*fn_)();
        delete this;
    }
};

template <class A>
struct work1 : public proton::void_function0 {
    void (* fn_)(A);
    A a_;

    work1(void (* t)(A), A a) :
        fn_(t), a_(a) {}

    void operator()() {
        (*fn_)(a_);
        delete this;
    }
};

template <class A, class B>
struct work2 : public proton::void_function0 {
    void (* fn_)(A, B);
    A a_;
    B b_;

    work2(void (* t)(A, B), A a, B b) :
        fn_(t), a_(a), b_(b) {}

    void operator()() {
        (*fn_)(a_, b_);
        delete this;
    }
};

template <class A, class B, class C>
struct work3 : public proton::void_function0 {
    void (* fn_)(A, B, C);
    A a_;
    B b_;
    C c_;

    work3(void (* t)(A, B, C), A a, B b, C c) :
        fn_(t), a_(a), b_(b), c_(c) {}

    void operator()() {
        (*fn_)(a_, b_, c_);
        delete this;
    }
};

template <class T>
struct work_pmf0 : public proton::void_function0 {
    T& holder_;
    void (T::* fn_)();

    work_pmf0(void (T::* a)(), T& h) :
        holder_(h), fn_(a) {}

    void operator()() {
        (holder_.*fn_)();
        delete this;
    }
};

template <class T, class A>
struct work_pmf1 : public proton::void_function0 {
    T& holder_;
    void (T::* fn_)(A);
    A a_;

    work_pmf1(void (T::* t)(A), T& h, A a) :
        holder_(h), fn_(t), a_(a) {}

    void operator()() {
        (holder_.*fn_)(a_);
        delete this;
    }
};

template <class T, class A, class B>
struct work_pmf2 : public proton::void_function0 {
    T& holder_;
    void (T::* fn_)(A, B);
    A a_;
    B b_;

    work_pmf2(void (T::* t)(A, B), T& h, A a, B b) :
        holder_(h), fn_(t), a_(a), b_(b) {}

    void operator()() {
        (holder_.*fn_)(a_, b_);
        delete this;
    }
};

template <class T, class A, class B, class C>
struct work_pmf3 : public proton::void_function0 {
    T& holder_;
    void (T::* fn_)(A, B, C);
    A a_;
    B b_;
    C c_;

    work_pmf3(void (T::* t)(A, B, C), T& h, A a, B b, C c) :
        holder_(h), fn_(t), a_(a), b_(b), c_(c) {}

    void operator()() {
        (holder_.*fn_)(a_, b_, c_);
        delete this;
    }
};

/// This version of proton::defer defers calling an object's member function to the object's work queue
template <class T>
void defer(void (T::*f)(), T* t) {
    work_pmf0<T>* w = new work_pmf0<T>(f, *t);
    t->add(*w);
}

template <class T, class A>
void defer(void (T::*f)(A), T* t, A a) {
    work_pmf1<T, A>* w = new work_pmf1<T, A>(f, *t, a);
    t->add(*w);
}

template <class T, class A, class B>
void defer(void (T::*f)(A, B), T* t, A a, B b) {
    work_pmf2<T, A, B>* w = new work_pmf2<T, A, B>(f, *t, a, b);
    t->add(*w);
}

template <class T, class A, class B, class C>
void defer(void (T::*f)(A, B, C), T* t, A a, B b, C c) {
    work_pmf3<T, A, B, C>* w = new work_pmf3<T, A, B, C>(f, *t, a, b, c);
    t->add(*w);
}

/// This version of proton::defer defers calling a member function to an arbitrary work queue
template <class T, class U>
void defer(U* wq, void (T::*f)(), T* t) {
    work_pmf0<T>* w = new work_pmf0<T>(f, *t);
    wq->add(*w);
}

template <class T, class U, class A>
void defer(U* wq, void (T::*f)(A), T* t, A a) {
    work_pmf1<T, A>* w = new work_pmf1<T, A>(f, *t, a);
    wq->add(*w);
}

template <class T, class U, class A, class B>
void defer(U* wq, void (T::*f)(A, B), T* t, A a, B b) {
    work_pmf2<T, A, B>* w = new work_pmf2<T, A, B>(f, *t, a, b);
    wq->add(*w);
}

template <class T, class U, class A, class B, class C>
void defer(U* wq, void (T::*f)(A, B, C), T* t, A a, B b, C c) {
    work_pmf3<T, A, B, C>* w = new work_pmf3<T, A, B, C>(f, *t, a, b, c);
    wq->add(*w);
}

/// This version of proton::defer defers calling a free function to an arbitrary work queue
template <class U>
void defer(U* wq, void (*f)()) {
    work0* w = new work0(f);
    wq->add(*w);
}

template <class U, class A>
void defer(U* wq, void (*f)(A), A a) {
    work1<A>* w = new work1<A>(f, a);
    wq->add(*w);
}

template <class U, class A, class B>
void defer(U* wq, void (*f)(A, B), A a, B b) {
    work2<A, B>* w = new work2<A, B>(f, a, b);
    wq->add(*w);
}

template <class U, class A, class B, class C>
void defer(U* wq, void (*f)(A, B, C), A a, B b, C c) {
    work3<A, B, C>* w = new work3<A, B, C>(f, a, b, c);
    wq->add(*w);
}
#else
// The C++11 version is *much* simpler and even so more general!
// These 2 definitions encompass everything in the C++03 section
template <class T, class... Rest>
void defer(void(T::*f)(Rest...), T* t, Rest... r) {
    t->add(std::bind(f, t, r...));
}

template <class U, class... Rest>
void defer(U* wq, Rest&&... r) {
    wq->add(std::bind(std::forward<Rest>(r)...));
}
#endif

} // proton

#endif // PROTON_EVENT_LOOP_HPP
