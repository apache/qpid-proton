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
#include <utility>
#if PN_CPP_HAS_LAMBDAS && PN_CPP_HAS_VARIADIC_TEMPLATES
#include <type_traits>
#endif

struct pn_connection_t;
struct pn_session_t;
struct pn_link_t;

/// @file
/// @copybrief proton::work_queue

namespace proton {

/// @cond INTERNAL

namespace internal { namespace v03 {

struct invocable {
    invocable() {}
    virtual ~invocable() {}

    virtual invocable& clone() const = 0;
    virtual void operator() () = 0;
};

template <class T>
struct invocable_cloner : invocable {
    virtual ~invocable_cloner() {}
    virtual invocable& clone() const {
        return *new T(static_cast<T const&>(*this));
    }
};

struct invocable_wrapper {
    invocable_wrapper(): wrapped_(0) {}
    invocable_wrapper(const invocable_wrapper& w): wrapped_(&w.wrapped_->clone()) {}
    invocable_wrapper& operator=(const invocable_wrapper& that) {
        invocable_wrapper newthis(that);
        std::swap(wrapped_, newthis.wrapped_);
        return *this;
    }
#if PN_CPP_HAS_RVALUE_REFERENCES
    invocable_wrapper(invocable_wrapper&& w): wrapped_(w.wrapped_) {}
    invocable_wrapper& operator=(invocable_wrapper&& that) {delete wrapped_; wrapped_ = that.wrapped_; return *this; }
#endif
    ~invocable_wrapper() { delete wrapped_; }

    invocable_wrapper(const invocable& i): wrapped_(&i.clone()) {}
    void operator()() { (*wrapped_)(); }

    invocable* wrapped_;
};

/// **Unsettled API** - A work item for a @ref work_queue "work queue".
///
/// It can be created from a function that takes no parameters and
/// returns no value.
class work {
  public:
    /// Create a work item.
    work() {}

    work(const invocable& i): item_(i) {}

    /// Invoke the work item.
    void operator()() { item_(); }

    ~work() {}

  private:
    invocable_wrapper item_;
};

// Utilities to make work from functions/member functions (C++03 version)
// Lots of repetition to handle functions/member functions with up to 3 arguments

template <class R>
struct work0 : public invocable_cloner<work0<R> > {
    R (* fn_)();

    work0(R (* f)()) :
        fn_(f) {}

    void operator()() {
        (*fn_)();
    }
};

template <class R, class A>
struct work1 : public invocable_cloner<work1<R,A> > {
    R (* fn_)(A);
    A a_;

    work1(R (* t)(A), A a) :
        fn_(t), a_(a) {}

    void operator()() {
        (*fn_)(a_);
    }
};

template <class R, class A, class B>
struct work2 : public invocable_cloner<work2<R,A,B> > {
    R (* fn_)(A, B);
    A a_;
    B b_;

    work2(R (* t)(A, B), A a, B b) :
        fn_(t), a_(a), b_(b) {}

    void operator()() {
        (*fn_)(a_, b_);
    }
};

template <class R, class A, class B, class C>
struct work3 : public invocable_cloner<work3<R,A,B,C> > {
    R (* fn_)(A, B, C);
    A a_;
    B b_;
    C c_;

    work3(R (* t)(A, B, C), A a, B b, C c) :
        fn_(t), a_(a), b_(b), c_(c) {}

    void operator()() {
        (*fn_)(a_, b_, c_);
    }
};

template <class R, class T>
struct work_pmf0 : public invocable_cloner<work_pmf0<R,T> > {
    T& holder_;
    R (T::* fn_)();

    work_pmf0(R (T::* a)(), T& h) :
        holder_(h), fn_(a) {}

    void operator()() {
        (holder_.*fn_)();
    }
};

template <class R, class T, class A>
struct work_pmf1 : public invocable_cloner<work_pmf1<R,T,A> > {
    T& holder_;
    R (T::* fn_)(A);
    A a_;

    work_pmf1(R (T::* t)(A), T& h, A a) :
        holder_(h), fn_(t), a_(a) {}

    void operator()() {
        (holder_.*fn_)(a_);
    }
};

template <class R, class T, class A, class B>
struct work_pmf2 : public invocable_cloner<work_pmf2<R,T,A,B> > {
    T& holder_;
    R (T::* fn_)(A, B);
    A a_;
    B b_;

    work_pmf2(R (T::* t)(A, B), T& h, A a, B b) :
        holder_(h), fn_(t), a_(a), b_(b) {}

    void operator()() {
        (holder_.*fn_)(a_, b_);
    }
};

template <class R, class T, class A, class B, class C>
struct work_pmf3 : public invocable_cloner<work_pmf3<R,T,A,B,C> > {
    T& holder_;
    R (T::* fn_)(A, B, C);
    A a_;
    B b_;
    C c_;

    work_pmf3(R (T::* t)(A, B, C), T& h, A a, B b, C c) :
        holder_(h), fn_(t), a_(a), b_(b), c_(c) {}

    void operator()() {
        (holder_.*fn_)(a_, b_, c_);
    }
};

/// `make_work` is the equivalent of C++11 `std::bind` for C++03.  It
/// will bind both free functions and pointers to member functions.
template <class R, class T>
work make_work(R (T::*f)(), T* t) {
    return work_pmf0<R, T>(f, *t);
}

template <class R, class T, class A>
work make_work(R (T::*f)(A), T* t, A a) {
    return work_pmf1<R, T, A>(f, *t, a);
}

template <class R, class T, class A, class B>
work make_work(R (T::*f)(A, B), T* t, A a, B b) {
    return work_pmf2<R, T, A, B>(f, *t, a, b);
}

template <class R, class T, class A, class B, class C>
work make_work(R (T::*f)(A, B, C), T* t, A a, B b, C c) {
    return work_pmf3<R, T, A, B, C>(f, *t, a, b, c);
}

template <class R>
work make_work(R (*f)()) {
    return work0<R>(f);
}

template <class R, class A>
work make_work(R (*f)(A), A a) {
    return work1<R, A>(f, a);
}

template <class R, class A, class B>
work make_work(R (*f)(A, B), A a, B b) {
    return work2<R, A, B>(f, a, b);
}

template <class R, class A, class B, class C>
work make_work(R (*f)(A, B, C), A a, B b, C c) {
    return work3<R, A, B, C>(f, a, b, c);
}

} } // internal::v03

#if PN_CPP_HAS_LAMBDAS && PN_CPP_HAS_VARIADIC_TEMPLATES

namespace internal { namespace v11 {

class work {
  public:
    /// **Unsettled API**
    work() {}

    /// **Unsettled API**
    ///
    /// Construct a unit of work from anything
    /// function-like that takes no arguments and returns
    /// no result.
    template <class T,
        // Make sure we don't match the copy or move constructors
        class = typename std::enable_if<!std::is_same<typename std::decay<T>::type,work>::value>::type
    >
    work(T&& f): item_(std::forward<T>(f)) {}

    /// **Unsettled API**
    ///
    /// Execute the piece of work
    void operator()() { item_(); }

    ~work() {}

  private:
    std::function<void()> item_;
};

/// **Unsettled API** - Make a unit of work.
///
/// Make a unit of work from either a function or a member function
/// and an object pointer.
///
/// **C++ versions** - This C++11 version is just a wrapper for
/// `std::bind`.
template <class... Rest>
work make_work(Rest&&... r) {
    return std::bind(std::forward<Rest>(r)...);
}

} } // internal::v11

using internal::v11::work;
using internal::v11::make_work;

#else

using internal::v03::work;
using internal::v03::make_work;

#endif

/// @endcond

/// **Unsettled API** - A context for thread-safe execution of work.
///
/// Event-handler functions associated with a single
/// `proton::connection` are called in sequence.  The connection's
/// `proton::work_queue` allows you to "inject" extra work from
/// any thread and have it executed in the same sequence.
///
/// You may also create arbitrary `proton::work_queue` objects backed
/// by a @ref container that allow other objects to have their own
/// serialised work queues that can have work injected safely from
/// other threads. The @ref container ensures that the work is
/// correctly serialised.
///
/// The `proton::work` class represents the work to be queued and can
/// be created from a function that takes no parameters and returns no
/// value.
class PN_CPP_CLASS_EXTERN work_queue {
    /// @cond INTERNAL
    class impl;
    work_queue& operator=(impl* i);
    /// @endcond

  public:
    /// **Unsettled API** - Create a work queue.
    PN_CPP_EXTERN work_queue();

    /// **Unsettled API** - Create a work queue backed by a container.
    PN_CPP_EXTERN work_queue(container&);

    PN_CPP_EXTERN ~work_queue();

    /// **Unsettled API** - Add work `fn` to the work queue.
    ///
    /// Work `fn` will be called serially with other work in the queue.
    /// The work may be deferred and executed in another thread.
    ///
    /// @return true if `fn` has been or will be called; false if the
    /// event loops is ended or `fn` cannot be injected for any other
    /// reason.
    PN_CPP_EXTERN bool add(work fn);

    /// **Deprecated** - Use `add(work)`.
    PN_CPP_EXTERN PN_CPP_DEPRECATED("Use 'work_queue::add(work)'") bool add(void_function0& fn);

    /// **Unsettled API** - Add work `fn` to the work queue after a
    /// duration.
    ///
    /// Scheduled execution is "best effort". It may not be possible
    /// to inject the work after the elapsed duration.  There will be
    /// no indication of this.
    ///
    /// @copydetails add()
    PN_CPP_EXTERN void schedule(duration, work fn);

    /// **Deprecated** - Use `schedule(duration, work)`.
    PN_CPP_EXTERN PN_CPP_DEPRECATED("Use 'work_queue::schedule(duration, work)'") void schedule(duration, void_function0& fn);

  private:
    /// Declare both v03 and v11 if compiling with c++11 as the library contains both.
    /// A C++11 user should never call the v03 overload so it is private in this case
#if PN_CPP_HAS_LAMBDAS && PN_CPP_HAS_VARIADIC_TEMPLATES
    PN_CPP_EXTERN bool add(internal::v03::work fn);
    PN_CPP_EXTERN void schedule(duration, internal::v03::work fn);
#endif

    PN_CPP_EXTERN static work_queue& get(pn_connection_t*);
    PN_CPP_EXTERN static work_queue& get(pn_session_t*);
    PN_CPP_EXTERN static work_queue& get(pn_link_t*);

    internal::pn_unique_ptr<impl> impl_;

    /// @cond INTERNAL
  friend class container;
  friend class io::connection_driver;
    /// @endcond
};

} // proton

#endif // PROTON_WORK_QUEUE_HPP
