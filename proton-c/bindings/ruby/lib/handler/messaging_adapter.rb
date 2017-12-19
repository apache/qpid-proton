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


# @private
module Qpid::Proton
  module Handler

    # Adapt raw proton events to {MessagingHandler} events.
    class MessagingAdapter  < Adapter

      def delegate(method, *args)
        forward(method, *args) or forward(:on_unhandled, method, *args)
      end

      def delegate_error(method, context)
        unless forward(method, context) || forward(:on_error, context.condition)
          forward(:on_unhandled, method, context)
          # Close the whole connection on an un-handled error
          context.connection.close(context.condition)
        end
      end

      def on_container_start(container) delegate(:on_container_start, container); end
      def on_container_stop(container) delegate(:on_container_stop, container); end

      # Define repetative on_xxx_open/close methods for each endpoint type
      def self.open_close(endpoint)
        Module.new do
          define_method(:"on_#{endpoint}_remote_open") do |event|
            begin
              delegate(:"on_#{endpoint}_open", event.context)
              event.context.open if event.context.local_uninit?
            rescue StopAutoResponse
            end
          end

          define_method(:"on_#{endpoint}_remote_close") do |event|
            delegate_error(:"on_#{endpoint}_error", event.context) if event.context.condition
            begin
              delegate(:"on_#{endpoint}_close", event.context)
              event.context.close if event.context.local_active?
            rescue StopAutoResponse
            end
          end
        end
      end
      # Generate and include open_close modules for each endpoint type
      # Using modules so we can override to extend the behavior later in the handler.
      [:connection, :session, :link].each { |endpoint| include open_close(endpoint) }

      def on_transport_error(event)
        delegate_error(:on_transport_error, event.context)
      end

      def on_transport_closed(event)
        delegate(:on_transport_close, event.context) rescue StopAutoResponse
      end

      # Add flow control for link opening events
      def on_link_local_open(event) add_credit(event); end
      def on_link_remote_open(event) super; add_credit(event); end


      def on_delivery(event)
        if event.link.receiver?       # Incoming message
          d = event.delivery
          if d.aborted?
            delegate(:on_delivery_abort, d)
          elsif d.complete?
            if d.link.local_closed? && d.receiver.auto_accept
              d.release         # Auto release after close
            else
              begin
                delegate(:on_message, d, d.message)
                d.accept if d.receiver.auto_accept  && !d.settled?
              rescue Reject
                d.reject unless d.settled?
              rescue Release
                d.release unless d.settled?
              end
            end
          end
          delegate(:on_delivery_settle, d) if d.settled?
          add_credit(event)
        else                      # Outgoing message
          t = event.tracker
          case t.state
          when Delivery::ACCEPTED then delegate(:on_tracker_accept, t)
          when Delivery::REJECTED then delegate(:on_tracker_reject, t)
          when Delivery::RELEASED then delegate(:on_tracker_release, t)
          when Delivery::MODIFIED then delegate(:on_tracker_modify, t)
          end
          delegate(:on_tracker_settle, t) if t.settled?
          t.settle if t.sender.auto_settle
        end
      end

      def on_link_flow(event)
        add_credit(event)
        sender = event.sender
        delegate(:on_sendable, sender) if sender && sender.open? && sender.credit > 0
      end

      def add_credit(event)
        return unless (r = event.receiver)
        if r.open? && (r.drained == 0) && r.credit_window && (r.credit_window > r.credit)
          r.flow(r.credit_window - r.credit)
        end
      end
    end
  end
end
