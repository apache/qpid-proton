#ifndef PROTON_FWD_HPP
#define PROTON_FWD_HPP

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

namespace proton {

class annotation_key;
class connection;
class connection_options;
class container;
class delivery;
class error_condition;
class event;
class work_queue;
class message;
class message_id;
class messaging_handler;
class listen_handler;
class listener;
class receiver;
class receiver_iterator;
class receiver_options;
class reconnect_timer;
class sasl;
class sender;
class sender_iterator;
class sender_options;
class session;
class session_options;
class source_options;
class ssl;
class target_options;
class tracker;
class transport;
class url;
class void_function0;


namespace io {

class connection_driver;

}

template <class T> class returned;
template <class T> class thread_safe;

}

#endif // PROTON_FWD_HPP
