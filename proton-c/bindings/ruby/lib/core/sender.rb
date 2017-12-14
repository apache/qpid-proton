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

  # The sending endpoint.
  #
  # @see Receiver
  #
  class Sender < Link

    # @private
    include Util::ErrorHandler

    # Hint to the remote receiver about the number of messages available.
    # The receiver may use this to optimize credit flow, or may ignore it.
    # @param n [Integer] The number of deliveries potentially available.
    def offered(n)
      Cproton.pn_link_offered(@impl, n)
    end

    # TODO aconway 2017-12-05: incompatible, used to return bytes sent.

    # @!method send(message)
    # Send a message.
    # @param message [Message] The message to send.
    # @return [Tracker] Tracks the outcome of the message.
    def send(message, *args)
      tag = nil
      if args.size > 0
        # deprecated: allow tag in args[0] for backwards compat
        raise ArgumentError("too many arguments") if args.size > 1
        tag = args[0]
      end
      tag ||= next_tag
      t = Tracker.new(Cproton.pn_delivery(@impl, tag))
      Cproton.pn_link_send(@impl, message.encode)
      Cproton.pn_link_advance(@impl)
      t.settle if snd_settle_mode == SND_SETTLED
      return t
    end

    # @deprecated use {#send}
    def stream(bytes)
      deprecated __method__, "send"
      Cproton.pn_link_send(@impl, bytes)
    end

    # @deprecated internal use only
    def delivery_tag() deprecated(__method__); next_tag; end

    private

    def initialize(*arg) super; @tag_count = 0; end
    def next_tag() (@tag_count += 1).to_s(32); end

    # @private
    can_raise_error :stream, :error_class => Qpid::Proton::LinkError
  end
end


