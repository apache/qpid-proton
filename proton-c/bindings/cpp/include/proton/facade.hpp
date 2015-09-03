#ifndef PROTON_CPP_FACADE_H
#define PROTON_CPP_FACADE_H

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

/*! \page c-and-cpp C and memory management.
 *\brief
 *
 * The C++ API is a very thin wrapper over the C API.  The C API consists of a
 * set of `struct` types and associated C functions.  For each there is a a C++
 * `facade` that provides C++ member functions to call the corresponding C
 * functions on the underlying C struct. Facade classes derive from the
 * `proton::facade` template.
 *
 * The facade class occupies no additional memory. The C+ facade pointer points
 * directly to the underlying C++ struct, calling C++ member functions corresponds
 * directly to calling the C function.
 *
 * If you want to mix C and C++ code (which should be done carefully!) you can
 * cast between a facade pointer and a C struct pointer with `proton::pn_cast`
 * and `foo::cast()` where `foo` is some C++ facade class.
 *
 * Deleting a facade object calls the appropriate `pn_foo_free` function or
 * `pn_decref` as appropriate.
 *
 * Some proton structs are reference counted, the facade classes for these
 * derive from the `proton::counted_facade` template. Most proton functions that
 * return facade objects return a reference to an object that is owned by the
 * called object. Such references are only valid in a limited scope (for example
 * in an event handler function.) To keep a reference outside that scope, you
 * can assign it to a proton::counted_ptr, std::shared_ptr, boost::shared_ptr,
 * boost::intrusive_ptr or std::unique_ptr. You can also call
 * `proton::counted_facade::new_ptr` to get a raw pointer which you must
 * delete when you are done (e.g. using `std::auto_ptr`)
 */

/**@file
 * Classes and templates used by object facades.
 */

#include "proton/export.hpp"
#include "counted_ptr.hpp"

namespace proton {

///@cond INTERNAL
struct empty_base {};
///@endcond

/**
 * Base class for C++ facades of proton C struct types.
 *
 * @see \ref c-and-cpp
 */
template <class P, class T, class Base=empty_base> class facade : public Base {
  public:
    /// The underlying C struct type.
    typedef P pn_type;

    /// Cast the C struct pointer to a C++ facade pointer.
    static T* cast(P* p) { return reinterpret_cast<T*>(p); }

#if PN_USE_CPP11
    facade() = delete;
    facade(const facade&) = delete;
    facade& operator=(const facade&) = delete;
    void operator delete(void* p) = delete; // Defined by type T.
#else
  private:
    facade();
    facade(const facade&);
    facade& operator=(const facade&);
    void operator delete(void* p);
#endif
};

/** Cast a facade type to the C struct type.
 * Allow casting away const, the underlying pn structs have not constness.
 */
template <class T> typename T::pn_type* pn_cast(const T* p) {
    return reinterpret_cast<typename T::pn_type*>(const_cast<T*>(p));
}

/** Cast a counted pointer to a facade type to the C struct type.
 * Allow casting away const, the underlying pn structs have not constness.
 */
template <class T> typename T::pn_type* pn_cast(const counted_ptr<T>& p) {
    return reinterpret_cast<typename T::pn_type*>(const_cast<T*>(p.get()));
}

/**
 * Some proton C structs are reference counted. The C++ facade for such structs can be
 * converted to any of the following smart pointers: std::shared_ptr, std::unique_ptr,
 * boost::shared_ptr, boost::intrusive_ptr.
 *
 * unique_ptr takes ownership of a single *reference* not the underlying struct,
 * so it is safe to have multiple unique_ptr to the same facade object or to mix
 * unique_ptr with shared_ptr etc.
 *
 * Deleting a counted_facade subclass actually calls `pn_decref` to remove a reference.
 */
template <class P, class T, class Base=empty_base>
class counted_facade : public facade<P, T, Base>
{
  public:

    /// Deleting a counted_facade actually calls `pn_decref` to remove a reference.
    void operator delete(void* p) { decref(p); }

    operator counted_ptr<T>() { return counted_ptr<T>(static_cast<T*>(this)); }
    operator counted_ptr<const T>() const { return counted_ptr<const T>(static_cast<const T*>(this)); }
#if PN_USE_CPP11
    // TODO aconway 2015-08-21: need weak pointer context for efficient shared_ptr
    operator std::shared_ptr<T>() { return std::shared_ptr<T>(new_ptr()); }
    operator std::shared_ptr<const T>() const { return std::shared_ptr<const T>(new_ptr()); }
    operator std::unique_ptr<T>() { return std::unique_ptr<T>(new_ptr()); }
    operator std::unique_ptr<const T>() const { return std::unique_ptr<const T>(new_ptr()); }
#endif
#if PN_USE_BOOST
    // TODO aconway 2015-08-21: need weak pointer context for efficient shared_ptr
    operator boost::shared_ptr<T>() { return boost::shared_ptr<T>(new_ptr()); }
    operator boost::shared_ptr<const T>() const { return boost::shared_ptr<const T>(new_ptr()); }
    operator boost::intrusive_ptr<T>() { return boost::intrusive_ptr<T>(this); }
    operator boost::intrusive_ptr<const T>() const { return boost::intrusive_ptr<const T>(this); }
#endif

    /** Get a pointer to a new reference to the underlying object.
     * You must delete the returned pointer to release the reference.
     * It is safer to convert to one of the supported smart pointer types.
     */
    T* new_ptr() { incref(this); return static_cast<T*>(this); }
    const T* new_ptr() const { incref(this); return static_cast<const T*>(this); }

  private:
    counted_facade(const counted_facade&);
    counted_facade& operator=(const counted_facade&);

  template <class U> friend class counted_ptr;
};

}
#endif  /*!PROTON_CPP_FACADE_H*/
