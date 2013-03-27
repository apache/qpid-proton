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

require 'cproton'

module Qpid

  module Proton

    # +DataError+ is raised when an error occurs while encoding
    # or decoding data.
    class DataError < Exception; end

    # The +Data+ class provides an interface for decoding, extracting,
    # creating, and encoding arbitrary AMQP data. A +Data+ object
    # contains a tree of AMQP values. Leaf nodes in this tree correspond
    # to scalars in the AMQP type system such as INT or STRING. Interior
    # nodes in this tree correspond to compound values in the AMQP type
    # system such as *LIST*,*MAP*, *ARRAY*, or *DESCRIBED*. The root node
    # of the tree is the +Data+ object itself and can have an arbitrary
    # number of children.
    #
    # A +Data+ object maintains the notion of the current sibling node
    # and a current parent node. Siblings are ordered within their parent.
    # Values are accessed and/or added by using the #next, #prev,
    # #enter, and #exit methods to navigate to the desired location in
    # the tree and using the supplied variety of mutator and accessor
    # methods to access or add a value of the desired type.
    #
    # The mutator methods will always add a value _after_ the current node
    # in the tree. If the current node has a next sibling the mutator method
    # will overwrite the value on this node. If there is no current node
    # or the current node has no next sibling then one will be added. The
    # accessor methods always set the added/modified node to the current
    # node. The accessor methods read the value of the current node and do
    # not change which node is current.
    #
    # The following types of scalar values are supported:
    #
    # * *NULL*
    # * *BOOL*
    # * *UBYTE*
    # * *BYTE*
    # * *USHORT*
    # * *SHORT*
    # * *UINT*
    # * *INT*
    # * *CHAR*
    # * *ULONG*
    # * *LONG*
    # * *TIMESTAMP*
    # * *FLOAT*
    # * *DOUBLE*
    # * *DECIMAL32*
    # * *DECIMAL64*
    # * *DECIMAL128*
    # * *UUID*
    # * *BINARY*
    # * *STRING*
    # * *SYMBOL*
    #
    # The following types of compound values are supported:
    #
    # * *DESCRIBED*
    # * *ARRAY*
    # * *LIST*
    # * *MAP*
    #
    class Data

      # Creates a new instance with the specified capacity.
      #
      # ==== Options
      #
      # * capacity - the capacity
      #
      def initialize(capacity = 16)
        if (!capacity.nil?) &&
            (capacity.is_a?(Fixnum) ||
             capacity.is_a?(Bignum))
          @data = Cproton.pn_data(capacity)
          @free = true
        else
          @data = capacity
          @free = false
        end

        # destructor
        ObjectSpace.define_finalizer(self, self.class.finalize!(@data))
      end

      def self.finalize!(data) # :nodoc:
        proc {
          Cproton.pn_data_free(data) if @free
        }
      end

      # Clears the object.
      def clear
        Cproton.pn_data_clear(@data)
      end

      # Clears the current node and sets the parent to the root node.
      #
      # Clearing the current node sets it _before_ the first node, calling
      # #next will advance to the first node.
      def rewind
        Cproton.pn_data_rewind(@data)
      end

      # Advances the current node to its next sibling and returns its types.
      #
      # If there is no next sibling the current node remains unchanged
      # and nil is returned.
      def next
        return Cproton.pn_data_next(@data) ? type : nil
      end

      # Advances the current node to its previous sibling and returns its type.
      #
      # If there is no previous sibling then the current node remains unchanged
      # and nil is return.
      def prev
        return Cproton.pn_data_prev(@data) ? type : nil
      end

      # Sets the parent node to the current node and clears the current node.
      #
      # Clearing the current node sets it _before_ the first child.
      def enter
        Cproton.pn_data_enter(@data)
      end

      # Sets the current node to the parent node and the parent node to its own
      # parent.
      def exit
        Cproton.pn_data_exit(@data)
      end

      # Returns the numeric type code of the current node.
      def type_code
        dtype = Cproton.pn_data_type(@data)
        return (dtype == -1) ? nil : dtype
      end

      # Return the Type object for the current node
      def type
        Type.by_code(type_code)
      end

      # Returns a representation of the data encoded in AMQP format.
      def encode
        buffer = "\0"*1024
        loop do
          cd = Cproton.pn_data_encode(@data, buffer, buffer.length)
          if cd == Cproton::PN_OVERFLOW
            buffer *= 2
          elsif cd >= 0
            return buffer[0...cd]
          else
            check(cd)
          end
        end
      end

      # Decodes the first value from supplied AMQP data and returns the number
      # of bytes consumed.
      #
      # ==== Options
      #
      # * encoded - the encoded data
      #
      def decode(encoded)
        check(Cproton.pn_data_decode(@data, encoded, encoded.length))
      end

      # Puts a list value.
      #
      # Elements may be filled by entering the list node and putting element
      # values.
      #
      # ==== Examples
      #
      #   data = Qpid::Proton::Data.new
      #   data.put_list
      #   data.enter
      #   data.int = 1
      #   data.int = 2
      #   data.int = 3
      #   data.exit
      #
      def put_list
        check(Cproton.pn_data_put_list(@data))
      end

      # If the current node is a list, this returns the number of elements.
      # Otherwise, it returns zero.
      #
      # List elements can be accessed by entering the list.
      #
      # ==== Examples
      #
      #   count = @data.list
      #   @data.enter
      #   (0...count).each
      #     type = @data.next
      #     puts "Value: #{@data.string}" if type == STRING
      #     # ... process other node types
      #   end
      def list
        Cproton.pn_data_get_list(@data)
      end

      # Puts a map value.
      #
      # Elements may be filled by entering the map node and putting alternating
      # key/value pairs.
      #
      # ==== Examples
      #
      #   data = Qpid::Proton::Data.new
      #   data.put_map
      #   data.enter
      #   data.string = "key"
      #   data.string = "value"
      #   data.exit
      #
      def put_map
        check(Cproton.pn_data_put_map(@data))
      end

      # If the  current node is a map, this returns the number of child
      # elements. Otherwise, it returns zero.
      #
      # Key/value pairs can be accessed by entering the map.
      #
      # ==== Examples
      #
      #   count = @data.map
      #   @data.enter
      #   (0...count).each do
      #     type = @data.next
      #     puts "Key=#{@data.string}" if type == STRING
      #     # ... process other key types
      #     type = @data.next
      #     puts "Value=#{@data.string}" if type == STRING
      #     # ... process other value types
      #   end
      #   @data.exit
      def map
        Cproton.pn_data_get_map(@data)
      end

      # Puts an array value.
      #
      # Elements may be filled by entering the array node and putting the
      # element values. The values must all be of the specified array element
      # type.
      #
      # If an array is *described* then the first child value of the array
      # is the descriptor and may be of any type.
      #
      # ==== Options
      #
      # * described - specifies whether the array is described
      # * element_type - the type of the array elements
      #
      # ==== Examples
      #
      #   # create an array of integer values
      #   data = Qpid::Proton::Data.new
      #   data.put_array(false, INT)
      #   data.enter
      #   data.int = 1
      #   data.int = 2
      #   data.int = 3
      #   data.exit
      #
      #   # create an array of double values
      #   data.put_array(true, DOUBLE)
      #   data.enter
      #   data.symbol = "array-descriptor"
      #   data.double = 1.1
      #   data.double = 1.2
      #   data.double = 1.3
      #   data.exit
      #
      def put_array(described, element_type)
        check(Cproton.pn_data_put_array(@data, described, element_type.code))
      end

      # If the current node is an array, returns a tuple of the element count, a
      # boolean indicating whether the array is described, and the type of each
      # element. Otherwise it returns +(0, false, nil).
      #
      # Array data can be accessed by entering the array.
      #
      # ==== Examples
      #
      #   # get the details of thecurrent array
      #   count, described, array_type = @data.array
      #
      #   # enter the node
      #   data.enter
      #
      #   # get the next node
      #   data.next
      #   puts "Descriptor: #{data.symbol}" if described
      #   (0...count).each do
      #     @data.next
      #     puts "Element: #{@data.string}"
      #   end
      def array
        count = Cproton.pn_data_get_array(@data)
        described = Cproton.pn_data_is_array_described(@data)
        array_type = Cproton.pn_data_get_array_type(@data)
        return nil if array_type == -1
        [count, described, Type.by_code(array_type) ]
      end

      # Puts a described value.
      #
      # A described node has two children, the descriptor and the value.
      # These are specified by entering the node and putting the
      # desired values.
      #
      # ==== Examples
      #
      #   data = Qpid::Proton::Data.new
      #   data.put_described
      #   data.enter
      #   data.symbol = "value-descriptor"
      #   data.string = "the value"
      #   data.exit
      #
      def put_described
        check(Cproton.pn_data_put_described(@data))
      end

      # Checks if the current node is a described value.
      #
      # The described and value may be accessed by entering the described value.
      #
      # ==== Examples
      #
      #   if @data.described?
      #     @data.enter
      #     puts "The symbol is #{@data.symbol}"
      #     puts "The value is #{@data.string}"
      #   end
      def described?
        Cproton.pn_data_is_described(@data)
      end

      # Puts a null value.
      def null
        check(Cproton.pn_data_put_null(@data))
      end

      # Checks if the current node is null.
      def null?
        Cproton.pn_data_is_null(@data)
      end

      # Puts a boolean value.
      #
      # ==== Options
      #
      # * value - the boolean value
      def bool=(value)
        check(Cproton.pn_data_put_bool(@data, value))
      end

      # If the current node is a boolean, then it returns the value. Otherwise,
      # it returns false.
      def bool
        Cproton.pn_data_get_bool(@data)
      end

      # Puts an unsigned byte value.
      #
      # ==== Options
      #
      # * value - the unsigned byte value
      def ubyte=(value)
        check(Cproton.pn_data_put_ubyte(@data, value))
      end

      # If the current node is an unsigned byte, returns its value. Otherwise,
      # it reutrns 0.
      def ubyte
        Cproton.pn_data_get_ubyte(@data)
      end

      # Puts a byte value.
      #
      # ==== Options
      #
      # * value - the byte value
      def byte=(value)
        check(Cproton.pn_data_put_byte(@data, value))
      end

      # If the current node is an byte, returns its value. Otherwise,
      # it returns 0.
      def byte
        Cproton.pn_data_get_byte(@data)
      end

      # Puts an unsigned short value.
      #
      # ==== Options
      #
      # * value - the unsigned short value
      def ushort=(value)
        check(Cproton.pn_data_put_ushort(@data, value))
      end

      # If the current node is an unsigned short, returns its value. Otherwise,
      # it returns 0.
      def ushort
        Cproton.pn_data_get_ushort(@data)
      end

      # Puts a short value.
      #
      # ==== Options
      #
      # * value - the short value
      def short=(value)
        check(Cproton.pn_data_put_short(@data, value))
      end

      # If the current node is a short, returns its value. Otherwise,
      # returns a 0.
      def short
        Cproton.pn_data_get_short(@data)
      end

      # Puts an unsigned integer value.
      #
      # ==== Options
      #
      # * value - the unsigned integer value
      def uint=(value)
        raise TypeError if value.nil?
        raise RangeError, "invalid uint: #{value}" if value < 0
        check(Cproton.pn_data_put_uint(@data, value))
      end

      # If the current node is an unsigned int, returns its value. Otherwise,
      # returns 0.
      def uint
        Cproton.pn_data_get_uint(@data)
      end

      # Puts an integer value.
      #
      # ==== Options
      #
      # * value - the integer value
      def int=(value)
        check(Cproton.pn_data_put_int(@data, value))
      end

      # If the current node is an integer, returns its value. Otherwise,
      # returns 0.
      def int
        Cproton.pn_data_get_int(@data)
      end

      # Puts a character value.
      #
      # ==== Options
      #
      # * value - the character value
      def char=(value)
        check(Cproton.pn_data_put_char(@data, value))
      end

      # If the current node is a character, returns its value. Otherwise,
      # returns 0.
      def char
        Cproton.pn_data_get_char(@data)
      end

      # Puts an unsigned long value.
      #
      # ==== Options
      #
      # * value - the unsigned long value
      def ulong=(value)
        raise TypeError if value.nil?
        raise RangeError, "invalid ulong: #{value}" if value < 0
        check(Cproton.pn_data_put_ulong(@data, value))
      end

      # If the current node is an unsigned long, returns its value. Otherwise,
      # returns 0.
      def ulong
        Cproton.pn_data_get_ulong(@data)
      end

      # Puts a long value.
      #
      # ==== Options
      #
      # * value - the long value
      def long=(value)
        check(Cproton.pn_data_put_long(@data, value))
      end

      # If the current node is a long, returns its value. Otherwise, returns 0.
      def long
        Cproton.pn_data_get_long(@data)
      end

      # Puts a timestamp value.
      #
      # ==== Options
      #
      # * value - the timestamp value
      def timestamp=(value)
        value = value.to_i if (!value.nil? && value.is_a?(Time))
        check(Cproton.pn_data_put_timestamp(@data, value))
      end

      # If the current node is a timestamp, returns its value. Otherwise,
      # returns 0.
      def timestamp
        Cproton.pn_data_get_timestamp(@data)
      end

      # Puts a float value.
      #
      # ==== Options
      #
      # * value - the float value
      def float=(value)
        check(Cproton.pn_data_put_float(@data, value))
      end

      # If the current node is a float, returns its value. Otherwise,
      # returns 0.
      def float
        Cproton.pn_data_get_float(@data)
      end

      # Puts a double value.
      #
      # ==== Options
      #
      # * value - the double value
      def double=(value)
        check(Cproton.pn_data_put_double(@data, value))
      end

      # If the current node is a double, returns its value. Otherwise,
      # returns 0.
      def double
        Cproton.pn_data_get_double(@data)
      end

      # Puts a decimal32 value.
      #
      # ==== Options
      #
      # * value - the decimal32 value
      def decimal32=(value)
        check(Cproton.pn_data_put_decimal32(@data, value))
      end

      # If the current node is a decimal32, returns its value. Otherwise,
      # returns 0.
      def decimal32
        Cproton.pn_data_get_decimal32(@data)
      end

      # Puts a decimal64 value.
      #
      # ==== Options
      #
      # * value - the decimal64 value
      def decimal64=(value)
        check(Cproton.pn_data_put_decimal64(@data, value))
      end

      # If the current node is a decimal64, returns its value. Otherwise,
      # it returns 0.
      def decimal64
        Cproton.pn_data_get_decimal64(@data)
      end

      # Puts a decimal128 value.
      #
      # ==== Options
      #
      # * value - the decimal128 value
      def decimal128=(value)
        raise TypeError, "invalid decimal128 value: #{value}" if value.nil?
        value = value.to_s(16).rjust(32, "0")
        bytes = []
        value.scan(/(..)/) {|v| bytes << v[0].to_i(16)}
        check(Cproton.pn_data_put_decimal128(@data, bytes))
      end

      # If the current node is a decimal128, returns its value. Otherwise,
      # returns 0.
      def decimal128
        value = ""
        Cproton.pn_data_get_decimal128(@data).each{|val| value += ("%02x" % val)}
        value.to_i(16)
      end

      # Puts a +UUID+ value.
      #
      # The UUID is expected to be in the format of a string or else a 128-bit
      # integer value.
      #
      # ==== Options
      #
      # * value - the +UUID+
      #
      # ==== Examples
      #
      #   # set a uuid value from a string value
      #   require 'securerandom'
      #   @data.uuid = SecureRandom.uuid
      #
      #   # or
      #   @data.uuid = "fd0289a5-8eec-4a08-9283-81d02c9d2fff"
      #
      #   # set a uuid value from a 128-bit value
      #   @data.uuid = 0 # sets to 00000000-0000-0000-0000-000000000000
      #
      def uuid=(value)
        raise ArgumentError, "invalid uuid: #{value}" if value.nil?

        # if the uuid that was submitted was numeric value, then translated
        # it into a hex string, otherwise assume it was a string represtation
        # and attempt to decode it
        if value.is_a? Numeric
          value = "%032x" % value
        else
          raise ArgumentError, "invalid uuid: #{value}" if !valid_uuid?(value)

          value = (value[0, 8]  +
                   value[9, 4]  +
                   value[14, 4] +
                   value[19, 4] +
                   value[24, 12])
        end
        bytes = []
        value.scan(/(..)/) {|v| bytes << v[0].to_i(16)}
        check(Cproton.pn_data_put_uuid(@data, bytes))
      end

      # If the current value is a +UUID+, returns its value. Otherwise,
      # it returns nil.
      def uuid
        value = ""
        Cproton.pn_data_get_uuid(@data).each{|val| value += ("%02x" % val)}
        value.insert(8, "-").insert(13, "-").insert(18, "-").insert(23, "-")
      end

      # Puts a binary value.
      #
      # ==== Options
      #
      # * value - the binary value
      def binary=(value)
        check(Cproton.pn_data_put_binary(@data, value))
      end

      # If the current node is binary, returns its value. Otherwise, it returns
      # an empty string ("").
      def binary
        Cproton.pn_data_get_binary(@data)
      end

      # Puts a unicode string value.
      #
      # *NOTE:* A nil value is stored as an empty string rather than as a nil.
      #
      # ==== Options
      #
      # * value - the unicode string value
      def string=(value)
        check(Cproton.pn_data_put_string(@data, value))
      end

      # If the current node is a string, returns its value. Otherwise, it
      # returns an empty string ("").
      def string
        Cproton.pn_data_get_string(@data)
      end

      # Puts a symbolic value.
      #
      # ==== Options
      #
      # * value - the symbol name
      def symbol=(value)
        check(Cproton.pn_data_put_symbol(@data, value))
      end

      # If the current node is a symbol, returns its value. Otherwise, it
      # returns an empty string ("").
      def symbol
        Cproton.pn_data_get_symbol(@data)
      end

      # Convenience class for described types.
      #
      # Holds the descriptor and value, implements methods to get and put a
      # described type in a Data object.
      Described = Struct.new(:descriptor, :value)
      class Described
        def self.get(data)
          def fail; raise 'Not a described type'; end
          (data.described? and data.enter) or fail
          begin
            data.next or fail
            descriptor = data.get
            data.next or fail
            value = data.get
            return Described.new(descriptor, value)
          ensure
            data.exit
          end
        end

        def put(data)
          described
          enter
          data.put(@descriptor)
          data.put(@value)
        end
      end

      # Convenience class for arrays
      #
      # Convenience class for arrays.
      #
      # An Array with methods to get and put the array in a Data object
      # as an AMQP array.
      class Array < ::Array
        def initialize(descriptor, type, elements=[])
          @descriptor, @type = descriptor, type
          super(elements.collect { |x| x })
        end

        attr_reader :descriptor, :type

        def ==(o) super; end
        def eql?(o)
          o.class == self.class && @descriptor == o.descriptor && @type == o.type &&
            super
        end


        def self.get(data)
          count, described, type = data.array
          data.enter or raise 'Not an array'
          begin
            descriptor = nil
            if described then
              data.next; descriptor = data.get
            end
            elements = []
            while data.next do; elements << data.get; end
            elements.size == count or
              raise "Array wrong length, expected #{count} but got #{elements.size}"
            return Array.new(descriptor, type, elements)
          ensure
            data.exit
          end
        end

        def put(data)
          data.put_array(@descriptor, @type)
          data.enter
          begin
            data.put(@descriptor) if @descriptor
            elements.each { |e| data.put(e); }
          ensure
            data.exit
          end
        end
      end

      # Convenience class for arrays.
      #
      # An array with methods to get and put the in a Data object
      # as an AMQP list.
      class List < ::Array

        def initialize(elements) super; end

        def self.get(data)
          def fail; raise 'Not a list'; end
          size = data.list
          data.enter or fail
          begin
            elements = []
            while data.next do elements << data.get; end
            elements.size == size or
              raise "List wrong length, expected #{size} but got #{elements.size}"
            return List.new(elements)
          ensure
            data.exit
          end
        end

        def put(data)
          data.put_list
          @elements.each { |e| data.put(e); }
        end
      end

      # Convenience class for maps
      #
      # A Hash with methods to get and put the map in a Data object as an AMQP
      # map.
      class Map < ::Hash
        def initialize(map) super; end

        def self.get(data)
          data.enter or raise "Not a Map"
          begin
            map = {}
            while data.next
              k = data.get
              data.next or raise "Map missing final value"
              map[k] = data.get
            end
            return map
          ensure
            data.exit
          end
        end

        def put(data)
          data.enter
          begin
            @map.each { |k,v| data.put(k); data.put(v); }
          ensure
            data.exit
          end
        end
      end

      # Convenience class for null values
      #
      # Empty object with methods to get and put it in a Data object.
      class Null
        def initialize; end

        def self.get(data)
          data.null? or raise "Not a null"
          return nil
        end

        def put(data); data.null; end
      end

      # Information about AMQP types including how to get/put an object of that
      # type in a Data object.
      #
      # A convenience class is provided for each of the compound types
      # to allow get/put of those types as a single object.
      class Type
        attr_reader :code, :name

        def initialize(code,name,klass=nil)
          @code, @name, @klass = code,name,klass;
          @get,@put = name.intern,(name+"=").intern if !klass
          @@by_code ||= {}
          @@by_code[code] = self
          @@by_name ||= {}
          @@by_name[name] = self
        end

        def get(data)
          if @klass then @klass.send(:get, data)
          else data.send(@get); end
        end

        def put(data, value)
          if @klass then @klass.send(:put, data, value)
          else data.send(@put, value); end
        end

        def to_s() return name; end
        def self.by_name(name) @@by_name[name]; end
        def self.by_code(code) @@by_code[code]; end


        def self.get(data)
          if @klass then @klass.send(:get, data)
          else data.send(@get); end
        end

        def put(data, value)
          if @klass then @klass.send(:put, data, value)
          else data.send(@put, value); end

          def to_s name; end
        end
      end

      # Get the current value as a single object.
      def get
        type.get(self);
      end

      # Put value as an object of type type_
      def put(value, type_);
        type_.put(self, value);
      end

      # Constants for all the supported types
      NULL       = Type.new(Cproton::PN_NULL,       "null",      Null)
      BOOL       = Type.new(Cproton::PN_BOOL,       "bool")
      UBYTE      = Type.new(Cproton::PN_UBYTE,      "ubyte")
      BYTE       = Type.new(Cproton::PN_BYTE,       "byte")
      USHORT     = Type.new(Cproton::PN_USHORT,     "ushort")
      SHORT      = Type.new(Cproton::PN_SHORT,      "short")
      UINT       = Type.new(Cproton::PN_UINT,       "uint")
      INT        = Type.new(Cproton::PN_INT,        "int")
      CHAR       = Type.new(Cproton::PN_CHAR,       "char")
      ULONG      = Type.new(Cproton::PN_ULONG,      "ulong")
      LONG       = Type.new(Cproton::PN_LONG,       "long")
      TIMESTAMP  = Type.new(Cproton::PN_TIMESTAMP,  "timestamp")
      FLOAT      = Type.new(Cproton::PN_FLOAT,      "float")
      DOUBLE     = Type.new(Cproton::PN_DOUBLE,     "double")
      DECIMAL32  = Type.new(Cproton::PN_DECIMAL32,  "decimal32")
      DECIMAL64  = Type.new(Cproton::PN_DECIMAL64,  "decimal64")
      DECIMAL128 = Type.new(Cproton::PN_DECIMAL128, "decimal128")
      UUID       = Type.new(Cproton::PN_UUID,       "uuid")
      BINARY     = Type.new(Cproton::PN_BINARY,     "binary")
      STRING     = Type.new(Cproton::PN_STRING,     "string")
      SYMBOL     = Type.new(Cproton::PN_SYMBOL,     "symbol")
      DESCRIBED  = Type.new(Cproton::PN_DESCRIBED,  "described", Described)
      ARRAY      = Type.new(Cproton::PN_ARRAY,      "array",     Array)
      LIST       = Type.new(Cproton::PN_LIST,       "list",      List)
      MAP        = Type.new(Cproton::PN_MAP,        "map",       Map)

      private

      def valid_uuid?(value)
        # ensure that the UUID is in the right format
        # xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx
        value =~ /[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}/
      end

      def check(err) # :nodoc:
        if err < 0
          raise DataError, "[#{err}]: #{Cproton.pn_data_error(@data)}"
        else
          return err
        end
      end
    end
  end
end
