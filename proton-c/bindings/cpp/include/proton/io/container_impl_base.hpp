#ifndef PROTON_IO_CONTAINER_IMPL_BASE_HPP
#define PROTON_IO_CONTAINER_IMPL_BASE_HPP

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

#include "../container.hpp"

#include <mutex>
#include <sstream>

namespace proton {
namespace io {

/// **Experimental** - A base container implementation.
///
/// This is a thread-safe partial implementation of the
/// proton::container interface to reduce boilerplate code in
/// container implementations. Requires C++11.
///
/// You can ignore this class if you want to implement the functions
/// in a different way.
class container_impl_base : public standard_container {
  public:
    // Pull in base class functions here so that name search finds all the overloads
    using standard_container::open_receiver;
    using standard_container::open_sender;

    /// @copydoc container::client_connection_options
    void client_connection_options(const connection_options & opts) {
        store(client_copts_, opts);
    }
    
    /// @copydoc container::client_connection_options
    connection_options client_connection_options() const {
        return load(client_copts_);
    }
    
    /// @copydoc container::server_connection_options
    void server_connection_options(const connection_options & opts) {
        store(server_copts_, opts);
    }
    
    /// @copydoc container::server_connection_options
    connection_options server_connection_options() const {
        return load(server_copts_);
    }
    
    /// @copydoc container::sender_options
    void sender_options(const class sender_options & opts) {
        store(sender_opts_, opts);
    }
    
    /// @copydoc container::sender_options
    class sender_options sender_options() const {
        return load(sender_opts_);
    }
    
    /// @copydoc container::receiver_options
    void receiver_options(const class receiver_options & opts) {
        store(receiver_opts_, opts);
    }
    
    /// @copydoc container::receiver_options
    class receiver_options receiver_options() const {
        return load(receiver_opts_);
    }

    /// @copydoc container::open_sender
    returned<sender> open_sender(
        const std::string &url, const class sender_options &opts, const connection_options &copts)
    {
        return open_link<sender, class sender_options>(url, opts, copts, &connection::open_sender);
    }

    /// @copydoc container::open_receiver
    returned<receiver> open_receiver(
        const std::string &url, const class receiver_options &opts, const connection_options &copts)
    {
        return open_link<receiver>(url, opts, copts, &connection::open_receiver);
    }

  private:
    template<class T, class Opts>
    returned<T> open_link(
        const std::string &url_str, const Opts& opts, const connection_options& copts,
        T (connection::*open_fn)(const std::string&, const Opts&))
    {
        std::string addr = url(url_str).path();
        std::shared_ptr<thread_safe<connection> > ts_connection = connect(url_str, copts);
        std::promise<returned<T> > result_promise;
        auto do_open = [ts_connection, addr, opts, open_fn, &result_promise]() {
            try {
                connection c = ts_connection->unsafe();
                returned<T> s = make_thread_safe((c.*open_fn)(addr, opts));
                result_promise.set_value(s);
            } catch (...) {
                result_promise.set_exception(std::current_exception());
            }
        };
        ts_connection->event_loop()->inject(do_open);
        std::future<returned<T> > result_future = result_promise.get_future();
        if (!result_future.valid())
            throw error(url_str+": connection closed");
        return result_future.get();
    }

    mutable std::mutex lock_;
    template <class T> T load(const T& v) const {
        std::lock_guard<std::mutex> g(lock_);
        return v;
    }
    template <class T> void store(T& v, const T& x) const {
        std::lock_guard<std::mutex> g(lock_);
        v = x;
    }
    connection_options client_copts_, server_copts_;
    class receiver_options receiver_opts_;
    class sender_options sender_opts_;
};

} // io
} // proton

#endif // PROTON_IO_CONTAINER_IMPL_BASE_HPP
