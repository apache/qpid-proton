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
require "weakref"

begin
  require "securerandom"
rescue LoadError
  require "kconv"               # Ruby < 1.9
end

DEPRECATION = "[DEPRECATION]"
def deprecated(old, new=nil)
  repl = new ? ", use `#{new}`" : "with no replacement"
  warn "#{DEPRECATION} `#{old}` is deprecated #{repl} (called from #{caller(2).first})"
end

# Exception classes
require "core/exceptions"

# Utility classes
require "util/version"
require "util/error_handler"
require "util/swig_helper"
require "util/wrapper"
require "util/timeout"

# Types
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
require "core/delivery"
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

# Messenger API classes
require "messenger/subscription"
require "messenger/tracker_status"
require "messenger/tracker"
require "messenger/messenger"

# Handler classes
require "handler/adapter"

# Core classes that depend on Handler
require "core/messaging_handler"
require "core/container"
require "core/connection_driver"

# Backwards compatibility shims
require "reactor/container"

module Qpid::Proton::Handler
  # @deprecated alias for backwards compatibility
  MessagingHandler = Qpid::Proton::MessagingHandler
end

module Qpid::Proton
  Tracker = Delivery

  # @private
  def self.registry
    @registry ||= {}
  end

  # @private
  def self.add_to_registry(key, value)
    self.registry[key] = value
  end

  # @private
  def self.get_from_registry(key)
    self.registry[key]
  end

  # @private
  def self.delete_from_registry(key)
    self.registry.delete(key)
  end
end
