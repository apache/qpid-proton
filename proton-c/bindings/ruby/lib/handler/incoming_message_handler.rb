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

# @private
module Qpid::Proton::Handler
  private

  # A utility for simpler and more intuitive handling of delivery events
  # related to incoming messages.
  #
  # uses @auto_accept
  #
  module IncomingMessageHandler
    def on_delivery(event)
      super
      delivery = event.delivery
      return unless delivery.link.receiver?
      if delivery.readable? && !delivery.partial?
        m = Qpid::Proton::Message.new
        m.receive(delivery)
        event.message = m
        if event.link.local_closed?
          delivery.settle Qpid::Proton::Delivery::RELEASED if @auto_accept
        else
          begin
            self.on_message(event)
            delivery.settle Qpid::Proton::Delivery::ACCEPTED if @auto_accept
          rescue Qpid::Proton::Reject
            delivery.settle REJECTED
          rescue Qpid::Proton::Release
            delivery.settle MODIFIED
          end
        end
      elsif delivery.updated? && delivery.settled?
        self.on_settled(event)
      end
    end
  end
end
