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

  # The receiving endpoint.
  #
  # @see Sender
  #
  class Receiver < Link

    # @private
    include Util::SwigHelper

    # @private
    PROTON_METHOD_PREFIX = "pn_link"

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
    proton_accessor :drain

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
    # @param n [Fixnum] The amount to increment the link credit.
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
    # @param limit [Fixnum] The maximum bytes to receive.
    #
    # @return [Fixnum, nil] The number of bytes received, or nil if the end of
    # the stream was reached.t
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
