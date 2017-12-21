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

module Qpid::Proton::Util
  # @private
  module Deprecation
    MATCH_DIR = /#{File.dirname(File.dirname(__FILE__))}/

    DEPRECATE_FULL_TRACE = false

    def self.deprecated(old, new=nil)
      replace = new ? "use `#{new}`" : "internal use only"

      from = DEPRECATE_FULL_TRACE ? caller(2).join("\n") : caller.find { |l| not MATCH_DIR.match(l) }
      warn "[DEPRECATION] `#{old}` is deprecated, #{replace}. Called from #{from}"
    end

    def deprecated(*arg) Deprecation.deprecated(*arg); end

    module ClassMethods
      def deprecated_alias(bad, good)
        bad, good = bad.to_sym, good.to_sym
        define_method(bad) do |*args, &block|
          self.deprecated bad, good
          self.__send__(good, *args, &block)
        end
      end
    end

    def self.included(other)
      other.extend ClassMethods
    end
  end
end
