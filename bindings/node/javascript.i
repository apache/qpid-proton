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

%module cproton

// provided by SWIG development libraries
%include javascript.swg

%header %{
/* Include the headers needed by the code in this wrapper file */
#include <proton/types.h>
#include <proton/connection.h>
#include <proton/condition.h>
#include <proton/delivery.h>
#include <proton/driver.h>
#include <proton/driver_extras.h>
#include <proton/event.h>
#include <proton/handlers.h>
#include <proton/message.h>
#include <proton/messenger.h>
#include <proton/reactor.h>
#include <proton/session.h>
#include <proton/url.h>
%}

%include "proton/cproton.i"
