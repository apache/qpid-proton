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

if RUBY_VERSION < "1.9"
require "kconv"
else
  require "securerandom"
end

# Exception classes
require "core/exceptions"

# Utility classes
require "util/version"
require "util/error_handler"
require "util/constants"
require "util/swig_helper"
require "util/condition"
require "util/wrapper"
require "util/class_wrapper"
require "util/engine"
require "util/uuid"
require "util/timeout"
require "util/handler"
require "util/reactor"

# Types
require "types/strings"
require "types/hash"
require "types/array"
require "types/described"

# Codec classes
require "codec/mapping"
require "codec/data"

# Event API classes
require "event/event_type"
require "event/event_base"
require "event/event"
require "event/collector"

# Main Proton classes
require "core/selectable"
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
require "core/base_handler"
require "core/url"

# Messenger API classes
require "messenger/subscription"
require "messenger/tracker_status"
require "messenger/tracker"
require "messenger/messenger"

# Handler classes
require "handler/c_adaptor"
require "handler/wrapped_handler"
require "handler/acking"
require "handler/endpoint_state_handler"
require "handler/incoming_message_handler"
require "handler/outgoing_message_handler"
require "handler/c_flow_controller"
require "handler/messaging_handler"

# Reactor classes
require "reactor/task"
require "reactor/acceptor"
require "reactor/reactor"
require "reactor/ssl_config"
require "reactor/global_overrides"
require "reactor/urls"
require "reactor/connector"
require "reactor/backoff"
require "reactor/session_per_connection"
require "reactor/container"
require "reactor/link_option"

module Qpid::Proton
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
