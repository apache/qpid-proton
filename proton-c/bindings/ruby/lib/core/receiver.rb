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

  # The receiving endpoint.
  #
  # @see Sender
  #
  class Receiver < Link

    # @private
    PROTON_METHOD_PREFIX = "pn_link"
    # @private
    include Util::Wrapper

    # Open {Receiver} link
    #
    # @overload open_receiver(address)
    #   @param address [String] address of the source to receive from
    # @overload open_receiver(opts)
    #   @param opts [Hash] Receiver options, see {Receiver#open}
    #   @option opts [Integer] :credit_window automatically maintain this much credit
    #     for messages to be pre-fetched while the current message is processed.
    #   @option opts [Boolean] :auto_accept if true, deliveries that are not settled by
    #     the application in {MessagingHandler#on_message} are automatically accepted.
    #   @option opts [Boolean] :dynamic (false) dynamic property for source {Terminus#dynamic}
    #   @option opts [String,Hash] :source source address or source options, see {Terminus#apply}
    #   @option opts [String,Hash] :target target address or target options, see {Terminus#apply}
    #   @option opts [String] :name (generated) unique name for the link.
    def open(opts=nil)
      opts ||= {}
      opts = { :source => opts } if opts.is_a? String
      @credit_window =  opts.fetch(:credit_window, 10)
      @auto_accept = opts.fetch(:auto_accept, true)
      source.apply(opts[:source])
      target.apply(opts[:target])
      source.dynamic = !!opts[:dynamic]
      super()
      self
    end

    # @return [Integer] credit window, see {#open}
    attr_reader :credit_window

    # @return [Boolean] auto_accept flag, see {#open}
    attr_reader :auto_accept

    # @!attribute drain
    #
    # The drain mode.
    #
    # If a receiver is in drain mode, then the sending endpoint of a link must
    # immediately use up all available credit on the link. If this is not
    # possible, the excess credit must be returned by invoking #drained.
    #
    # Only the receiving endpoint can set the drain mode.
    #
    # @return [Boolean] True if drain mode is set.
    #
    proton_set_get :drain

    # @!attribute [r] draining?
    #
    # Returns if a link is currently draining.
    #
    # A link is defined to be draining when drain mode is set to true and
    # the sender still has excess credit.
    #
    # @return [Boolean] True if the receiver is currently draining.
    #
    proton_caller :draining?

    # Grants credit for incoming deliveries.
    #
    # @param n [Integer] The amount to increment the link credit.
    #
    def flow(n)
      Cproton.pn_link_flow(@impl, n)
    end

    # Allows receiving up to the specified limit of data from the remote
    # endpoint.
    #
    # Note that large messages can be streamed across the network, so just
    # because there is no data to read does not imply the message is complete.
    #
    # To ensure the entirety of the message data has been read, either call
    # #receive until nil is returned, or verify that #partial? is false and
    # Delivery#pending is 0.
    #
    # @param limit [Integer] The maximum bytes to receive.
    #
    # @return [Integer, nil] The number of bytes received, or nil if the end of
    # the stream was reached.
    #
    # @see Deliver#pending To see how much buffer space is needed.
    #
    # @raise [LinkError] If an error occurs.
    #
    def receive(limit)
      (n, bytes) = Cproton.pn_link_recv(@impl, limit)
      return nil if n == Qpid::Proton::Error::EOS
      raise LinkError.new("[#{n}]: #{Cproton.pn_link_error(@impl)}") if n < 0
      return bytes
    end

  end

end
