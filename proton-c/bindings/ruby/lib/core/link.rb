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

module Qpid::Proton

  # The base for both Sender and Receiver, providing common functionality
  # between both ends.
  #
  # A Link has a single parent Qpid::Proton::Session instance.
  #
  class Link < Endpoint

    # The sender will send all deliveries initially unsettled.
    SND_UNSETTLED = Cproton::PN_SND_UNSETTLED
    # The sender will send all deliveries settled to the receiver.
    SND_SETTLED = Cproton::PN_SND_SETTLED
    # The sender may send a mixture of settled and unsettled deliveries.
    SND_MIXED = Cproton::PN_SND_MIXED

    # The receiver will settle deliveries regardless of what the sender does.
    RCV_FIRST = Cproton::PN_RCV_FIRST
    # The receiver will only settle deliveries after the sender settles.
    RCV_SECOND = Cproton::PN_RCV_SECOND

    # @private
    include Util::SwigHelper

    # @private
    PROTON_METHOD_PREFIX = "pn_link"

    # @!attribute [r] state
    #
    # Returns the endpoint state flags.
    #
    proton_caller :state

    # @!method open
    #
    # Opens the link. Once this operation has completed, the state flag will be
    # set.
    #
    # @see Endpoint::LOCAL_ACTIVE
    proton_caller :open

    # @!method close
    #
    # Closes the link.
    #
    # Once this operation has completed, the state flag will be set.
    # This may be called without first calling #open, which is the equivalent to
    # calling #open and then #close.
    #
    # @see Endpoint::LOCAL_CLOSED
    proton_caller :close

    # @!method detach
    #
    # Detaches the link.
    proton_caller :detach

    # Advance the current delivery to the next on the link.
    #
    # For sending links, this operation is used to finish sending message data
    # for the current outgoing delivery and move on to the next outgoing
    # delivery (if any).
    #
    # For receiving links, this operatoin is used to finish accessing message
    # data from the current incoming delivery and move on to the next incoming
    # delivery (if any).
    #
    # @return [Boolean] True if the current delivery was changed.
    #
    # @see #current
    #
    proton_caller :advance

    proton_caller :unsettled

    # @!attribute [r] credit
    #
    # Returns the credit balance for a link.
    #
    # Links use a credit based flow control scheme. Every receiver maintains a
    # credit balance that corresponds to the number of deliveries that the
    # receiver can accept at any given moment.
    #
    # As more capacity becomes available at the receiver, it adds credit to this
    # balance and communicates the new balance to the sender. Whenever a
    # delivery is sent/received, the credit balance maintained by the link is
    # decremented by one.
    #
    # Once the credit balance at the sender reaches zero, the sender must pause
    # sending until more credit is obtained from the receiver.
    #
    # NOte that a sending link may still be used to send deliveries eve if
    # credit reaches zero. However those deliveries will end up being buffer by
    # the link until enough credit is obtained from the receiver to send them
    # over the wire. In this case the balance reported will go negative.
    #
    # @return [Fixnum] The credit balance.
    #
    # @see #flow
    #
    proton_caller :credit

    # @!attribute [r] remote_credit
    #
    # Returns the remote view of the credit.
    #
    # The remote view of the credit for a link differs from the local view of
    # credit for a link by the number of queued deliveries. In other words,
    # remote credit is defined as credit - queued.
    #
    # @see #queued
    # @see #credit
    #
    # @return [Fixnum] The remove view of the credit.
    #
    proton_caller :remote_credit

    # @!attribute [r] available
    #
    # Returns the available deliveries hint for a link.
    #
    # The available count for a link provides a hint as to the number of
    # deliveries that might be able to be sent if sufficient credit were issued
    # by the receiving link endpoint.
    #
    # @return [Fixnum] The available deliveries hint.
    #
    # @see Sender#offered
    #
    proton_caller :available

    # @!attribute [r] queued
    #
    # Returns the number of queued deliveries for a link.
    #
    # Links may queue deliveries for a number of reasons. For example, there may
    # be insufficient credit to send them to the receiver, or they simply may
    # not have yet had a chance to be written to the wire.
    #
    # @return [Fixnum] The number of queued deliveries.
    #
    # @see #credit
    #
    proton_caller :queued

    # @!attribute [r] name
    #
    # Returns the name of the link.
    #
    # @return [String] The name.
    #
    proton_caller :name

    # @!attribute [r] sender?
    #
    # Returns if the link is a sender.
    #
    # @return [Boolean] True if the link is a sender.
    #
    proton_reader  :sender, :is_or_get => :is

    # @!attribute [r] receiver?
    #
    # Returns if the link is a receiver.
    #
    # @return [Boolean] True if the link is a receiver.
    #
    proton_reader  :receiver, :is_or_get => :is

    # @private
    proton_reader :attachments

    # Drains excess credit.
    #
    # When a link is in drain mode, the sender must use all excess credit
    # immediately and release any excess credit back to the receiver if there
    # are no deliveries available to send.
    #
    # When invoked on a Sender that is in drain mode, this operation will
    # release all excess credit back to the receiver and return the number of
    # credits released back to the sender. If the link is not in drain mode,
    # this operation is a noop.
    #
    # When invoked on a Receiver, this operation will return and reset the
    # number of credits the sender has released back to it.
    #
    # @return [Fixnum] The number of credits drained.
    #
    proton_caller :drained

    # @private
    include Util::Wrapper

    # @private
    def self.wrap(impl)
      return nil if impl.nil?

      result = self.fetch_instance(impl, :pn_link_attachments)
      return result unless result.nil?
      if Cproton.pn_link_is_sender(impl)
        return Sender.new(impl)
      elsif Cproton.pn_link_is_receiver(impl)
        return Receiver.new(impl)
      end
    end

    # @private
    def initialize(impl)
      @impl = impl
      self.class.store_instance(self, :pn_link_attachments)
    end

    # Returns additional error information.
    #
    # Whenever a link operation fails (i.e., returns an error code) additional
    # error details can be obtained from this method. Ther error object that is
    # returned may also be used to clear the error condition.
    #
    # @return [Error] The error.
    #
    def error
      Cproton.pn_link_error(@impl)
    end

    # Returns the next link that matches the given state mask.
    #
    # @param state_mask [Fixnum] The state mask.
    #
    # @return [Sender, Receiver] The next link.
    #
    def next(state_mask)
      return Link.wrap(Cproton.pn_link_next(@impl, state_mask))
    end

    # Returns the locally defined source terminus.
    #
    # @return [Terminus] The terminus
    def source
      Terminus.new(Cproton.pn_link_source(@impl))
    end

    # Returns the locally defined target terminus.
    #
    # @return [Terminus] The terminus.
    #
    def target
      Terminus.new(Cproton.pn_link_target(@impl))
    end

    # Returns a representation of the remotely defined source terminus.
    #
    # @return [Terminus] The terminus.
    #
    def remote_source
      Terminus.new(Cproton.pn_link_remote_source(@impl))
    end

    # Returns a representation of the remotely defined target terminus.
    #
    # @return [Terminus] The terminus.
    #
    def remote_target
      Terminus.new(Cproton.pn_link_remote_target(@impl))
    end

    # Returns the parent session.
    #
    # @return [Session] The session.
    #
    def session
      Session.wrap(Cproton.pn_link_session(@impl))
    end

    # Returns the parent connection.
    #
    # @return [Connection] The connection.
    #
    def connection
      self.session.connection
    end

    # Returns the parent delivery.
    #
    # @return [Delivery] The delivery.
    #
    def delivery(tag)
      Delivery.new(Cproton.pn_delivery(@impl, tag))
    end

    # Returns the current delivery.
    #
    # Each link maintains a sequence of deliveries in the order they were
    # created, along with a reference to the *current* delivery. All send and
    # receive operations on a link take place on the *current* delivery. If a
    # link has no current delivery, the current delivery is automatically
    # pointed to the *next* delivery created on the link.
    #
    # Once initialized, the current delivery remains the same until it is
    # changed by advancing, or until it is settled.
    #
    # @see #next
    # @see Delivery#settle
    #
    # @return [Delivery] The current delivery.
    #
    def current
      Delivery.wrap(Cproton.pn_link_current(@impl))
    end

    # Sets the local sender settle mode.
    #
    # @param mode [Fixnum] The settle mode.
    #
    # @see #SND_UNSETTLED
    # @see #SND_SETTLED
    # @see #SND_MIXED
    #
    def snd_settle_mode=(mode)
      Cproton.pn_link_set_snd_settle_mode(@impl, mode)
    end

    # Returns the local sender settle mode.
    #
    # @return [Fixnum] The local sender settle mode.
    #
    # @see #snd_settle_mode
    #
    def snd_settle_mode
      Cproton.pn_link_snd_settle_mode(@impl)
    end

    # Sets the local receiver settle mode.
    #
    # @param mode [Fixnum] The settle mode.
    #
    # @see #RCV_FIRST
    # @see #RCV_SECOND
    #
    def rcv_settle_mode=(mode)
      Cproton.pn_link_set_rcv_settle_mode(@impl, mode)
    end

    # Returns the local receiver settle mode.
    #
    # @return [Fixnum] The local receiver settle mode.
    #
    def rcv_settle_mode
      Cproton.pn_link_rcv_settle_mode(@impl)
    end

    # @private
    def _local_condition
      Cproton.pn_link_condition(@impl)
    end

    # @private
    def _remote_condition
      Cproton.pn_link_remote_condition(@impl)
    end

    def ==(other)
      other.respond_to?(:impl) &&
      (Cproton.pni_address_of(other.impl) == Cproton.pni_address_of(@impl))
    end

  end

end
