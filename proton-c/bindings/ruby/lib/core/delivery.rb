# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
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
  # Allow a {Receiver} to indicate the status of a received message to the {Sender}
  class Delivery < Transfer
    # @return [Receiver] The parent {Receiver} link.
    def receiver() link; end

    # Accept the receiveed message.
    def accept() settle ACCEPTED; end

    # Reject a received message that is considered invalid and should never
    # be delivered again to this or any other receiver.
    def reject() settle REJECTED; end

    # Release a received message. It may be delivered again to this or another
    # receiver.
    #
    # @option opts [Boolean] :failed (default true) If true
    # {Message#delivery_count} will be increased so future receivers will know
    # there was a failed delivery. If false, {Message#delivery_count} will not
    # be increased.
    #
    # @option opts [Boolean] :undeliverable (default false) If true the message
    # will not be re-delivered to this receiver. It may be delivered tbo other
    # receivers.
    #
    # @option opts [Hash] :annotations Annotations to be added to
    # {Message#annotations} before re-delivery. Entries with the same key
    # replace existing entries in {Message#annotations}
    def release(opts = nil)
      opts = { :failed => true } if opts == true # Backwards compatibility
      failed = opts ? opts.fetch(:failed, true) : true
      undeliverable = opts && opts[:undeliverable]
      annotations = opts && opts[:annotations]
      if failed || undeliverable || annotations
        d = Cproton.pn_delivery_local(@impl)
        Cproton.pn_disposition_set_failed(d) if failed
        Cproton.pn_disposition_set_undeliverable(d) if undeliverable
        Data.from_object(Cproton.pn_disposition_annotations(d), annotations) if annotations
        settle(MODIFIED)
      else
        settle(RELEASED)
      end
    end

    # @deprecated use {#release} with modification options
    def modify()
      Qpid.deprecated __method__, "#release"
      release failed=>true
    end

    # @return [Boolean] True if the transfer was aborted by the sender.
    proton_caller :aborted?

    # @return true if the incoming message is complete, call {#message} to retrieve it.
    def complete?() readable? && !aborted? && !partial?; end

    # Get the message from the delivery.
    # @raise [ProtonError] if the message is not {#complete?} or there is an
    # error decoding the message.
    def message
      return @message if @message
      raise ProtonError("message aborted by sender") if aborted?
      raise ProtonError("incoming message incomplete") if partial?
      raise ProtonError("no incoming message") unless readable?
      @message = Message.new
      @message.decode(link.receive(pending))
      link.advance
      @message
    end

  end
end
