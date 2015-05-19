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

module Qpid::Proton::Event

  # A Collector is used to register interest in events produced by one
  # or more Connection objects.
  #
  # == Events
  #
  # @see Qpid::Proton::Event The list of predefined events.
  #
  # @example
  #
  #   conn = Qpid::Proton::Connection.new
  #   coll = Qpid::Proton::Event::Collector.new
  #   conn.collect(coll)
  #
  #   # transport setup not included here for brevity
  #
  #   loop do
  #
  #      # wait for an event and then perform the following
  #
  #      event = collector.peek
  #
  #      unless event.nil?
  #        case event.type
  #
  #        when Qpid::Proton::Event::CONNECTION_REMOTE_CLOSE
  #          conn = event.context # the context here is the connection
  #          # the remote connection closed, so only close our side if it's
  #          # still open
  #          if !(conn.state & Qpid::Proton::Endpoint::LOCAL_CLOSED)
  #            conn.close
  #          end
  #
  #        when Qpid::proton::Event::SESSION_REMOTE_OPEN
  #          session = event.session # the context here is the session
  #          # the remote session is now open, so if the local session is
  #          # uninitialized, then open it
  #          if session.state & Qpid::Proton::Endpoint::LOCAL_UNINIT
  #            session.incoming_capacity = 1000000
  #            session.open
  #          end
  #
  #        end
  #
  #       # remove the processed event and get the next event
  #       # the loop will exit when we have no more events to process
  #       collector.pop
  #       event = collector.peek
  #
  #   end
  #
  class Collector

    # @private
    attr_reader :impl

    # Creates a new Collector.
    #
    def initialize
      @impl = Cproton.pn_collector
      ObjectSpace.define_finalizer(self, self.class.finalize!(@impl))
    end

    # @private
    def self.finalize!(impl)
      proc {
        Cproton.pn_collector_free(impl)
      }
    end

    # Releases the collector.
    #
    # Once in a released state, a collector will drain any internally queued
    # events, shrink its memory footprint to a minimu, and discard any newly
    # created events.
    #
    def release
      Cproton.pn_collector_release(@impl)
    end

    # Place a new event on the collector.
    #
    # This operation will create a new event of the given type and context
    # and return a new Event instance. In some cases an event of a given
    # type can be elided. When this happens, this operation will return
    # nil.
    #
    # @param context [Object] The event context.
    # @param event_type [EventType] The event type.
    #
    # @return [Event] the event if it was queued
    # @return [nil] if it was elided
    #
    def put(context, event_type)
      Cproton.pn_collector_put(@impl, Cproton.pn_rb2void(context), event_type.type_code)
    end

    # Access the head event.
    #
    # This operation will continue to return the same event until it is
    # cleared by using #pop. The pointer return by this  operation will be
    # valid until ::pn_collector_pop is invoked or #free is called, whichever
    # happens sooner.
    #
    # @return [Event] the head event
    # @return [nil] if there are no events
    #
    # @see #pop
    # @see #put
    #
    def peek
      Event.wrap(Cproton.pn_collector_peek(@impl))
    end

    # Clear the head event.
    #
    # @return [Boolean] true if an event was removed
    #
    # @see #release
    # @see #peek
    #
    def pop
      Cproton.pn_collector_pop(@impl)
    end

  end

end
