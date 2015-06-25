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
  # related to incoming messages.
  #
  class IncomingMessageHandler < Qpid::Proton::BaseHandler

    include Acking

    def initialize(auto_accept = true, delegate = nil)
      @delegate = delegate
      @auto_accept = auto_accept
    end

    def on_delivery(event)
      delivery = event.delivery
      return unless delivery.link.receiver?
      if delivery.readable? && !delivery.partial?
        event.message = Qpid::Proton::Util::Engine.receive_message(delivery)
        if event.link.local_closed?
          if @auto_accept
            delivery.update(Qpid::Proton::Disposition::RELEASED)
            delivery.settle
          end
        else
          begin
            self.on_message(event)
            if @auto_accept
              delivery.update(Qpid::Proton::Disposition::ACCEPTED)
              delivery.settle
            end
          rescue Qpid::Proton::Reject
            delivery.update(Qpid::Proton::Disposition::REJECTED)
            delivery.settle
          rescue Qpid::Proton::Release
            delivery.update(Qpid::Proton::Disposition::MODIFIED)
            delivery.settle
          end
        end
      elsif delivery.updated? && delivery.settled?
        self.on_settled(event)
      end
    end

    def on_message(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_message, event) if !@delegate.nil?
    end

    def on_settled(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_settled, event) if !@delegate.nil?
    end

  end

end
