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


#--
# Patch the Array class to provide methods for adding its contents
# to a Qpid::Proton::Codec::Data instance.
#++

module Qpid::Proton
  module Types

    # @deprecated use {UniformArray}
    class ArrayHeader
      def initialize(type, descriptor = nil)
        Util::Deprecation.deprecated ArrayHeader, "UniformArray"
        @type, @descriptor = Codec::Mapping[type], descriptor
      end
      attr_reader :type, :descriptor
      def described?() !@descriptor.nil?; end
      def <=>(x) [@type, @descriptor] <=> [x.type, x.descriptor]; end
      include Comparable
    end

    # *Unsettled API* - An array that is converted to/from an AMQP array of uniform element type.
    # A plain ruby +::Array+ is converted to/from an AMQP list, which can contain mixed type elements.
    # If invalid elements are included, then {TypeError} will be raised when encoding to AMQP.
    class UniformArray < ::Array

      # Construct a uniform array, which will be converted to an AMQP array.
      # A plain ruby +::Array+ is converted to/from an AMQP list, containing mixed type elements.
      #
      # @param type [Type] Elements must be convertible to this AMQP type.
      # @param elements [Enumerator] Initial elements for the array
      # @param descriptor [Object] Optional array descriptor
      def initialize(type, elements=nil, descriptor=nil)
        @type, @descriptor = type, descriptor
        @proton_array_header = nil
        raise ArgumentError, "no type specified for array" if @type.nil?
        super elements if elements
      end

      # @deprecated backwards compatibility  {UniformArray}
      def proton_array_header
        @proton_array_header ||= ArrayHeader.new(@type, @descriptor) # Deprecated
      end

      # @return [Type] Array elements must be convertible to this AMQP type
      attr_reader :type

      # @return [Object] Optional descriptor.
      attr_reader :descriptor

      def inspect() "#{self.class.name}<#{type}>#{super}"; end

      def <=>(x)
        ret = [@type, @descriptor] <=> [x.type, x.descriptor]
        ret == 0 ? super : ret
      end
    end
  end
end

# {Array} is converted to/from an AMQP list, which is allowed to hold mixed-type elements.
# Use  {UniformArray} to convert/from an AMQP array with uniform element type.
class ::Array
  # @deprecated use  {UniformArray}
  def proton_array_header
    Qpid::Proton::Util::Deprecation.deprecated __method__, UniformArray
    @proton_array_header
  end

  # @deprecated use  {UniformArray}
  def proton_array_header=(h)
    Qpid::Proton::Util::Deprecation.deprecated __method__, UniformArray
    @proton_array_header= h
  end

  # @deprecated use  {UniformArray}
  def proton_described?()
    Qpid::Proton::Util::Deprecation.deprecated __method__, UniformArray
    @proton_array_header && @proton_array_header.described?
  end

  # @deprecated
  def proton_put(data)
    Qpid::Proton::Util::Deprecation.deprecated __method__, "Codec::Data#array=, Codec::Data#list="
    raise TypeError, "nil data" unless data
    if @proton_array_header && @proton_array_header.type
      data.array = self
    else
      data.list = self
    end
  end

  # @deprecated
  def self.proton_get(data)
    Qpid::Proton::Util::Deprecation.deprecated __method__, "Codec::Data#list"
    data.list
  end
end

