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

  # Tracks the status of a sent message.
  class Tracker < Transfer
    # @return [Sender] The parent {Sender} link.
    def sender() link; end

    # If {#state} == {#MODIFIED} this method returns additional information
    # about re-delivery from the receiver's call to {Delivery#release}
    #
    # @return [Hash] See {Delivery#release} options for the meaning of hash entries.
    def modified()
      return unless (state == MODIFIED) && (d = Cproton.pn_delivery_remote(@impl))
      {
       :failed => Cproton.pn_disposition_get_failed(d),
       :undeliverable => Cproton.pn_disposition_get_undeliverable(d),
       :annotations => Data.to_object(Cproton.pn_disposition_annotations(d))
      }
    end
  end

end
