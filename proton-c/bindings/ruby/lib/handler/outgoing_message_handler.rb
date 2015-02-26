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

module Qpid::Proton::Handler

  # A utility for simpler and more intuitive handling of delivery events
  # related to outgoing messages.
  #
  class OutgoingMessageHandler < Qpid::Proton::BaseHandler

    def initialize(auto_settle = true, delegate = nil)
      @auto_settle = auto_settle
      @delegate = delegate
    end

    def on_link_flow(event)
      self.on_sendable(event) if event.link.sender? && event.link.credit > 0
    end

    def on_delivery(event)
      delivery = event.delivery
      if delivery.link.sender? && delivery.updated?
        if delivery.remote_accepted?
          self.on_accepted(event)
        elsif delivery.remote_rejected?
          self.on_rejected(event)
        elsif delivery.remote_released? || delivery.remote_modified?
          self.on_released(event)
        end
        self.on_settled(event) if delivery.settled?
        delivery.settle if @auto_settle
      end
    end

    # Called when the sender link has credit and messages and be transferred.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_sendable(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_sendable, event) if !@delegate.nil?
    end

    # Called when the remote peer accepts a sent message.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_accepted(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_accepted, event) if !@delegate.nil?
    end

    # Called when the remote peer rejects a sent message.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_rejected(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_rejected, event) if !@delegate.nil?
    end

    # Called when the remote peer releases an outgoing message.
    #
    # Note that this may be in resposnse to either the REELAASE or MODIFIED
    # state as defined by the AMQP specification.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_released(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_released, event) if !@delegate.nil?
    end

    # Called when the remote peer has settled the outgoing message.
    #
    # This is the point at which it should never be retransmitted.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_settled(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_settled, event) if !@delegate.nil?
    end

  end

end
