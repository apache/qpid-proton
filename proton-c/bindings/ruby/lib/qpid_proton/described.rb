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

module Qpid # :nodoc:

  module Proton # :nodoc:

    class Described

      attr_reader :descriptor
      attr_reader :value

      def initialize(descriptor, value)
        @descriptor = descriptor
        @value = value
      end

      # Retrieves the descriptor and value from the supplied Data object.
      #
      # ==== Arguments
      #
      # * data - the Qpid::Proton::Data instance
      #
      def self.get(data)
        type = data.next
        raise TypeError, "not a described type" unless type == Mapping.SYMBOL
        descriptor = data.symbol
        type = data.next
        raise TypeError, "not a described type" unless type == Mapping.STRING
        value = data.string
        Described.new(descriptor, value)
      end

      # Puts the description into the Data object.
      #
      # ==== Arguments
      #
      # * data - the Qpid::Proton::Data instance
      #
      # ==== Examples
      #
      #   described = Qpid::Proton::Described.new("my-descriptor", "the value")
      #   data = Qpid::Proton::Data.new
      #   ...
      #   described.put(data)
      #
      def put(data)
        data.symbol = @descriptor
        data.string = @value
      end

    end

  end

end
