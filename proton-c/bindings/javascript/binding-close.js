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

// These values are essentially constants sitting in the proton namespace.
// We have to set them after pn_get_version_major/pn_get_version_minor have been
// defined so we must do it here in binding-close.js as it's a --post-js block.
Module['VERSION_MAJOR'] = _pn_get_version_major();
Module['VERSION_MINOR'] = _pn_get_version_minor();

})(); // End of self calling lambda used to wrap library.
