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

  # Provides mixin functionality for dealing with exception conditions.
  #
  # @private
  module ErrorHandler

    def self.included(base)
      base.extend(self)

      unless defined? base.to_be_wrapped
        class << base
          @@to_be_wrapped = []
        end
      end

      define_method :method_added do |name|
        if (!@@to_be_wrapped.nil?) && (@@to_be_wrapped.include? name)
          @@to_be_wrapped.delete name
          create_exception_handler_wrapper(name)
        end
      end
    end

    def can_raise_error(method_names, options = {})
      error_class = options[:error_class]
      below = options[:below] || 0
      # coerce the names to be an array
      Array(method_names).each do |method_name|
        # if the method doesn't already exist then queue this aliasing
        unless self.method_defined? method_name
          @@to_be_wrapped ||= []
          @@to_be_wrapped << method_name
        else
          create_exception_handler_wrapper(method_name, error_class, below)
        end
      end
    end

    def create_exception_handler_wrapper(method_name, error_class = nil, below = 0)
      original_method_name = method_name.to_s
      wrapped_method_name = "_excwrap_#{original_method_name}"
      alias_method wrapped_method_name, original_method_name
      define_method original_method_name do |*args, &block|
        # need to get a reference to the method object itself since
        # calls to Class.send interfere with Messenger.send
        method = self.method(wrapped_method_name.to_sym)
        rc = method.call(*args, &block)
        check_for_error(rc, error_class) if rc < below
        return rc
      end
    end

    # Raises an Proton-specific error if a return code is non-zero.
    #
    # Expects the class to provide an +error+ method.
    def check_for_error(code, error_class = nil)

      raise ::ArgumentError.new("Invalid error code: #{code}") if code.nil?

      return code if code > 0

      case(code)

      when Qpid::Proton::Error::NONE
        return

      when Qpid::Proton::Error::EOS
        raise Qpid::Proton::EOSError.new(self.error)

      when Qpid::Proton::Error::ERROR
        raise Qpid::Proton::ProtonError.new(self.error)

      when Qpid::Proton::Error::OVERFLOW
        raise Qpid::Proton::OverflowError.new(self.error)

      when Qpid::Proton::Error::UNDERFLOW
        raise Qpid::Proton::UnderflowError.new(self.error)

      when Qpid::Proton::Error::ARGUMENT
        raise Qpid::Proton::ArgumentError.new(self.error)

      when Qpid::Proton::Error::STATE
        raise Qpid::Proton::StateError.new(self.error)

      when Qpid::Proton::Error::TIMEOUT
        raise Qpid::Proton::TimeoutError.new(self.error)

      when Qpid::Proton::Error::INPROGRESS
        return

      when Qpid::Proton::Error::INTERRUPTED
        raise Qpid::Proton::InterruptedError.new(self.error)

      when Qpid::Proton::Error::INPROGRESS
        raise Qpid::Proton::InProgressError.new(self.error)

      else

        raise ::ArgumentError.new("Unknown error code: #{code}")

      end

    end

  end

end
