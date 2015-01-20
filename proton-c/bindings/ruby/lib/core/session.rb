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

  # A session is the parent for senders and receivers.
  #
  # A Session has a single parent Qpid::Proton::Connection instance.
  #
  class Session < Endpoint

    # @private
    include Util::Wrapper

    # @private
    include Util::SwigHelper

    # @private
    PROTON_METHOD_PREFIX = "pn_session"

    # @!attribute incoming_capacity
    #
    # The incoming capacity of a session determines how much incoming message
    # data the session will buffer. Note that if this value is less than the
    # negotatied frame size of the transport, it will be rounded up to one full
    # frame.
    #
    # @return [Fixnum] The incoing capacity of the session, measured in bytes.
    #
    proton_accessor :incoming_capacity

    # @private
    proton_reader :attachments

    # @!attribute [r] outgoing_bytes
    #
    # @return [Fixnum] The number of outgoing bytes currently being buffered.
    #
    proton_caller :outgoing_bytes

    # @!attribute [r] incoming_bytes
    #
    # @return [Fixnum] The number of incomign bytes currently being buffered.
    #
    proton_caller :incoming_bytes

    # @!method open
    # Opens the session.
    #
    # Once this operaton has completed, the state flag is updated.
    #
    # @see LOCAL_ACTIVE
    #
    proton_caller :open

    # @!attribute [r] state
    #
    # @return [Fixnum] The endpoint state.
    #
    proton_caller :state

    # @private
    def self.wrap(impl)
      return nil if impl.nil?
      self.fetch_instance(impl, :pn_session_attachments) || Session.new(impl)
    end

    # @private
    def initialize(impl)
      @impl = impl
      self.class.store_instance(self, :pn_session_attachments)
    end

    # Closed the session.
    #
    # Once this operation has completed, the state flag will be set. This may be
    # called without calling #open, in which case it is the equivalence of
    # calling #open and then close immediately.
    #
    def close
      self._update_condition
      Cproton.pn_session_close(@impl)
    end

    # Retrieves the next session from a given connection that matches the
    # specified state mask.
    #
    # When uses with Connection#session_head an application can access all of
    # the session son the connection that match the given state.
    #
    # @param state_mask [Fixnum] The state mask to match.
    #
    # @return [Session, nil] The next session if one matches, or nil.
    #
    def next(state_mask)
      Session.wrap(Cproton.pn_session_next(@impl, state_mask))
    end

    # Returns the parent connection.
    #
    # @return [Connection] The connection.
    #
    def connection
      Connection.wrap(Cproton.pn_session_connection(@impl))
    end

    # Constructs a new sender.
    #
    # Each sender between two AMQP containers must be uniquely named. Note that
    # this uniqueness cannot be enforced at the library level, so some
    # consideration should be taken in choosing link names.
    #
    # @param name [String] The link name.
    #
    # @return [Sender, nil] The sender, or nil if an error occurred.
    #
    def sender(name)
      Sender.new(Cproton.pn_sender(@impl, name))
    end

    # Constructs a new receiver.
    #
    # Each receiver between two AMQP containers must be uniquely named. Note
    # that this uniqueness cannot be enforced at the library level, so some
    # consideration should be taken in choosing link names.
    #
    # @param name [String] The link name.
    #
    # @return [Receiver, nil] The receiver, or nil if an error occurred.
    #
    def receiver(name)
      Receiver.new(Cproton.pn_receiver(@impl, name))
    end

    # @private
    def _local_condition
      Cproton.pn_session_condition(@impl)
    end

    # @private
    def _remote_condition # :nodoc:
      Cproton.pn_session_remote_condition(@impl)
    end

  end

end
