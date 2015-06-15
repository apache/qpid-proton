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

  class Condition

    def initialize(name, description = nil, info = nil)
      @name = name
      @description = description
      @info = info
    end

    # @private
    def to_s
      "Condition(#{@name}, #{@description}, #{@info})"
    end

    # @private
    def ==(other)
      ((other.class = self.class) &&
       (other.name == self.name) && 
       (other.description == self.description) &&
       (other.info == self.info))
    end

  end

end
