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

    # Reject a message, indicating to the sender that is invalid and should
    # never be delivered again to this or any other receiver.
    def reject() settle REJECTED; end

    # Release a message, indicating to the sender that it was not processed
    # but may be delivered again to this or another receiver.
    #
    # @param mods [Hash] Instructions to the sender to modify re-delivery.
    #  To allow re-delivery with no modifications at all use +release(nil)+
    #
    # @option mods [Boolean] :failed Instruct the sender to increase
    #  {Message#delivery_count} so future receivers will know there was a
    #  previous failed delivery.
    #
    # @option mods [Boolean] :undeliverable Instruct the sender that this
    #  message should never be re-delivered to this receiver, although it may be
    #  delivered other receivers.
    #
    # @option mods [Hash] :annotations Instruct the sender to update the
    #  {Message#annotations} with these +key=>value+ pairs before re-delivery.
    def release(mods = {:failed=>true})
      mods = { :failed => true } if mods == true # Backwards compatibility
      if !mods || mods.empty?
        settle(RELEASED)
      else
        d = Cproton.pn_delivery_local(@impl)
        Cproton.pn_disposition_set_failed(mods[:failed])
        Cproton.pn_disposition_set_undeliverable(mods[:undeliverable])
        Data.from_object(Cproton.pn_disposition_annotations(d), mods[:annotations]) if mods.key? :annotations
        settle(MODIFIED)
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
