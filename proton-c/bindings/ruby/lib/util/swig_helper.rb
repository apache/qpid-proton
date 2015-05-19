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

module Qpid::Proton::Util

  # Provides helper functions for writing wrapper functions for the
  # underlying C APIs.
  #
  # Before defining any mutators the class must define the name of the
  # prefix for methods with the constant PROTON_METOD_PREFIX.
  #
  # == Mutators, Setters And Getters
  #
  # There are three types of wrappers that are supported:
  #
  # [proton_writer] Defines a set-only method for the named attribute.
  # [proton_reader] Defines a get-only method for the named attribute.
  # [proton_accessor] Defines both a set- and a get-method for the named
  #                  attribute.
  # [proton_caller] A simple wrapper for calling an underlying method,
  #                  avoids repetitive boiler plate coding.
  #
  # == Arguments
  #
  # [:is_or_get => {:is, :get}] For both the getter and the mutator types
  # you can also declare that the method uses "is" instead of "get" in the
  # underlying API. Such methods are then defined with "?"
  #
  # @example
  #   class Terminus
  #
  #     include WrapperHelper
  #
  #     PROTON_METHOD_PREFIX = "pn_terminus"
  #
  #     # add methods "type" and "type=" that call "pn_terminus_{get,set}_type"
  #     proton_accessor :type
  #
  #     # adds the method "dynamic?" that calls "pn_terminus_is_dynamic"
  #     proton_accessor :dynamic, :is_or_get => :is
  #
  #     # adds a method named "foo" that calls "pn_terminus_foo"
  #     proton_caller :foo
  #
  #   end
  #
  # @private
  module SwigHelper

    def self.included(base)
      base.extend ClassMethods
    end

    module ClassMethods # :nodoc:

      def create_wrapper_method(name, proton_method, with_arg = false)
        if with_arg
          define_method "#{name}" do |arg|
            Cproton.__send__(proton_method.to_sym, @impl, arg)
          end
        else
          define_method "#{name}" do
            Cproton.__send__(proton_method.to_sym, @impl)
          end
        end
      end

      # Defines a method that calls an underlying C library function.
      def proton_caller(name, options = {})
        proton_method = "#{self::PROTON_METHOD_PREFIX}_#{name}"
        # drop the trailing '?' if this is a property method
        proton_method = proton_method[0..-2] if proton_method.end_with? "?"
        create_wrapper_method(name, proton_method)
      end

      def proton_writer(name, options = {})
        proton_method = "#{self::PROTON_METHOD_PREFIX}_set_#{name}"
        create_wrapper_method("#{name}=", proton_method, true)
      end

      def proton_reader(name, options = {})
        an_is_method = options[:is_or_get] == :is
        prefix = (an_is_method) ? "is" : "get"
        proton_method = "#{self::PROTON_METHOD_PREFIX}_#{prefix}_#{name}"
        name = "#{name}?" if an_is_method
        create_wrapper_method(name, proton_method)
      end

      def proton_accessor(name, options = {})
        proton_writer(name, options)
        proton_reader(name, options)
      end

    end

  end

end
