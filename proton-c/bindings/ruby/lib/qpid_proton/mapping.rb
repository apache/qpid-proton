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

        @debug = (name == "bool")

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
    BOOL       = Mapping.new(Cproton::PN_BOOL, "bool", [TrueClass, FalseClass], "bool")
    UBYTE      = Mapping.new(Cproton::PN_UBYTE, "ubyte")
    BYTE       = Mapping.new(Cproton::PN_BYTE, "byte")
    USHORT     = Mapping.new(Cproton::PN_USHORT, "ushort")
    SHORT      = Mapping.new(Cproton::PN_SHORT, "short")
    UINT       = Mapping.new(Cproton::PN_UINT, "uint")
    INT        = Mapping.new(Cproton::PN_INT, "int")
    CHAR       = Mapping.new(Cproton::PN_CHAR, "char")
    ULONG      = Mapping.new(Cproton::PN_ULONG, "ulong")
    LONG       = Mapping.new(Cproton::PN_LONG, "long", [Fixnum, Bignum])
    TIMESTAMP  = Mapping.new(Cproton::PN_TIMESTAMP, "timestamp", [Date, Time])
    FLOAT      = Mapping.new(Cproton::PN_FLOAT, "float")
    DOUBLE     = Mapping.new(Cproton::PN_DOUBLE, "double", [Float])
    DECIMAL32  = Mapping.new(Cproton::PN_DECIMAL32, "decimal32")
    DECIMAL64  = Mapping.new(Cproton::PN_DECIMAL64, "decimal64")
    DECIMAL128 = Mapping.new(Cproton::PN_DECIMAL128, "decimal128")
    UUID       = Mapping.new(Cproton::PN_UUID, "uuid")
    BINARY     = Mapping.new(Cproton::PN_BINARY, "binary")
    STRING     = Mapping.new(Cproton::PN_STRING, "string", [String, Symbol])

    class << STRING
      def put(data, value)
        # In Ruby 1.9+ we have encoding methods that can check the content of
        # the string, so use them to see if what we have is unicode. If so,
        # good! If not, then just treat is as binary.
        #
        # No such thing in Ruby 1.8. So there we need to use Iconv to try and
        # convert it to unicode. If it works, good! But if it raises an
        # exception then we'll treat it as binary.
        if RUBY_VERSION >= "1.9"
          if value.encoding == "UTF-8" || value.force_encoding("UTF-8").valid_encoding?
            data.string = value.to_s
          else
            data.binary = value.to_s
          end
        else
          begin
            newval = Iconv.new("UTF8//TRANSLIT//IGNORE", "UTF8").iconv(value.to_s)
            data.string = newval
          rescue
            data.binary = value
          end
        end
      end
    end

    SYMBOL     = Mapping.new(Cproton::PN_SYMBOL, "symbol")
    DESCRIBED  = Mapping.new(Cproton::PN_DESCRIBED, "described", [Qpid::Proton::Described], "get_described")
    ARRAY      = Mapping.new(Cproton::PN_ARRAY, "array", nil, "get_array")
    LIST       = Mapping.new(Cproton::PN_LIST, "list", [::Array], "get_array")
    MAP        = Mapping.new(Cproton::PN_MAP, "map", [::Hash], "get_map")

    class << MAP
      def put(data, map)
        data.put_map
        data.enter
        map.each_pair do |key, value|
          Mapping.for_class(key.class).put(data, key)

          if value.nil?
            data.null
          else
            Mapping.for_class(value.class).put(data, value)
          end
        end
        data.exit
      end
    end

  end

end
