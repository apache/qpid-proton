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

/// @file
/// @copybrief proton::transaction

namespace proton {

class
PN_CPP_CLASS_EXTERN Transaction {
    proton::sender txn_ctrl;
    proton::transaction_handler handler;
    // TODO int
    int id = 0;
    proton::tracker _declare;
    proton::tracker _discharge;
    bool failed = false;
    std::vector<proton::tracker> pending;
  public:
  // TODO:
    PN_CPP_EXTERN Transaction(proton::sender _txn_ctrl, proton::transaction_handler _handler, bool _settle_before_discharge = false);
    PN_CPP_EXTERN virtual ~Transaction();
    PN_CPP_EXTERN virtual void commit();
    PN_CPP_EXTERN virtual void abort();
    PN_CPP_EXTERN virtual void declare();
    PN_CPP_EXTERN virtual void discharge(bool failed);
    PN_CPP_EXTERN virtual proton::tracker send_ctrl(proton::symbol descriptor, proton::value _value);
    PN_CPP_EXTERN virtual proton::tracker send(proton::sender s, proton::message msg);
    PN_CPP_EXTERN virtual void handle_outcome(proton::tracker t);
};

class
PN_CPP_CLASS_EXTERN transaction_handler {
  
  public:
    PN_CPP_EXTERN transaction_handler();
    PN_CPP_EXTERN virtual ~transaction_handler();

    /// Called when a local transaction is declared.
    PN_CPP_EXTERN virtual void on_transaction_declared(Transaction&);

    /// Called when a local transaction is discharged successfully.
    PN_CPP_EXTERN virtual void on_transaction_committed(Transaction&);

    /// Called when a local transaction is discharged unsuccessfully (aborted).
    PN_CPP_EXTERN virtual void on_transaction_aborted(Transaction&);

    /// Called when a local transaction declare fails.
    PN_CPP_EXTERN virtual void on_transaction_declare_failed(Transaction&);

    /// Called when the commit of a local transaction fails.
    PN_CPP_EXTERN virtual void on_transaction_commit_failed(Transaction&);
};

} // namespace proton

#endif // PROTON_TRANSACTION_HPP
