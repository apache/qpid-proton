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
require "util/engine"
require "util/uuid"

# Types
require "types/strings"
require "types/hash"
require "types/array"
require "types/described"

# Codec classes
require "codec/mapping"
require "codec/data"

# Event API classes
require "event/collector"

# Main Proton classes
require "core/message"
require "core/endpoint"
require "core/session"
require "core/terminus"
require "core/disposition"
require "core/delivery"
require "core/link"
require "core/sender"
require "core/receiver"

# Messenger API classes
require "messenger/filters"
require "messenger/subscription"
require "messenger/tracker_status"
require "messenger/tracker"
require "messenger/selectable"
require "messenger/messenger"

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
