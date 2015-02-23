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

module Qpid::Proton::Reactor

  class Reactor

    include Qpid::Proton::Util::Handler

    # @private
    include Qpid::Proton::Util::SwigHelper

    # @private
    PROTON_METHOD_PREFIX = "pn_reactor"

    proton_caller :yield

    proton_caller :mark

    proton_caller :start

    proton_caller :stop

    # @private
    include Qpid::Proton::Util::Timeout

    include Qpid::Proton::Util::Wrapper

    attr_reader :errors

    def self.wrap(impl)
      return nil if impl.nil?

      self.fetch_instance(impl, :pn_reactor_attachments) || Reactor.new(nil, :impl => impl)
    end

    def initialize(handlers, options = {})
      @impl = options[:impl]
      if @impl.nil?
        @impl = Cproton.pn_reactor
      end
      if !handlers.nil?
        [handlers].flatten.each {|handler| self.handler.add(handler)}
      end
      @errors = []
      @handlers = []
      self.class.store_instance(self, :pn_reactor_attachments)
    end

    # Returns whether the reactor has any unbuffered data.
    #
    # @return [Boolean] True if there is no unbuffered data.
    #
    def quiesced?
      Cproton.pn_reactor_quiesced(@impl)
    end

    def on_error(info)
      self.errors << info
      self.yield
    end

    def global_handler
      impl = Cproton.pn_reactor_get_global_handler(@impl)
      Qpid::Proton::Handler::WrappedHandler.wrap(impl, self.method(:on_error))
    end

    def global_handler=(handler)
      impl = chandler(handler, self.method(:on_error))
      Cproton.pn_reactor_set_global_handler(@impl, impl)
      Cproton.pn_decref(impl)
    end

    # Returns the timeout period.
    #
    # @return [Fixnum] The timeout period, in seconds.
    #
    def timeout
      millis_to_timeout(Cproton.pn_reactor_get_timeout(@impl))
    end

    # Sets the timeout period.
    #
    # @param timeout [Fixnum] The timeout, in seconds.
    #
    def timeout=(timeout)
      Cproton.pn_reactor_set_timeout(@impl, timeout_to_millis(timeout))
    end

    def handler
      impl = Cproton.pn_reactor_get_handler(@impl)
      Qpid::Proton::Handler::WrappedHandler.wrap(impl, self.method(:on_error))
    end

    def handler=(handler)
      impl = chandler(handler, set.method(:on_error))
      Cproton.pn_reactor_set_handler(@impl, impl)
      Cproton.pn_decref(impl)
    end

    def run(&block)
      self.timeout = 3.14159265359
      self.start
      while self.process do
        if block_given?
          yield
        end
      end
      self.stop
    end

    def wakeup
      n = Cproton.pn_reactor_wakeup(@impl)
      unless n.zero?
        io = Cproton.pn_reactor_io(@impl)
        raise IOError.new(Cproton.pn_io_error(io))
      end
    end

    def process
      result = Cproton.pn_reactor_process(@impl)
      if !self.errors.nil? && !self.errors.empty?
        (0...self.errors.size).each do |index|
          error_set = self.errors[index]
          print error.backtrace.join("\n")
        end
        raise self.errors.last
      end
      return result
    end

    def schedule(delay, task)
      impl = chandler(task, self.method(:on_error))
      task = Task.wrap(Cproton.pn_reactor_schedule(@impl, sec_to_millis(delay), impl))
      Cproton.pn_decref(impl)
      return task
    end

    def acceptor(host, port, handler = nil)
      impl = chandler(handler, self.method(:on_error))
      aimpl = Cproton.pn_reactor_acceptor(@impl, host, "#{port}", impl)
      Cproton.pn_decref(impl)
      if !aimpl.nil?
        return Acceptor.new(aimpl)
      else
        io = Cproton.pn_reactor_io(@impl)
        io_error = Cproton.pn_io_error(io)
        error_text = Cproton.pn_error_text(io_error)
        text = "(#{Cproton.pn_error_text(io_error)} (#{host}:#{port}))"
        raise IOError.new(text)
      end
    end

    def connection(handler = nil)
      impl = chandler(handler, self.method(:on_error))
      conn = Qpid::Proton::Connection.wrap(Cproton.pn_reactor_connection(@impl, impl))
      Cproton.pn_decref(impl)
      return conn
    end

    def selectable(handler = nil)
      impl = chandler(handler, self.method(:on_error))
      result = Selectable.wrap(Cproton.pn_reactor_selectable(@impl))
      if !impl.nil?
        record = Cproton.pn_selectable_attachments(result.impl)
        Cproton.pn_record_set_handler(record, impl)
        Cproton.pn_decref(impl)
      end
      return result
    end

    def update(sel)
      Cproton.pn_reactor_update(@impl, sel.impl)
    end

    def push_event(obj, etype)
      Cproton.pn_collector_put(Cproton.pn_reactor_collector(@impl), Qpid::Proton::Util::RBCTX, Cproton.pn_py2void(obj), etype.number)
    end

  end

end
