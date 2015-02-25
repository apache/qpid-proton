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

module Qpid::Proton::Handler

  # Mixing that provides methods for acknowledging a delivery.
  #
  module Acking

    # Accept the receivered message.
    #
    # @param delivery [Qpid::Proton::Delivery] The delivery.
    #
    def accept(delivery)
      self.settle(delivery, Qpid::Proton::Delivery::ACCEPTED)
    end

    # Rejects a received message that is considered invalid or unprocessable.
    #
    # @param delivery [Qpid::Proton::Delivery] The delivery.
    #
    def reject(delivery)
      self.settle(delivery, Qpid::Proton::Delivery::REJECTED)
    end

    # Releases a received message, making it available at the source for any
    # other interested receiver.
    #
    # @param delivery [Qpid::Proton::Delivery] The delivery
    # @param delivered [Boolean] True if this was considered a delivery
    #   attempt.
    #
    def release(delivery, delivered = true)
      if delivered
        self.settle(delivery, Qpid::Proton::Delivery::MODIFIED)
      else
        self.settle(delivery, Qpid::Proton::Delivery::RELEASED)
      end
    end

    # Settles the specified delivery. Updates the delivery state if a state
    # is specified.
    #
    # @param delivery [Qpid::Proton::Delivery] The delivery.
    # @param state [Fixnum] The delivery state.
    #
    def settle(delivery, state = nil)
      delivery.update(state) unless state.nil?
      delivery.settle
    end

  end

end
