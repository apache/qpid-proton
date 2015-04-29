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
  def self.is_valid_utf?(value)
    # In Ruby 1.9+ we have encoding methods that can check the content of
    # the string, so use them to see if what we have is unicode. If so,
    # good! If not, then just treat is as binary.
    #
    # No such thing in Ruby 1.8. So there we need to use Iconv to try and
    # convert it to unicode. If it works, good! But if it raises an
    # exception then we'll treat it as binary.
    if RUBY_VERSION < "1.9"
      return true if value.isutf8
      return false
    else
      return true if (value.encoding == "UTF-8" ||
                      value.encode("UTF-8").valid_encoding?)

      return false
    end
  end

  # UTFString lets an application explicitly state that a
  # string of characters is to be UTF-8 encoded.
  #
  class UTFString < ::String

    def initialize(value)
      if !Qpid::Proton::Types.is_valid_utf?(value)
        raise RuntimeError.new("invalid UTF string")
      end

      super(value)
    end

  end

  # BinaryString lets an application explicitly declare that
  # a string value represents arbitrary data.
  #
  class BinaryString < ::String; end

end
