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

module Qpid::Proton

  class Condition

    attr_reader :name, :description, :info

    def initialize(name, description = nil, info = nil)
      @name = name
      @description = description
      @info = info
    end

    def to_s() "#{@name}: #{@description
    def to_s() "#{@name}: #{@description}"; end
}"; end

    def inspect() "#{self.class.name}(#{@name.inspect}, #{@description.inspect}, #{@info.inspect})"; end

    def ==(other)
      ((other.is_a? Condition) &&
       (other.name == self.name) && 
       (other.description == self.description) &&
       (other.info == self.info))
    end

    # Make a condition.
    # @param obj the object to turn into a condition
    # @param default_name condition name to use if obj does not imply a name
    # @return
    # - when Condition return obj unchanged
    # - when Exception return Condition(obj.class.name, obj.to_s)
    # - when nil then nil
    # - else return Condition(default_name, obj.to_s)
    # If objey
    def self.make(obj, default_name="proton")
      case obj
      when Condition then obj
      when Exception then Condition.new(obj.class.name, obj.to_s)
      when nil then nil
      else Condition.new(default_name, obj.to_s)
      end
    end

  end

  module Util                   #TODO aconway 2017-10-28: backwards compat
    Condition = Qpid::Proton::Condition
  end
end
