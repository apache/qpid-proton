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
 *
 */

/*
 * This file is largely just a stub, we're actually creating a JavaScript library
 * rather than an executable so most of the  work is done at the link stage.
 * We do however need to link the libqpid-proton-bitcode.so with *something* and
 * this is it. This file also provides a way to pass any global variable or
 * #defined values to the JavaScript binding by providing wrapper functions.
 */
#include <stdio.h>
#include <proton/version.h>

// To access #define values in JavaScript we need to wrap them in a function.
int pn_get_version_major() {return PN_VERSION_MAJOR;}
int pn_get_version_minor() {return PN_VERSION_MINOR;}

