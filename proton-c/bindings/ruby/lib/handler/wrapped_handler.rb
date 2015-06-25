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

  class WrappedHandler

    # @private
    include Qpid::Proton::Util::Wrapper

    def self.wrap(impl, on_error = nil)
      return nil if impl.nil?

      result = self.fetch_instance(impl) || WrappedHandler.new(impl)
      result.on_error = on_error
      return result
    end

    include Qpid::Proton::Util::Handler

    def initialize(impl_or_constructor)
      if impl_or_constructor.is_a?(Method)
        @impl = impl_or_constructor.call
      else
        @impl = impl_or_constructor
        Cproton.pn_incref(@impl)
      end
      @on_error = nil
      self.class.store_instance(self)
    end

    def add(handler)
      return if handler.nil?

      impl = chandler(handler, self.method(:_on_error))
      Cproton.pn_handler_add(@impl, impl)
      Cproton.pn_decref(impl)
    end

    def clear
      Cproton.pn_handler_clear(@impl)
    end

    def on_error=(on_error)
      @on_error = on_error
    end

    private

    def _on_error(info)
      if self.has?['on_error']
        self['on_error'].call(info)
      else
        raise info
      end
    end

  end

end
