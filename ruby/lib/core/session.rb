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


module Qpid::Proton

  # A session is the parent for senders and receivers.
  #
  # A Session has a single parent Qpid::Proton::Connection instance.
  #
  class Session < Endpoint
    include Util::Deprecation

    # @private
    PROTON_METHOD_PREFIX = "pn_session"
    # @private
    include Util::Wrapper

    # @!attribute incoming_capacity
    #
    # The incoming capacity of a session determines how much incoming message
    # data the session will buffer. Note that if this value is less than the
    # negotatied frame size of the transport, it will be rounded up to one full
    # frame.
    #
    # @return [Integer] The incoing capacity of the session, measured in bytes.
    #
    proton_set_get :incoming_capacity

    # @private
    proton_get :attachments

    # @!attribute [r] outgoing_bytes
    #
    # @return [Integer] The number of outgoing bytes currently being buffered.
    #
    proton_caller :outgoing_bytes

    # @!attribute [r] incoming_bytes
    #
    # @return [Integer] The number of incomign bytes currently being buffered.
    #
    proton_caller :incoming_bytes

    # Open the session
    proton_caller :open

    # @!attribute [r] state
    #
    # @return [Integer] The endpoint state.
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

    # Close the local end of the session. The remote end may or may not be closed.
    # @param error [Condition] Optional error condition to send with the close.
    def close(error=nil)
      Condition.assign(_local_condition, error)
      Cproton.pn_session_close(@impl)
    end

    # @deprecated use {Connection#each_session}
    def next(state_mask)
      deprecated __method__, "Connection#each_session"
      Session.wrap(Cproton.pn_session_next(@impl, state_mask))
    end

    # Returns the parent connection.
    #
    # @return [Connection] The connection.
    #
    def connection
      Connection.wrap(Cproton.pn_session_connection(@impl))
    end

    # @deprecated use {#open_sender}
    def sender(name)
      deprecated __method__, "open_sender"
      Sender.new(Cproton.pn_sender(@impl, name));
    end

    # @deprecated use {#open_receiver}
    def receiver(name)
      deprecated __method__, "open_receiver"
      Receiver.new(Cproton.pn_receiver(@impl, name))
    end

    # Create and open a {Receiver} link, see {Receiver#open}
    # @param opts [Hash] receiver options, see {Receiver#open}
    # @return [Receiver]
    def open_receiver(opts=nil) 
      Receiver.new(Cproton.pn_receiver(@impl, link_name(opts))).open(opts)
    end

    # Create and open a {Sender} link, see {#open}
    # @param opts [Hash] sender options, see {Sender#open}
    # @return [Sender]
    def open_sender(opts=nil)
      Sender.new(Cproton.pn_sender(@impl, link_name(opts))).open(opts)
    end

    # Get the links on this Session.
    # @overload each_link
    #   @yieldparam l [Link] pass each link to block
    # @overload each_link
    #   @return [Enumerator] enumerator over links
    def each_link
      return enum_for(:each_link) unless block_given?
      l = Cproton.pn_link_head(Cproton.pn_session_connection(@impl), 0);
      while l
        link = Link.wrap(l)
        yield link if link.session == self
        l = Cproton.pn_link_next(l, 0)
      end
      self
    end

    # Get the {Sender} links - see {#each_link}
    def each_sender() each_link.select { |l| l.sender? }; end

    # Get the {Receiver} links - see {#each_link}
    def each_receiver() each_link.select { |l| l.receiver? }; end

    private

    def link_name(opts)
      (opts.respond_to?(:to_hash) && opts[:name]) || connection.link_name
    end

    def _local_condition
      Cproton.pn_session_condition(@impl)
    end

    def _remote_condition # :nodoc:
      Cproton.pn_session_remote_condition(@impl)
    end

  end

end
