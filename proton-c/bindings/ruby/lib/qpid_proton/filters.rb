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

    module Filters

      def self.included(base)
        base.class_eval do
          extend ClassMethods
        end
      end

      module ClassMethods

        def method_added(method_name)
          @@hooked_methods ||= []
          return if @@hooked_methods.include?(method_name)
          @@hooked_methods << method_name
          hooks = @@before_hooks[method_name]
          return if hooks.nil?
          orig_method = instance_method(method_name)
          define_method(method_name) do |*args, &block|
            hooks = @@before_hooks[method_name]
            hooks.each do |hook|
              method(hook).call
            end

            orig_method.bind(self).call(*args, &block)
          end
        end

        def call_before(before_method, *methods)
          @@before_hooks ||= {}
          methods.each do |method|
            hooks = @@before_hooks[method] || []
            raise "Repeat filter: #{before_method}" if hooks.include? before_method
            hooks << before_method
            @@before_hooks[method] = hooks
          end
        end

      end

    end

  end

end
