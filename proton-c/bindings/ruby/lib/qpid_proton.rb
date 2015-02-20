#--
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#++

require "cproton"
require "date"

if RUBY_VERSION < "1.9"
require "kconv"
end

# Exception classes
require "core/exceptions"

# Utility classes
require "util/version"
require "util/error_handler"

# Types
require "types/strings"
require "types/hash"
require "types/array"
require "types/described"

# Codec classes
require "codec/mapping"
require "codec/data"

# Main Proton classes
require "core/message"

# Messenger API classes
require "messenger/filters"
require "messenger/subscription"
require "messenger/tracker_status"
require "messenger/tracker"
require "messenger/selectable"
require "messenger/messenger"
