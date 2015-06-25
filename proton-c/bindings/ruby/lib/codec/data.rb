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

module Qpid::Proton::Codec

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
  # * NULL
  # * BOOL
  # * UBYTE
  # * BYTE
  # * USHORT
  # * SHORT
  # * UINT
  # * INT
  # * CHAR
  # * ULONG
  # * LONG
  # * TIMESTAMP
  # * FLOAT
  # * DOUBLE
  # * DECIMAL32
  # * DECIMAL64
  # * DECIMAL128
  # * UUID
  # * BINARY
  # * STRING
  # * SYMBOL
  #
  # The following types of compound values are supported:
  #
  # * DESCRIBED
  # * ARRAY
  # * LIST
  # * MAP
  #
  class Data

    # Creates a new instance with the specified capacity.
    #
    # @param capacity [Fixnum, Object] The initial capacity or content.
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
      ObjectSpace.define_finalizer(self, self.class.finalize!(@data, @free))
    end

    # @private
    def self.finalize!(data, free)
      proc {
        Cproton.pn_data_free(data) if free
      }
    end

    # @private
    def to_s
      tmp = Cproton.pn_string("")
      Cproton.pn_inspect(@data, tmp)
      result = Cproton.pn_string_get(tmp)
      Cproton.pn_free(tmp)
      return result
    end

    # Clears the object.
    #
    def clear
      Cproton.pn_data_clear(@data)
    end

    # Clears the current node and sets the parent to the root node.
    #
    # Clearing the current node sets it *before* the first node, calling
    # #next will advance to the first node.
    #
    def rewind
      Cproton.pn_data_rewind(@data)
    end

    # Advances the current node to its next sibling and returns its types.
    #
    # If there is no next sibling the current node remains unchanged
    # and nil is returned.
    #
    def next
      Cproton.pn_data_next(@data)
    end

    # Advances the current node to its previous sibling and returns its type.
    #
    # If there is no previous sibling then the current node remains unchanged
    # and nil is return.
    #
    def prev
      return Cproton.pn_data_prev(@data) ? type : nil
    end

    # Sets the parent node to the current node and clears the current node.
    #
    # Clearing the current node sets it _before_ the first child.
    #
    def enter
      Cproton.pn_data_enter(@data)
    end

    # Sets the current node to the parent node and the parent node to its own
    # parent.
    #
    def exit
      Cproton.pn_data_exit(@data)
    end

    # Returns the numeric type code of the current node.
    #
    # @return [Fixnum] The current node type.
    # @return [nil] If there is no current node.
    #
    def type_code
      dtype = Cproton.pn_data_type(@data)
      return (dtype == -1) ? nil : dtype
    end

    # Return the type object for the current node
    #
    # @param [Fixnum] The object type.
    #
    # @see #type_code
    #
    def type
      Mapping.for_code(type_code)
    end

    # Returns a representation of the data encoded in AMQP format.
    #
    # @return [String] The context of the Data as an AMQP data string.
    #
    # @example
    #
    #   @data.string = "This is a test."
    #   @encoded = @data.encode
    #
    #   # @encoded now contains the text "This is a test." encoded for
    #   # AMQP transport.
    #
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
    # @param encoded [String] The encoded data.
    #
    # @example
    #
    #   # SCENARIO: A string of encoded data, @encoded, contains the text
    #   #           of "This is a test." and is passed to an instance of Data
    #   #           for decoding.
    #
    #   @data.decode(@encoded)
    #   @data.string #=> "This is a test."
    #
    def decode(encoded)
      check(Cproton.pn_data_decode(@data, encoded, encoded.length))
    end

    # Puts a list value.
    #
    # Elements may be filled by entering the list node and putting element
    # values.
    #
    # @example
    #
    #   data = Qpid::Proton::Codec::Data.new
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
    # @example
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
    # @example
    #
    #   data = Qpid::Proton::Codec::Data.new
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
    # @example
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

    # @private
    def get_map
      ::Hash.proton_data_get(self)
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
    # @param described [Boolean] True if the array is described.
    # @param element_type [Fixnum] The AMQP type for each element of the array.
    #
    # @example
    #
    #   # create an array of integer values
    #   data = Qpid::Proton::Codec::Data.new
    #   data.put_array(false, INT)
    #   data.enter
    #   data.int = 1
    #   data.int = 2
    #   data.int = 3
    #   data.exit
    #
    #   # create a described  array of double values
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
    # @example
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
      [count, described, Mapping.for_code(array_type) ]
    end

    # @private
    def get_array
      ::Array.proton_get(self)
    end

    # Puts a described value.
    #
    # A described node has two children, the descriptor and the value.
    # These are specified by entering the node and putting the
    # desired values.
    #
    # @example
    #
    #   data = Qpid::Proton::Codec::Data.new
    #   data.put_described
    #   data.enter
    #   data.symbol = "value-descriptor"
    #   data.string = "the value"
    #   data.exit
    #
    def put_described
      check(Cproton.pn_data_put_described(@data))
    end

    # @private
    def get_described
      raise TypeError, "not a described type" unless self.described?
      self.enter
      self.next
      type = self.type
      descriptor = type.get(self)
      self.next
      type = self.type
      value = type.get(self)
      self.exit
      Qpid::Proton::Types::Described.new(descriptor, value)
    end

    # Checks if the current node is a described value.
    #
    # The described and value may be accessed by entering the described value.
    #
    # @example
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
    #
    def null
      check(Cproton.pn_data_put_null(@data))
    end

    # Utility method for Qpid::Proton::Codec::Mapping
    #
    # @private
    #
    def null=(value)
      null
    end

    # Puts an arbitrary object type.
    #
    # The Data instance will determine which AMQP type is appropriate and will
    # use that to encode the object.
    #
    # @param object [Object] The value.
    #
    def object=(object)
      Mapping.for_class(object.class).put(self, object)
    end

    # Gets the current node, based on how it was encoded.
    #
    # @return [Object] The current node.
    #
    def object
      type = self.type
      return nil if type.nil?
      type.get(data)
    end

    # Checks if the current node is null.
    #
    # @return [Boolean] True if the node is null.
    #
    def null?
      Cproton.pn_data_is_null(@data)
    end

    # Puts a boolean value.
    #
    # @param value [Boolean] The boolean value.
    #
    def bool=(value)
      check(Cproton.pn_data_put_bool(@data, value))
    end

    # If the current node is a boolean, then it returns the value. Otherwise,
    # it returns false.
    #
    # @return [Boolean] The boolean value.
    #
    def bool
      Cproton.pn_data_get_bool(@data)
    end

    # Puts an unsigned byte value.
    #
    # @param value [Fixnum] The unsigned byte value.
    #
    def ubyte=(value)
      check(Cproton.pn_data_put_ubyte(@data, value))
    end

    # If the current node is an unsigned byte, returns its value. Otherwise,
    # it returns 0.
    #
    # @return [Fixnum] The unsigned byte value.
    #
    def ubyte
      Cproton.pn_data_get_ubyte(@data)
    end

    # Puts a byte value.
    #
    # @param value [Fixnum] The byte value.
    #
    def byte=(value)
      check(Cproton.pn_data_put_byte(@data, value))
    end

    # If the current node is an byte, returns its value. Otherwise,
    # it returns 0.
    #
    # @return [Fixnum] The byte value.
    #
    def byte
      Cproton.pn_data_get_byte(@data)
    end

    # Puts an unsigned short value.
    #
    # @param value [Fixnum] The unsigned short value
    #
    def ushort=(value)
      check(Cproton.pn_data_put_ushort(@data, value))
    end

    # If the current node is an unsigned short, returns its value. Otherwise,
    # it returns 0.
    #
    # @return [Fixnum] The unsigned short value.
    #
    def ushort
      Cproton.pn_data_get_ushort(@data)
    end

    # Puts a short value.
    #
    # @param value [Fixnum] The short value.
    #
    def short=(value)
      check(Cproton.pn_data_put_short(@data, value))
    end

    # If the current node is a short, returns its value. Otherwise,
    # returns a 0.
    #
    # @return [Fixnum] The short value.
    #
    def short
      Cproton.pn_data_get_short(@data)
    end

    # Puts an unsigned integer value.
    #
    # @param value [Fixnum] the unsigned integer value
    #
    def uint=(value)
      raise TypeError if value.nil?
      raise RangeError, "invalid uint: #{value}" if value < 0
      check(Cproton.pn_data_put_uint(@data, value))
    end

    # If the current node is an unsigned int, returns its value. Otherwise,
    # returns 0.
    #
    # @return [Fixnum] The unsigned integer value.
    #
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
    #
    # @return [Fixnum] The integer value.
    #
    def int
      Cproton.pn_data_get_int(@data)
    end

    # Puts a character value.
    #
    # @param value [Fixnum] The character value.
    #
    def char=(value)
      check(Cproton.pn_data_put_char(@data, value))
    end

    # If the current node is a character, returns its value. Otherwise,
    # returns 0.
    #
    # @return [Fixnum] The character value.
    #
    def char
      Cproton.pn_data_get_char(@data)
    end

    # Puts an unsigned long value.
    #
    # @param value [Fixnum] The unsigned long value.
    #
    def ulong=(value)
      raise TypeError if value.nil?
      raise RangeError, "invalid ulong: #{value}" if value < 0
      check(Cproton.pn_data_put_ulong(@data, value))
    end

    # If the current node is an unsigned long, returns its value. Otherwise,
    # returns 0.
    #
    # @return [Fixnum] The unsigned long value.
    #
    def ulong
      Cproton.pn_data_get_ulong(@data)
    end

    # Puts a long value.
    #
    # @param value [Fixnum] The long value.
    #
    def long=(value)
      check(Cproton.pn_data_put_long(@data, value))
    end

    # If the current node is a long, returns its value. Otherwise, returns 0.
    #
    # @return [Fixnum] The long value.
    def long
      Cproton.pn_data_get_long(@data)
    end

    # Puts a timestamp value.
    #
    # @param value [Fixnum] The timestamp value.
    #
    def timestamp=(value)
      value = value.to_i if (!value.nil? && value.is_a?(Time))
      check(Cproton.pn_data_put_timestamp(@data, value))
    end

    # If the current node is a timestamp, returns its value. Otherwise,
    # returns 0.
    #
    # @return [Fixnum] The timestamp value.
    #
    def timestamp
      Cproton.pn_data_get_timestamp(@data)
    end

    # Puts a float value.
    #
    # @param value [Float] The floating point value.
    #
    def float=(value)
      check(Cproton.pn_data_put_float(@data, value))
    end

    # If the current node is a float, returns its value. Otherwise,
    # returns 0.
    #
    # @return [Float] The floating point value.
    #
    def float
      Cproton.pn_data_get_float(@data)
    end

    # Puts a double value.
    #
    # @param value [Float] The double precision floating point value.
    #
    def double=(value)
      check(Cproton.pn_data_put_double(@data, value))
    end

    # If the current node is a double, returns its value. Otherwise,
    # returns 0.
    #
    # @return [Float] The double precision floating point value.
    #
    def double
      Cproton.pn_data_get_double(@data)
    end

    # Puts a decimal32 value.
    #
    # @param value [Fixnum] The decimal32 value.
    #
    def decimal32=(value)
      check(Cproton.pn_data_put_decimal32(@data, value))
    end

    # If the current node is a decimal32, returns its value. Otherwise,
    # returns 0.
    #
    # @return [Fixnum] The decimal32 value.
    #
    def decimal32
      Cproton.pn_data_get_decimal32(@data)
    end

    # Puts a decimal64 value.
    #
    # @param value [Fixnum] The decimal64 value.
    #
    def decimal64=(value)
      check(Cproton.pn_data_put_decimal64(@data, value))
    end

    # If the current node is a decimal64, returns its value. Otherwise,
    # it returns 0.
    #
    # @return [Fixnum] The decimal64 value.
    #
    def decimal64
      Cproton.pn_data_get_decimal64(@data)
    end

    # Puts a decimal128 value.
    #
    # @param value [Fixnum] The decimal128 value.
    #
    def decimal128=(value)
      raise TypeError, "invalid decimal128 value: #{value}" if value.nil?
      value = value.to_s(16).rjust(32, "0")
      bytes = []
      value.scan(/(..)/) {|v| bytes << v[0].to_i(16)}
      check(Cproton.pn_data_put_decimal128(@data, bytes))
    end

    # If the current node is a decimal128, returns its value. Otherwise,
    # returns 0.
    #
    # @return [Fixnum] The decimal128 value.
    #
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
    # @param value [String, Numeric] A string or numeric representation of the UUID.
    #
    # @example
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
      raise ::ArgumentError, "invalid uuid: #{value}" if value.nil?

      # if the uuid that was submitted was numeric value, then translated
      # it into a hex string, otherwise assume it was a string represtation
      # and attempt to decode it
      if value.is_a? Numeric
        value = "%032x" % value
      else
        raise ::ArgumentError, "invalid uuid: #{value}" if !valid_uuid?(value)

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
    #
    # @return [String] The string representation of the UUID.
    #
    def uuid
      value = ""
      Cproton.pn_data_get_uuid(@data).each{|val| value += ("%02x" % val)}
      value.insert(8, "-").insert(13, "-").insert(18, "-").insert(23, "-")
    end

    # Puts a binary value.
    #
    # A binary string is encoded as an ASCII 8-bit string value. This is in
    # contranst to other strings, which are treated as UTF-8 encoded.
    #
    # @param value [String] An arbitrary string value.
    #
    # @see #string=
    #
    def binary=(value)
      check(Cproton.pn_data_put_binary(@data, value))
    end

    # If the current node is binary, returns its value. Otherwise, it returns
    # an empty string ("").
    #
    # @return [String] The binary string.
    #
    # @see #string
    #
    def binary
      Qpid::Proton::Types::BinaryString.new(Cproton.pn_data_get_binary(@data))
    end

    # Puts a UTF-8 encoded string value.
    #
    # *NOTE:* A nil value is stored as an empty string rather than as a nil.
    #
    # @param value [String] The UTF-8 encoded string value.
    #
    # @see #binary=
    #
    def string=(value)
      check(Cproton.pn_data_put_string(@data, value))
    end

    # If the current node is a string, returns its value. Otherwise, it
    # returns an empty string ("").
    #
    # @return [String] The UTF-8 encoded string.
    #
    # @see #binary
    #
    def string
      Qpid::Proton::Types::UTFString.new(Cproton.pn_data_get_string(@data))
    end

    # Puts a symbolic value.
    #
    # @param value [String] The symbolic string value.
    #
    def symbol=(value)
      check(Cproton.pn_data_put_symbol(@data, value))
    end

    # If the current node is a symbol, returns its value. Otherwise, it
    # returns an empty string ("").
    #
    # @return [String] The symbolic string value.
    #
    def symbol
      Cproton.pn_data_get_symbol(@data)
    end

    # Get the current value as a single object.
    #
    # @return [Object] The current node's object.
    #
    # @see #type_code
    # @see #type
    #
    def get
      type.get(self);
    end

    # Puts a new value with the given type into the current node.
    #
    # @param value [Object] The value.
    # @param type_code [Mapping] The value's type.
    #
    # @private
    #
    def put(value, type_code);
      type_code.put(self, value);
    end

    private

    def valid_uuid?(value)
      # ensure that the UUID is in the right format
      # xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx
      value =~ /[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}/
    end

    # @private
    def check(err)
      if err < 0
        raise DataError, "[#{err}]: #{Cproton.pn_data_error(@data)}"
      else
        return err
      end
    end

  end

end
