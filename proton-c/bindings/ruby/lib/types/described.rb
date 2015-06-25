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

module Qpid::Proton::Types

  # @private
  class Described

    attr_reader :descriptor
    attr_reader :value

    def initialize(descriptor, value)
      @descriptor = descriptor
      @value = value
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

    def ==(that) # :nodoc:
      (that.is_a?(Qpid::Proton::Types::Described) &&
       (self.descriptor == that.descriptor) &&
       (self.value == that.value))
    end

    def to_s # :nodoc:
      "descriptor=#{descriptor} value=#{value}"
    end

  end

end
