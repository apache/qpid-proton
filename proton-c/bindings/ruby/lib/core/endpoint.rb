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
    include Util::Deprecation

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
    LOCAL_MASK = Cproton::PN_LOCAL_UNINIT | Cproton::PN_LOCAL_ACTIVE | Cproton::PN_LOCAL_CLOSED

    # Bitmask for the remote-only flags.
    REMOTE_MASK = Cproton::PN_REMOTE_UNINIT | Cproton::PN_REMOTE_ACTIVE | Cproton::PN_REMOTE_CLOSED

    # @private
    def condition; remote_condition || local_condition; end
    # @private
    def remote_condition; Condition.convert(_remote_condition); end
    # @private
    def local_condition; Condition.convert(_local_condition); end

    # Return the transport associated with this endpoint.
    #
    # @return [Transport] The transport.
    #
    def transport
      self.connection.transport
    end

    # @return [WorkQueue] the work queue for work on this endpoint.
    def work_queue() connection.work_queue; end

    # @private
    # @return [Bool] true if {#state} has all the bits of `mask` set
    def check_state(mask) (self.state & mask) == mask; end

    # @return [Bool] true if endpoint has sent and received a CLOSE frame
    def closed?() check_state(LOCAL_CLOSED | REMOTE_CLOSED); end

    # @return [Bool] true if endpoint has sent and received an OPEN frame
    def open?() check_state(LOCAL_ACTIVE | REMOTE_ACTIVE); end

    def local_uninit?
      check_state(LOCAL_UNINIT)
    end

    def local_open?
      check_state(LOCAL_ACTIVE)
    end

    def local_closed?
      check_state(LOCAL_CLOSED)
    end

    def remote_uninit?
      check_state(REMOTE_UNINIT)
    end

    def remote_open?
      check_state(REMOTE_ACTIVE)
    end

    def remote_closed?
      check_state(REMOTE_CLOSED)
    end

    alias local_active? local_open?
    alias remote_active? remote_open?

  end
end
