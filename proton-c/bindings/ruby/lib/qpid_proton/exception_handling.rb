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

    # Provides mixin functionality for dealing with exception conditions.
    #
    module ExceptionHandling

      # Raises an Proton-specific error if a return code is non-zero.
      #
      # Expects the class to provide an +error+ method.
      def check_for_error(code)

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

end
