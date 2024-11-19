#ifndef PROTON_TRANSACTION_HPP
#define PROTON_TRANSACTION_HPP


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
#include "./internal/export.hpp"
#include "./sender.hpp"
#include "./tracker.hpp"
#include "./container.hpp"

/// @file
/// @copybrief proton::transaction

namespace proton {

class transaction_handler;

// TODO: This should not be accessible to users.
class transaction_impl {
  public:
    proton::sender *txn_ctrl = nullptr;
    proton::transaction_handler *handler = nullptr;
    proton::binary id;
    proton::tracker _declare;
    proton::tracker _discharge;
    bool failed = false;
    std::vector<proton::tracker> pending;

    void commit();
    void abort();
    void declare();
    proton::tracker send(proton::sender s, proton::message msg);

    void discharge(bool failed);
    proton::tracker send_ctrl(proton::symbol descriptor, proton::value _value);
    void handle_outcome(proton::tracker t);
    transaction_impl(proton::sender &_txn_ctrl,
                     proton::transaction_handler &_handler,
                     bool _settle_before_discharge);

    // delete copy and assignment operator to ensure no copy of this object is
    // every made transaction_impl(const transaction_impl&) = delete;
    // transaction_impl& operator=(const transaction_impl&) = delete;
};

class
PN_CPP_CLASS_EXTERN transaction {
  // private:
  //   PN_CPP_EXTERN transaction(proton::sender& _txn_ctrl,
  //   proton::transaction_handler& _handler, bool _settle_before_discharge);

  static transaction mk_transaction_impl(sender &s, transaction_handler &h,
                                        bool f);
  PN_CPP_EXTERN transaction(transaction_impl* impl);
  public:
  transaction_impl* _impl;
  // TODO:
    // PN_CPP_EXTERN transaction(transaction &o);
    PN_CPP_EXTERN transaction();
    PN_CPP_EXTERN ~transaction();
    PN_CPP_EXTERN void commit();
    PN_CPP_EXTERN void abort();
    PN_CPP_EXTERN void declare();
    PN_CPP_EXTERN void handle_outcome(proton::tracker);
    PN_CPP_EXTERN proton::tracker send(proton::sender s, proton::message msg);

  friend class transaction_impl;
  friend class container::impl;
};

class
PN_CPP_CLASS_EXTERN transaction_handler {
  public:
    PN_CPP_EXTERN virtual ~transaction_handler();

    /// Called when a local transaction is declared.
    PN_CPP_EXTERN virtual void on_transaction_declared(transaction);

    /// Called when a local transaction is discharged successfully.
    PN_CPP_EXTERN virtual void on_transaction_committed(transaction);

    /// Called when a local transaction is discharged unsuccessfully (aborted).
    PN_CPP_EXTERN virtual void on_transaction_aborted(transaction);

    /// Called when a local transaction declare fails.
    PN_CPP_EXTERN virtual void on_transaction_declare_failed(transaction);

    /// Called when the commit of a local transaction fails.
    PN_CPP_EXTERN virtual void on_transaction_commit_failed(transaction);
};

} // namespace proton

#endif // PROTON_TRANSACTION_HPP
