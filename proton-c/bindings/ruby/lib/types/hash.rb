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

#--
# Patch the Hash class to provide methods for adding its contents
# to a Qpid::Proton::Data instance.
#++

# @private
class Hash # :nodoc:

  # Places the contents of the hash into the specified data object.
  #
  # ==== Arguments
  #
  # * data - the Qpid::Proton::Data instance
  #
  # ==== Examples
  #
  #   data = Qpid::Proton::Data.new
  #   values = {:foo => :bar}
  #   values.proton_data_put(data)
  #
  def proton_data_put(data)
    raise TypeError, "data object cannot be nil" if data.nil?

    data.put_map
    data.enter

    each_pair do |key, value|
      type = Qpid::Proton::Codec::Mapping.for_class(key.class)
      type.put(data, key)
      type = Qpid::Proton::Codec::Mapping.for_class(value.class)
      type.put(data, value)
    end

    data.exit
  end

  class << self

    def proton_data_get(data)
      raise TypeError, "data object cannot be nil" if data.nil?

      type = data.type

      raise TypeError, "element is not a map" unless type == Qpid::Proton::Codec::MAP

      count = data.map
      result = {}

      data.enter

      (0...(count/2)).each do
        data.next
        type = data.type
        key = type.get(data)
        data.next
        type = data.type
        value = type.get(data)
        result[key] = value
      end

      data.exit

      return result
    end

  end

end
