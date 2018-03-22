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


require "cproton"
require "date"
require "weakref"

begin
  require "securerandom"
rescue LoadError
  require "kconv"               # Ruby < 1.9
end

# @api qpid
# Qpid is the top level module for the Qpid project http://qpid.apache.org
# Definitions for this library are in module {Qpid::Proton}
module Qpid
  # Proton is a ruby API for sending and receiving AMQP messages in clients or servers.
  # See the {overview}[../file.README.html] for more.
  module Proton
    # Only opened here for module doc comment
  end
end

# Exception classes
require "core/exceptions"

# Utility classes
require "util/deprecation"
require "util/version"
require "util/error_handler"
require "util/schedule"
require "util/wrapper"

# Types
require "types/type"
require "types/strings"
require "types/hash"
require "types/array"
require "types/described"

# Codec classes
require "codec/mapping"
require "codec/data"

# Main Proton classes
require "core/condition"
require "core/event"
require "core/uri"
require "core/message"
require "core/endpoint"
require "core/session"
require "core/terminus"
require "core/disposition"
require "core/transfer"
require "core/delivery"
require "core/tracker"
require "core/link"
require "core/sender"
require "core/receiver"
require "core/connection"
require "core/sasl"
require "core/ssl_domain"
require "core/ssl_details"
require "core/ssl"
require "core/transport"
require "core/url"
require "core/connection_driver"

# Handlers and adapters
require "handler/adapter"
require "handler/messaging_adapter"
require "core/messaging_handler"

# Main container class
require "core/container"

# DEPRECATED Backwards compatibility shims for Reactor API
require "handler/reactor_messaging_adapter"
require "handler/messaging_handler" # Keep original name for compatibility
require "reactor/container"
