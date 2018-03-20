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

    # Adapter to convert raw proton events for the old {Handler::MessagingHandler}
    # used by the Reactor.
    class ReactorMessagingAdapter  < Adapter
      def initialize handler
        super
        @opts = (handler.options if handler.respond_to?(:options)) || {}
        @opts[:prefetch] ||= 10
        @opts[:peer_close_is_error] = false unless @opts.include? :peer_close_is_error
        [:auto_accept, :auto_settle, :auto_open, :auto_close].each do |k|
          @opts[k] = true unless @opts.include? k
        end
      end

      alias dispatch forward

      def delegate(method, event)
        event.method = method     # Update the event with the new method
        event.dispatch(@handler) or dispatch(:on_unhandled, event)
      end

      def delegate_error(method, event)
        event.method = method
        unless event.dispatch(@handler) || dispatch(:on_error, event)
          dispatch(:on_unhandled, event)
          event.connection.close(event.context.condition) if @opts[:auto_close]
        end
      end

      def on_container_start(container) delegate(:on_start, Event.new(nil, nil, container)); end
      def on_container_stop(container) delegate(:on_stop, Event.new(nil, nil, container)); end

      # Define repetative on_xxx_open/close methods for each endpoint type
      def self.open_close(endpoint)
        on_opening = :"on_#{endpoint}_opening"
        on_opened = :"on_#{endpoint}_opened"
        on_closing = :"on_#{endpoint}_closing"
        on_closed = :"on_#{endpoint}_closed"
        on_error = :"on_#{endpoint}_error"

        Module.new do
          define_method(:"on_#{endpoint}_local_open") do |event|
            delegate(on_opened, event) if event.context.remote_open?
          end

          define_method(:"on_#{endpoint}_remote_open") do |event|
            if event.context.local_open?
              delegate(on_opened, event)
            elsif event.context.local_uninit?
              delegate(on_opening, event)
              event.context.open if @opts[:auto_open]
            end
          end

          define_method(:"on_#{endpoint}_local_close") do |event|
            delegate(on_closed, event) if event.context.remote_closed?
          end

          define_method(:"on_#{endpoint}_remote_close") do |event|
            if event.context.remote_condition
              delegate_error(on_error, event)
            elsif event.context.local_closed?
              delegate(on_closed, event)
            elsif @opts[:peer_close_is_error]
              Condition.assign(event.context.__send__(:_remote_condition), "unexpected peer close")
              delegate_error(on_error, event)
            else
              delegate(on_closing, event)
            end
            event.context.close if @opts[:auto_close]
          end
        end
      end
      # Generate and include open_close modules for each endpoint type
      [:connection, :session, :link].each { |endpoint| include open_close(endpoint) }

      def on_transport_error(event) delegate_error(:on_transport_error, event); end
      def on_transport_closed(event) delegate(:on_transport_closed, event); end

      # Add flow control for link opening events
      def on_link_local_open(event) super; add_credit(event); end
      def on_link_remote_open(event) super; add_credit(event); end


      def on_delivery(event)
        if event.link.receiver?       # Incoming message
          d = event.delivery
          if d.aborted?
            delegate(:on_aborted, event)
            d.settle
          elsif d.complete?
            if d.link.local_closed? && @opts[:auto_accept]
              d.release
            else
              begin
                delegate(:on_message, event)
                d.accept if @opts[:auto_accept] && !d.settled?
              rescue Qpid::Proton::Reject
                d.reject
              rescue Qpid::Proton::Release
                d.release(true)
              end
            end
          end
          delegate(:on_settled, event) if d.settled?
          add_credit(event)
        else                      # Outgoing message
          t = event.tracker
          if t.updated?
            case t.state
            when Qpid::Proton::Delivery::ACCEPTED then delegate(:on_accepted, event)
            when Qpid::Proton::Delivery::REJECTED then delegate(:on_rejected, event)
            when Qpid::Proton::Delivery::RELEASED then delegate(:on_released, event)
            when Qpid::Proton::Delivery::MODIFIED then delegate(:on_modified, event)
            end
            delegate(:on_settled, event) if t.settled?
            t.settle if @opts[:auto_settle]
          end
        end
      end

      def on_link_flow(event)
        add_credit(event)
        l = event.link
        delegate(:on_sendable, event) if l.sender? && l.open? && l.credit > 0
      end

      def add_credit(event)
        r = event.receiver
        prefetch = @opts[:prefetch]
        if r && r.open? && (r.drained == 0) && prefetch && (prefetch > r.credit)
          r.flow(prefetch - r.credit)
        end
      end
    end
  end
end
