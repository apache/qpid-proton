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

  # A utility for simpler and more intuitive handling of delivery events
  # related to outgoing messages.
  #
  # Uses {#@auto_settle}
  module OutgoingMessageHandler

    def on_link_flow(event)
      super
      self.on_sendable(event) if event.link.sender? && event.link.credit > 0 &&
                                 (event.link.state & Qpid::Proton::Endpoint::LOCAL_ACTIVE) &&
                                 (event.link.state & Qpid::Proton::Endpoint::REMOTE_ACTIVE)
    end

    def on_delivery(event)
      super
      delivery = event.delivery
      if delivery.link.sender? && delivery.updated?
        if delivery.remote_accepted?
          self.on_accepted(event)
        elsif delivery.remote_rejected?
          self.on_rejected(event)
        elsif delivery.remote_released? || delivery.remote_modified?
          self.on_released(event)
        end
        self.on_settled(event) if delivery.settled?
        delivery.settle if @auto_settle
      end
    end
  end
end
