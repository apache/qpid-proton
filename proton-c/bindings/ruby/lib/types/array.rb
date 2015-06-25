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
# Patch the Array class to provide methods for adding its contents
# to a Qpid::Proton::Data instance.
#++

module Qpid::Proton::Types

  # Holds the information for an AMQP Array compound type.
  #
  # It holds the type for the array and the descriptor if the
  # array is described.
  #
  # @private
  #
  class ArrayHeader
    attr_reader :type
    attr_reader :descriptor

    def initialize(type, descriptor = nil)
      @type = type
      @descriptor = descriptor
    end

    # Returns true if the array is described.
    def described?
      !@descriptor.nil?
    end

    def ==(that)
      ((@type == that.type) && (@descriptor == that.descriptor))
    end
  end

end

# @private
class Array # :nodoc:

  # Used to declare an array as an AMQP array.
  #
  # The value, if defined, is an instance of Qpid::Proton::Types::ArrayHeader
  attr_accessor :proton_array_header

  # Returns true if the array is the a Proton described type.
  def proton_described?
    !@proton_array_header.nil? && @proton_array_header.described?
  end

  # Puts the elements of the array into the specified Qpid::Proton::Data object.
  def proton_put(data)
    raise TypeError, "data object cannot be nil" if data.nil?

    if @proton_array_header.nil?
      proton_put_list(data)
    else
      proton_put_array(data)
    end
  end

  private

  def proton_put_list(data)
    # create a list, then enter it and add each element
    data.put_list
    data.enter
    each do |element|
      # get the proton type for the element
      mapping = Qpid::Proton::Codec::Mapping.for_class(element.class)
      # add the element
      mapping.put(data, element)
    end
    # exit the list
    data.exit
  end

  def proton_put_array(data)
    data.put_array(@proton_array_header.described?, @proton_array_header.type)
    data.enter
    if @proton_array_header.described?
      data.symbol = @proton_array_header.descriptor
    end

    each do |element|
      @proton_array_header.type.put(data, element)
    end

    data.exit
  end

  class << self

    # Gets the elements of an array or list out of the specified
    # Qpid::Proton::Data object.
    def proton_get(data)
      raise TypeError, "can't convert nil into Qpid::Proton::Data" if data.nil?

      type = data.type

      if type == Qpid::Proton::Codec::LIST
        result = proton_get_list(data)
      elsif type == Qpid::Proton::Codec::ARRAY
        result = proton_get_array(data)
      else
        raise TypeError, "element is not a list and not an array"
      end
    end

    private

    def proton_get_list(data)
      size = data.list
      raise TypeError, "not a list" unless data.enter
      elements = []
      (0...size).each do
        data.next
        type = data.type
        raise TypeError, "missing next element in list" unless type
        elements << type.get(data)
      end
      data.exit
      return elements
    end

    def proton_get_array(data)
      count, described, type = data.array

      raise TypeError, "not an array" unless data.enter
      elements = []

      descriptor = nil

      if described
        data.next
        descriptor = data.symbol
      end

      elements.proton_array_header = Qpid::Proton::Types::ArrayHeader.new(type, descriptor)
      (0...count).each do |which|
        if data.next
          etype = data.type
          raise TypeError, "missing next element in array" unless etype
          raise TypeError, "invalid array element: #{etype}" unless etype == type
          elements << type.get(data)
        end
      end
      data.exit
      return elements
    end

  end

end

