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

    class MultiHandler
      def self.maybe(h)
        a = Array(h)
        a.size > 1 ? self.new(h) : h
      end

      def initialize(a)
        @a = a;
        @options = {}
        @methods = Set.new
        @a.each do |h|
          @methods.merge(h.methods.select { |m| m.to_s.start_with?("on_") })
          @options.merge(h.options) do |k, a, b|
            raise ArgumentError, "handlers have conflicting option #{k} => #{a} != #{b}"
          end
        end
      end

      attr_reader :options

      def method_missing(name, *args)
        if respond_to_missing?(name)
          @a.each { |h| h.__send__(name, *args) if h.respond_to? name}
        else
          super
        end
      end
      def respond_to_missing?(name, private=false); @methods.include?(name); end
      def respond_to?(name, all=false) super || respond_to_missing?(name); end # For ruby < 1.9.2
    end

    # Base adapter
    class Adapter
      def initialize(h)
        @handler = MultiHandler.maybe h
      end

      def self.adapt(h)
        if h.respond_to? :proton_event_adapter
          a = h.proton_event_adapter
          a = a.new(h) if a.is_a? Class
          a
        else
          OldMessagingAdapter.new h
        end
      end

      # Adapter is already an adapter
      def proton_event_adapter() self; end

      def dispatch(method, *args)
        (@handler.__send__(method, *args); true) if @handler.respond_to? method
      end
    end
  end
end
