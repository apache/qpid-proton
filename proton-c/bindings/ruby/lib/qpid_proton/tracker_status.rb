#
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
#

module Qpid

  module Proton

    # TrackerStatus contains symbols that represent the status value for a
    # Tracker.
    #
    class TrackerStatus

        def initialize value, name # :nodoc:
          @value = value
          @name = name
        end

        def value # :nodoc:
          @value
        end

        def to_s # :nodoc:
          @name.to_s
        end

       def self.by_name(name) # :nodoc:
          @by_name[name.to_sym] unless name.nil?
        end

        def self.by_value(value) # :nodoc:
          @by_value[value] unless value.nil?
        end

        private

        def self.add_item(key, value) # :nodoc:
          @by_name ||= {}
          @by_name[key] = TrackerStatus.new value, key
          @by_value ||= {}
          @by_value[value] = @by_name[key]
        end

        def self.const_missing(key) # :nodoc:
          @by_name[key]
        end

        self.add_item :UNKNOWN,  Cproton::PN_STATUS_UNKNOWN
        self.add_item :PENDING,  Cproton::PN_STATUS_PENDING
        self.add_item :ACCEPTED, Cproton::PN_STATUS_ACCEPTED
        self.add_item :REJECTED, Cproton::PN_STATUS_REJECTED
        self.add_item :SETTLED,  Cproton::PN_STATUS_SETTLED

    end

  end

end
