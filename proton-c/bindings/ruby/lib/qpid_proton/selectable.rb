#
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
#

module Qpid

  module Proton

    # Selectable enables accessing the underlying file descriptors
    # for Messenger.
    class Selectable

      include Qpid::Proton::Filters

      call_before :check_is_initialized,
      :fileno, :capacity, :pending, :deadline,
      :readable, :writable, :expired,
      :registered=, :registered?

      def initialize(messenger, impl) # :nodoc:
        @messenger = messenger
        @impl = impl
        @io = nil
        @freed = false
      end

      # Returns the underlying file descriptor.
      #
      # This can be used in conjunction with the IO class.
      #
      def fileno
        Cproton.pn_selectable_fd(@impl)
      end

      def to_io
        @io ||= IO.new(fileno)
      end

      # The number of bytes the selectable is capable of consuming.
      #
      def capacity
        Cproton.pn_selectable_capacity(@impl)
      end

      # The number of bytes waiting to be written to the file descriptor.
      #
      def pending
        Cproton.pn_selectable_pending(@impl)
      end

      # The future expiry time at which control will be returned to the
      # selectable.
      #
      def deadline
        tstamp = Cproton.pn_selectable_deadline(@impl)
        tstamp.nil? ? nil : tstamp / 1000
      end

      def readable
        Cproton.pn_selectable_readable(@impl)
      end

      def writable
        Cproton.pn_selectable_writable(@impl)
      end

      def expired?
        Cproton.pn_selectable_expired(@impl)
      end

      def registered=(registered)
        Cproton.pn_selectable_set_registered(@impl, registered)
      end

      def registered?
        Cproton.pn_selectable_is_registered(@impl)
      end

      def terminal?
        return true if @impl.nil?
        Cproton.pn_selectable_is_terminal(@impl)
      end

      def to_s
        "fileno=#{self.fileno} registered=#{self.registered?} terminal=#{self.terminal?}"
      end

      def free
        return if @freed
        @freed = true
        @messenger.unregister_selectable(fileno)
        @io.close unless @io.nil?
        Cproton.pn_selectable_free(@impl)
        @impl = nil
      end

      def freed? # :nodoc:
        @freed
      end

      private

      def check_is_initialized
        raise RuntimeError.new("selectable freed") if @impl.nil?
      end

    end

  end

end
