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

  # Provides a means for defining constant values within the namespace
  # of a class.
  #
  # If the class has defined the class method, :post_add_constant, then that
  # method will be invoked after each new item is added. It must be defined
  # *before* any constants are defined.
  #
  # ==== Example
  #
  #   class GrammarComponent
  #
  #     include Qpid::Proton::Constants
  #
  #     def self.post_add_constant(key, value)
  #       @terminal << value if value.terminal?
  #       @nonterminal << value if !value.terminal? && !value.rule
  #       @rule << value if value.rule
  #     end
  #
  #     self.add_constant :LEFT_PARENTHESIS, new GrammarComponent("(", :terminal)
  #     self.add_constant :RIGHT_PARENTHESIS, new GrammarComponent(")", :terminal)
  #     self.add_constant :ELEMENT, new GrammarComponent("E", :rule)
  #
  #     def initialize(component, type)
  #       @component = component
  #       @type = type
  #     end
  #
  #     def terminal?; @type == :terminal; end
  #
  #     def rule?; @type == :rule; end
  #
  #   end
  #
  # @private
  #
  module Constants

    def self.included(base)
      base.extend ClassMethods
    end

    module ClassMethods

      def add_constant(key, value)
        self.const_set(key, value)

        @pn_by_value ||= {}
        @pn_by_value[value] = key

        if self.respond_to? :post_add_constant
          self.post_add_constant(key, value)
        end
      end

      def by_value(value)
        (@pn_by_value || {})[value]
      end

    end

  end

end
