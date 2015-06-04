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

  # Selectable enables accessing the underlying file descriptors
  # for Messenger.
  #
  # @private
  class Selectable

    # @private
    include Util::SwigHelper

    # @private
    PROTON_METHOD_PREFIX = "pn_selectable"

    # Returns the underlying file descriptor.
    #
    # This can be used in conjunction with the IO class.
    #
    def fileno
      Cproton.pn_selectable_get_fd(@impl)
    end

    proton_reader :reading, :is_or_get => :is

    proton_reader :writing, :is_or_get => :is

    proton_caller :readable

    proton_caller :writable

    proton_caller :expired

    proton_accessor :registered, :is_or_get => :is

    proton_accessor :terminal, :is_or_get => :is

    proton_caller :terminate

    proton_caller :release

    # @private
    def self.wrap(impl)
      return nil if impl.nil?

      self.fetch_instance(impl, :pn_selectable_attachments) || Selectable.new(impl)
    end

    # @private
    include Util::Wrapper

    # @private
    def initialize(impl)
      @impl = impl
      self.class.store_instance(self, :pn_selectable_attachments)
    end

    private

    DEFAULT = Object.new

    public

    def fileno(fd = DEFAULT)
      if fd == DEFAULT
        Cproton.pn_selectable_get_fd(@impl)
      elsif fd.nil?
        Cproton.pn_selectable_set_fd(@impl, Cproton::PN_INVALID_SOCKET)
      else
        Cproton.pn_selectable_set_fd(@impl, fd)
      end
    end

    def reading=(reading)
      if reading.nil?
        reading = false
      elsif reading == "0"
        reading = false
      else
        reading = true
      end
      Cproton.pn_selectable_set_reading(@impl, reading ? true : false)
    end

    def writing=(writing)
      if writing.nil?
        writing = false
      elsif writing == "0"
        writing = false
      else
        writing = true
      end
      Cproton.pn_selectable_set_writing(@impl, writing ? true : false)
    end

    def deadline
      tstamp = Cproton.pn_selectable_get_deadline(@impl)
      return nil if tstamp.nil?
      mills_to_sec(tstamp)
    end

    def deadline=(deadline)
      Cproton.pn_selectable_set_deadline(sec_to_millis(deadline))
    end

    def to_io
      @io ||= IO.new(fileno)
    end

  end

end
