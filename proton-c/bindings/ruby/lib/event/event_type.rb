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

module Qpid::Proton::Event

  # Manages the association between an Event and the method which should
  # process on the context object associated with an occurance of the event.
  #
  # Each type is identified by a unique #type value.
  #
  # @example
  #
  #   # SCENARIO: A part of an application handles extracting and decrypting
  #   #            data received from a remote endpoint.
  #   #
  #   #            An EventType is created to notify handlers that such a
  #   #            situation has occurred.
  #
  #   ENCRYPTED_RECV = 10000 # the unique constant value for the event
  #
  #   # create a new event type which, when it occurs, invokes a method
  #   # named :on_encrypted_data when a handler is notified of its occurrance
  #   Qpid::Proton::Event::ENCRYPTED_RECV =
  #     Qpid::Proton::Event::EventType.new(ENCRYPTED_RECV, :on_encrypted_data)
  #
  # @see EventBase EventBase for the rest of this example.
  # @see Qpid::Proton::Event::Event The Event class for more details on events.
  #
  class EventType

    # The method to invoke on any potential handler.
    attr_reader :method
    attr_reader :number

    def initialize(number, method)
      @number = number
      @name = Cproton.pn_event_type_name(@number)
      @method = method
      @@types ||= {}
      @@types[number] = self
    end

    # @private
    def to_s
      @name
    end

    # @private
    def self.by_type(type) # :nodoc:
      @@types[type]
    end

  end

end
