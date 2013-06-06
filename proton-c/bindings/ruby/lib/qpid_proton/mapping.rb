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

    # Maps between Proton types and their Ruby native language counterparts.
    #
    class Mapping

      attr_reader :code
      attr_reader :put_method
      attr_reader :get_method

      # Creates a new mapping.
      #
      # ==== Arguments
      #
      # * code    - the AMQP code for this type
      # * name    - the AMQP name for this type
      # * klasses - the Ruby classes for this type
      # * getter  - overrides the get method for the type
      def initialize(code, name, klasses = nil, getter = nil)
        @code = code
        @name = name

        @@by_preferred ||= {}
        @@by_code ||= {}
        @@by_code["#{code}"] = self
        @@by_name ||= {}
        @@by_name[name] = self
        @@by_class ||= {}

        unless klasses.nil?
          klasses.each do |klass|
            raise "entry exists for #{klass}" if @@by_class.keys.include? klass
            @@by_class[klass] = self unless klass.nil?
          end
        end

        @put_method = (name + "=").intern

        if getter.nil?
          @get_method = name.intern
        else
          @get_method = getter.intern
        end
      end

      def to_s; @name; end

      def put(data, value)
        data.send(@put_method, value)
      end

      def get(data)
        data.send(@get_method)
      end

      def self.for_class(klass) # :nodoc:
        @@by_class[klass]
      end

      def self.for_code(code)
        @@by_code["#{code}"]
      end

    end

    NULL       = Mapping.new(Cproton::PN_NULL, "null", [NilClass], "nil?")
    BOOL       = Mapping.new(Cproton::PN_BOOL, "bool", [TrueClass, FalseClass])
    UBYTE      = Mapping.new(Cproton::PN_UBYTE, "ubyte")
    BYTE       = Mapping.new(Cproton::PN_BYTE, "byte")
    USHORT     = Mapping.new(Cproton::PN_USHORT, "ushort")
    SHORT      = Mapping.new(Cproton::PN_SHORT, "short")
    UINT       = Mapping.new(Cproton::PN_UINT, "uint")
    INT        = Mapping.new(Cproton::PN_INT, "int")
    CHAR       = Mapping.new(Cproton::PN_CHAR, "char")
    ULONG      = Mapping.new(Cproton::PN_ULONG, "ulong")
    LONG       = Mapping.new(Cproton::PN_LONG, "long", [Fixnum, Bignum])
    TIMESTAMP  = Mapping.new(Cproton::PN_TIMESTAMP, "timestamp", [Date])
    FLOAT      = Mapping.new(Cproton::PN_FLOAT, "float")
    DOUBLE     = Mapping.new(Cproton::PN_DOUBLE, "double", [Float])
    DECIMAL32  = Mapping.new(Cproton::PN_DECIMAL32, "decimal32")
    DECIMAL64  = Mapping.new(Cproton::PN_DECIMAL64, "decimal64")
    DECIMAL128 = Mapping.new(Cproton::PN_DECIMAL128, "decimal128")
    UUID       = Mapping.new(Cproton::PN_UUID, "uuid")
    BINARY     = Mapping.new(Cproton::PN_BINARY, "binary")
    STRING     = Mapping.new(Cproton::PN_STRING, "string", [String])
    SYMBOL     = Mapping.new(Cproton::PN_SYMBOL, "symbol")
    DESCRIBED  = Mapping.new(Cproton::PN_DESCRIBED, "described", [Qpid::Proton::Described])
    ARRAY      = Mapping.new(Cproton::PN_ARRAY, "array")
    LIST       = Mapping.new(Cproton::PN_LIST, "list", [::Array])
    MAP        = Mapping.new(Cproton::PN_MAP, "map", [::Hash])

  end

end
