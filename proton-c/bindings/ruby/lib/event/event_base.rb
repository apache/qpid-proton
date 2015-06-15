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

  # @private
  def self.dispatch(handler, method, *args)
    args = args.last unless args.nil?
    if handler.respond_to? method.to_sym
      return handler.__send__(method, args)
    elsif handler.respond_to? :on_unhandled
      return handler.__send__(:on_unhandled, method, args)
    end
  end

  # EventBase is the foundation for creating application-specific events.
  #
  # @example
  #
  #   # SCENARIO: A continuation of the example in EventType.
  #   #
  #   #           An Event class is defined to handle receiving encrypted
  #   #           data from a remote endpoint.
  #
  #   class EncryptedDataEvent < EventBase
  #     def initialize(message)
  #       super(EncryptedDataEvent, message,
  #             Qpid::Proton::Event::ENCRYPTED_RECV)
  #     end
  #   end
  #
  #   # at another point, when encrypted data is received
  #   msg = Qpid::Proton::Message.new
  #   msg.decode(link.receive(link.pending))
  #   if encrypted?(msg)
  #     collector.put(EncryptedDataEvent.new(msg)
  #   end
  #
  # @see EventType The EventType class for how ENCRYPTED_RECV was defined.
  #
  class EventBase

    # Returns the name for the class associated with this event.
    attr_reader :class_name

    # Returns the associated context object for the event.
    attr_reader :context

    # Returns the type of the event.
    attr_reader :type

    # Creates a new event with the specific class_name and context of the
    # specified type.
    #
    # @param class_name [String] The name of the class.
    # @param context [Object] The event context.
    # @param type [EventType] The event type.
    #
    def initialize(class_name, context, type)
      @class_name = class_name
      @context = context
      @type = type
    end

    # Invokes the type-specific method on the provided handler.
    #
    # @param handler [Object] The handler to be notified of this event.
    #
    def dispatch(handler)
      Qpid::Proton.dispatch(handler, @type.method, self)
    end

  end

end
