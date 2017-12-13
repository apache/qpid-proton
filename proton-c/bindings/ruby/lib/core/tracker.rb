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

  # Track the {Transfer::State} of a sent message.
  class Tracker < Transfer
    # @return [Sender] The parent {Sender} link.
    def sender() link; end

    # Re-delivery modifications sent by the receiver in {Delivery#release}
    # @return [Hash] See the {Delivery#release} +opts+ parameter.
    # @return [nil] If no modifications were requested by the receiver.
    def modifications()
      return nil if (state != MODIFIED)
      d = Cproton.pn_delivery_remote(@impl)
      {
       :failed => Cproton.pn_disposition_is_failed(d),
       :undeliverable => Cproton.pn_disposition_is_undeliverable(d),
       :annotations => Codec::Data.to_object(Cproton.pn_disposition_annotations(d))
      }
    end

    # Abort a partially-sent message.
    # The tracker can no longer be used after calling {#abort}.
    def abort()
      Cproton.pn_delivery_abort(@impl)
    end
  end
end
