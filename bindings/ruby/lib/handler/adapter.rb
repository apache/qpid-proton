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

    def self.handler_method?(method) /^on_/ =~ name; end

    # Handler for an array of handlers of uniform type, with non-conflicting options
    class ArrayHandler

      def initialize(handlers)
        raise "empty handler array" if handlers.empty?
        adapters = (handlers.map { |h| Adapter.adapter(h) }).uniq
        raise "handler array not uniform, adapters requested: #{adapters}" if adapters.size > 1
        @proton_adapter_class = adapters[0]
        @methods = Set.new
        handlers.each do |h|
          @methods.merge(h.methods.select { |m| handler_method? m }) # Collect handler methods
        end
      end

      attr_reader :options, :proton_adapter_class

      def method_missing(name, *args)
        if respond_to_missing?(name)
          @adapters.each { |a| a.__send__(name, *args) if a.respond_to? name}
        else
          super
        end
      end

      def respond_to_missing?(name, private=false); @methods.include?(name); end
      def respond_to?(name, all=false) super || respond_to_missing?(name); end # For ruby < 1.9.2
    end

    # Base adapter for raw proton events
    class Adapter
      def initialize(h) @handler = h; end

      # Create and return an adapter for handler, or return h if it does not need adapting.
      def self.adapt(handler)
        return unless handler
        a = Array(handler)
        h = (a.size == 1) ? a[0] : ArrayHandler.new(a)
        ac = adapter(h)
        ac ? ac.new(h) : h
      end

      # Adapter class if requested by handler, else default to MessagingHandler
      def self.adapter(handler)
        handler.respond_to?(:proton_adapter_class) ? handler.proton_adapter_class : MessagingAdapter
      end

      def proton_adapter_class() nil; end # Adapters don't need adapting

      def forward(method, *args)
        (@handler.__send__(method, *args); true) if @handler.respond_to? method
      end
    end
  end
end
