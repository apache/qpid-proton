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

  # Endpoint is the parent classes for Link and Session.
  #
  # It provides a namespace for constant values that relate to the current
  # state of both links and sessions.
  #
  # @example
  #
  #   conn = Qpid::Proton::Connection.new
  #   puts "Local connection flags : #{conn.state || Qpid::Proton::Endpoint::LOCAL_MASK}"
  #   puts "Remote connection flags: #{conn.state || Qpid::Proton::Endpoint::REMOTE_MASK}"
  #
  class Endpoint

    # The local connection is uninitialized.
    LOCAL_UNINIT = Cproton::PN_LOCAL_UNINIT
    # The local connection is active.
    LOCAL_ACTIVE = Cproton::PN_LOCAL_ACTIVE
    # The local connection is closed.
    LOCAL_CLOSED = Cproton::PN_LOCAL_CLOSED

    # The remote connection is unitialized.
    REMOTE_UNINIT = Cproton::PN_REMOTE_UNINIT
    # The remote connection is active.
    REMOTE_ACTIVE = Cproton::PN_REMOTE_ACTIVE
    # The remote connection is closed.
    REMOTE_CLOSED = Cproton::PN_REMOTE_CLOSED

    # Bitmask for the local-only flags.
    LOCAL_MASK = Cproton::PN_LOCAL_UNINIT |
                 Cproton::PN_LOCAL_ACTIVE |
                 Cproton::PN_LOCAL_CLOSED

    # Bitmask for the remote-only flags.
    REMOTE_MASK = Cproton::PN_REMOTE_UNINIT |
                  Cproton::PN_REMOTE_ACTIVE |
                  Cproton::PN_REMOTE_CLOSED

    # @private
    include Util::Engine

    # @private
    def initialize
      @condition = nil
    end

    # @private
    def _update_condition
      object_to_condition(@condition, self._local_condition)
    end

    # @private
    def remote_condition
      condition_to_object(self._remote_condition)
    end

    # Return the transport associated with this endpoint.
    #
    # @return [Transport] The transport.
    #
    def transport
      self.connection.transport
    end

    def local_uninit?
      check_state(LOCAL_UNINIT)
    end

    def local_active?
      check_state(LOCAL_ACTIVE)
    end

    def local_closed?
      check_state(LOCAL_CLOSED)
    end

    def remote_uninit?
      check_state(REMOTE_UNINIT)
    end

    def remote_active?
      check_state(REMOTE_ACTIVE)
    end

    def remote_closed?
      check_state(REMOTE_CLOSED)
    end

    def check_state(state_mask)
      !(self.state & state_mask).zero?
    end

    def handler
      reactor = Qpid::Proton::Reactor::Reactor.wrap(Cproton.pn_object_reactor(@impl))
      if reactor.nil?
        on_error = nil
      else
        on_error = reactor.method(:on_error)
      end
      record = self.attachments
      puts "record=#{record}"
      WrappedHandler.wrap(Cproton.pn_record_get_handler(record), on_error)
    end

    def handler=(handler)
      reactor = Qpid::Proton::Reactor::Reactor.wrap(Cproton.pn_object_reactor(@impl))
      if reactor.nil?
        on_error = nil
      else
        on_error = reactor.method(:on_error)
      end
      impl = chandler(handler, on_error)
      record = self.attachments
      Cproton.pn_record_set_handler(record, impl)
      Cproton.pn_decref(impl)
    end

  end

end
