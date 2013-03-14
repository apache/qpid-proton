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

require "spec_helper"

module Qpid

  module Proton

    describe "A data object" do

      before :each do
        @data = Qpid::Proton::Data.new
      end

      it "can be initialized" do
        @data.should_not be_nil
      end

      it "can hold a null" do
        @data.null
        @data.null?.should be_true
      end

      it "can hold a true boolean" do
        @data.bool = true
        @data.bool.should be_true
      end

      it "can hold a false boolean" do
        @data.bool = false
        @data.bool.should_not be_true
      end

      it "raises an error on a negative ubyte" do
        expect {
          @data.ubyte = (0 - (rand(127) + 1))
        }.to raise_error(RangeError)
      end

      it "raises an error on a null ubyte" do
        expect {
          @data.ubyte = nil
        }.to raise_error(TypeError)
      end

      it "can hold an unsigned byte" do
        value = rand(255)
        @data.ubyte = value
        @data.ubyte.should == value
      end

      it "can hold a byte" do
        value = rand(128)
        @data.byte = value
        @data.byte.should == value
      end

      it "can hold a negative byte" do
        value = 0 - (rand(126) + 1)
        @data.byte = value
        @data.byte.should == value
      end

      it "raises an error on a negative ushort" do
        expect {
          @data.ushort = (0 - (rand(65535) + 1))
        }.to raise_error(RangeError)
      end

      it "raises an error on a nil ushort" do
        expect {
          @data.ushort = nil
        }.to raise_error(TypeError)
      end

      it "can hold a zero unsigned short" do
        @data.ushort = 0
        @data.ushort.should == 0
      end

      it "can hold an unsigned short" do
        value = rand(2**15) + 1
        @data.ushort = value
        @data.ushort.should == value
      end

      it "raises an error on a nil short" do
        expect {
          @data.short = nil
        }.to raise_error(TypeError)
      end

      it "can hold a short" do
        value = rand(2**15) + 1
        @data.short = value
        @data.short.should == value
      end

      it "can hold a zero short" do
        @data.short = 0
        @data.short.should == 0
      end

      it "can hold a negative short" do
        value = (0 - (rand(2**15) + 1))
        @data.short = value
        @data.short.should == value
      end

      it "raises an error on a nil uint" do
        expect {
          @data.uint = nil
        }.to raise_error(TypeError)
      end

      it "raises an error on a negative uint" do
        expect {
          @data.uint = (0 - (rand(2**32) + 1))
        }.to raise_error(RangeError)
      end

      it "can hold an unsigned integer" do
        value = rand(2**32) + 1
        @data.uint = value
        @data.uint.should == value
      end

      it "can hold a zero unsigned integer" do
        @data.uint = 0
        @data.uint.should == 0
      end

      it "raise an error on a null integer" do
        expect {
          @data.int = nil
        }.to raise_error(TypeError)
      end

      it "can hold an integer" do
        value = rand(2**31) + 1
        @data.int = value
        @data.int.should == value
      end

      it "can hold zero as an integer" do
        @data.int = 0
        @data.int.should == 0
      end

      it "raises an error on a null character" do
        expect {
          @data.char = nil
        }.to raise_error(TypeError)
      end

      it "can hold a character" do
        value = rand(256).chr.ord
        @data.char = value
        @data.char.should == value
      end

      it "raises an error on a null ulong" do
        expect {
          @data.ulong = nil
        }.to raise_error(TypeError)
      end

      it "raises an error on a negative ulong" do
        expect {
          @data.ulong = (0 - (rand(2**63) + 1))
        }.to raise_error(RangeError)
      end

      it "can have a zero unsigned long" do
        @data.ulong = 0
        @data.ulong.should == 0
      end

      it "can hold an unsigned long" do
        value = rand(2**63) + 1
        @data.ulong = value
        @data.ulong.should == value
      end

      it "raises an error on a null long" do
        expect {
          @data.long = nil
        }.to raise_error(TypeError)
      end

      it "can have a zero long" do
        @data.long = 0
        @data.long.should == 0
      end

      it "can hold a long" do
        value = rand(2**63) + 1
        @data.long = value
        @data.long.should == value
      end

      it "raise an error on a null timestamp" do
        expect {
          @data.timestamp = nil
        }.to raise_error(TypeError)
      end

      it "can handle a negative timestamp" do
        last_year = Time.now - (60*60*24*365)
        @data.timestamp = last_year
        @data.timestamp.should == last_year.to_i
      end

      it "can handle a zero timestamp" do
        @data.timestamp = 0
        @data.timestamp.should == 0
      end

      it "can hold a timestamp" do
        next_year = Time.now + (60*60*24*365)
        @data.timestamp = next_year
        @data.timestamp.should == next_year.to_i
      end

      it "raises an error on a null float" do
        expect {
          @data.float = nil
        }.to raise_error(TypeError)
      end

      it "can hold a negative float" do
        value = 0.0 - (1.0 + rand(2.0**15)).to_f
        @data.float = value
        @data.float.should == value
      end

      it "can hold a zero float" do
        @data.float = 0.0
        @data.float.should == 0.0
      end

      it "can hold a float" do
        value = (1.0 + rand(2.0**15)).to_f
        @data.float = value
        @data.float.should == value
      end

      it "raise an error on a null double" do
        expect {
          @data.double = nil
        }.to raise_error(TypeError)
      end

      it "can hold a negative double" do
        value = 0.0 - (1.0 + rand(2.0**31)).to_f
        @data.double = value
        @data.double.should == value
      end

      it "can hold a zero double" do
        @data.double = 0.0
        @data.double.should == 0.0
      end

      it "can hold a double" do
        value = (1.0 + rand(2.0**31)).to_f
        @data.double = value
        @data.double.should == value
      end

      it "raises an error on a null decimal32" do
        expect {
          @data.decimal32 = nil
        }.to raise_error(TypeError)
      end

      it "can hold a zero decimal32" do
        @data.decimal32 = 0
        @data.decimal32.should == 0
      end

      it "can hold a decimal32" do
        value = 1 + rand(2**31)
        @data.decimal32 = value
        @data.decimal32.should == value
      end

      it "raises an error on a null decimal64" do
        expect {
          @data.decimal64 = nil
        }.to raise_error(TypeError)
      end

      it "can hold a zero decimal64" do
        @data.decimal64 = 0
        @data.decimal64.should == 0
      end

      it "can hold a decimal64" do
        value = 1 + rand(2**63)
        @data.decimal64 = value
        @data.decimal64.should == value
      end

      it "raises an error on a null decimal128" do
        expect {
          @data.decimal128 = nil
        }.to raise_error(TypeError)
      end

      it "can hold a zero decimal128" do
        @data.decimal128 = 0
        @data.decimal128.should == 0
      end

      it "can hold a decimal128" do
        value = 1 + rand(2**127)
        @data.decimal128 = value
        @data.decimal128.should == value
      end

      it "raises an error on a null UUID" do
        expect {
          @data.uuid = nil
        }.to raise_error(ArgumentError)
      end

      it "raises an error on a malformed UUID" do
        expect {
          @data.uuid = random_string(36)
        }.to raise_error(ArgumentError)
      end

      it "can set a UUID from an integer value" do
        @data.uuid = 336307859334295828133695192821923655679
        @data.uuid.should == "fd0289a5-8eec-4a08-9283-81d02c9d2fff"
      end

      it "can hold a UUID" do
        value = "fd0289a5-8eec-4a08-9283-81d02c9d2fff"
        @data.uuid = value
        @data.uuid.should == value
      end

      it "can hold a null binary" do
        @data.binary = nil
        @data.binary.should == ""
      end

      it "can hold a binary" do
        value = random_string(128)
        @data.binary = value
        @data.binary.should == value
      end

      it "can hold a null string" do
        @data.string = nil
        @data.string.should == ""
      end

      it "can hold a string" do
        value = random_string(128)
        @data.string = value
        @data.string.should == value
      end

      it "can hold a null symbol" do
        @data.symbol = nil
        @data.symbol.should == ""
      end

      it "can hold a symbol" do
        value = random_string(128)
        @data.symbol = value
        @data.symbol.should == value
      end

      it "can hold a described value" do
        name = random_string(16)
        value = random_string(16)
        @data.put_described
        @data.enter
        @data.symbol = name
        @data.string = value
        @data.exit

        @data.described?.should be_true
        @data.enter
        @data.next
        @data.symbol.should == name
        @data.next
        @data.string.should == value
      end

      it "raises an error when setting the wrong type in an array"

      it "can hold an array" do
        values = []
        (1..(rand(100) + 5)).each { values << rand(2**16) }
        @data.put_array false, Data::INT
        @data.enter
        values.each { |value| @data.int = value }
        @data.exit

        @data.enter
        values.each do |value|
          @data.next
          @data.int. should == value
        end
      end

      it "can hold a described array" do
        values = []
        (1..(rand(100) + 5)).each { values << random_string(64) }
        descriptor = random_string(32)
        @data.put_array true, Data::STRING
        @data.enter
        @data.symbol = descriptor
        values.each { |value| @data.string = value }
        @data.exit

        @data.array.should == [values.size, true, Data::STRING]
        @data.enter
        @data.next
        @data.symbol.should == descriptor
        values.each do |value|
          @data.next
          @data.string.should == value
        end
      end

      it "can hold a list" do
        values = []
        (1..(rand(100) + 5)).each { values << random_string(128) }
        @data.put_list
        @data.enter
        values.each {|value| @data.string = value}
        @data.exit

        @data.enter
        values.each do |value|
          @data.next
          @data.string.should == value
        end
      end

      it "can hold a map" do
        keys = []
        (1..(rand(100) + 5)).each {keys << random_string(128)}
        values = {}
        keys.each {|key| values[key] = random_string(128)}

        @data.put_map
        @data.enter
        keys.each do |key|
          @data.string = key
          @data.string = values[key]
        end
        @data.exit

        @data.enter
        keys.each do |key|
          @data.next
          @data.string.should == key
          @data.next
          @data.string.should == values[key]
        end
      end

    end

  end

end
