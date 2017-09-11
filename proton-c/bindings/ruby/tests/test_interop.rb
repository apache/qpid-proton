#!/usr/bin/env ruby

require 'minitest/autorun'
require 'minitest/unit'
require 'qpid_proton'

if ((RUBY_VERSION.split(".").map {|x| x.to_i}  <=> [1, 9]) < 0)
  require 'pathname'
  class File
    def self.absolute_path(name)
      return Pathname.new(name).realpath
    end
  end
end

class InteropTest < MiniTest::Test
  Data = Qpid::Proton::Codec::Data
  Message = Qpid::Proton::Message

  def setup
    @data = Data.new
    @message = Message.new
  end

  # Walk up the directory tree to find the tests directory.
  def get_data(name)
    path = File.join(File.dirname(__FILE__), "../../../../tests/interop/#{name}.amqp")
    raise "Can't find test/interop directory from #{__FILE__}" unless File.exists?(path)
    File.open(path, "rb") { |f| f.read }
  end

  # Decode encoded bytes as a Data object
  def decode_data(encoded)
    buffer = encoded
    while buffer.size > 0
      n = @data.decode(buffer)
      buffer = buffer[n..-1]
    end
    @data.rewind
    reencoded = @data.encode
    # Test the round-trip re-encoding gives the same result.
    assert_equal(encoded, reencoded)
  end

  def decode_data_file(name) decode_data(get_data(name)); end

  def decode_message_file(name)
    message = Message.new()
    message.decode(self.get_data(name))
    self.decode_data(message.body)
  end

  def assert_next(type, value)
    assert @data.next
    assert_equal(type, @data.type)
    assert_equal(value, type.get(@data))
  end

  def assert_array_next(expected, header)
    assert_next(Qpid::Proton::Codec::ARRAY, expected)
    result = @data.type.get(@data)
    assert_equal(result.proton_array_header, header)
  end

  def test_message
    decode_message_file("message")
    assert_next(Qpid::Proton::Codec::STRING, "hello")
    assert !@data.next
  end

  def test_primitives
    decode_data_file("primitives")
    assert_next(Qpid::Proton::Codec::BOOL, true)
    assert_next(Qpid::Proton::Codec::BOOL, false)
    assert_next(Qpid::Proton::Codec::UBYTE, 42)
    assert_next(Qpid::Proton::Codec::USHORT, 42)
    assert_next(Qpid::Proton::Codec::SHORT, -42)
    assert_next(Qpid::Proton::Codec::UINT, 12345)
    assert_next(Qpid::Proton::Codec::INT, -12345)
    assert_next(Qpid::Proton::Codec::ULONG, 12345)
    assert_next(Qpid::Proton::Codec::LONG, -12345)
    assert_next(Qpid::Proton::Codec::FLOAT, 0.125)
    assert_next(Qpid::Proton::Codec::DOUBLE, 0.125)
    assert !@data.next
  end

  def test_strings
    decode_data_file("strings")
    assert_next(Qpid::Proton::Codec::BINARY, "abc\0defg")
    assert_next(Qpid::Proton::Codec::STRING, "abcdefg")
    assert_next(Qpid::Proton::Codec::SYMBOL, "abcdefg")
    assert_next(Qpid::Proton::Codec::BINARY, "")
    assert_next(Qpid::Proton::Codec::STRING, "")
    assert_next(Qpid::Proton::Codec::SYMBOL, "")
    assert !@data.next
  end

  def test_described
    decode_data_file("described")
    assert_next(Qpid::Proton::Codec::DESCRIBED, Qpid::Proton::Types::Described.new("foo-descriptor", "foo-value"))
    assert(@data.described?)
    assert_next(Qpid::Proton::Codec::DESCRIBED, Qpid::Proton::Types::Described.new(12, 13))
    assert(@data.described?)
    assert !@data.next
  end

  def test_described_array
    decode_data_file("described_array")
    assert_array_next((0...10).to_a,
                       Qpid::Proton::Types::ArrayHeader.new(Qpid::Proton::Codec::INT,
                                                     "int-array"))
  end

  def test_arrays
    decode_data_file("arrays")
    assert_array_next((0...100).to_a,
                      Qpid::Proton::Types::ArrayHeader.new(Qpid::Proton::Codec::INT))
    assert_array_next(["a", "b", "c"],
                      Qpid::Proton::Types::ArrayHeader.new(Qpid::Proton::Codec::STRING))
    assert_array_next([],
                      Qpid::Proton::Types::ArrayHeader.new(Qpid::Proton::Codec::INT))
    assert !@data.next
  end

  def test_lists
    decode_data_file("lists")
    assert_next(Qpid::Proton::Codec::LIST, [32, "foo", true])
    assert_next(Qpid::Proton::Codec::LIST, [])
    assert !@data.next
  end

  def test_maps
    decode_data_file("maps")
    assert_next(Qpid::Proton::Codec::MAP, {"one" => 1, "two" => 2, "three" => 3 })
    assert_next(Qpid::Proton::Codec::MAP, {1 => "one", 2 => "two", 3 => "three"})
    assert_next(Qpid::Proton::Codec::MAP, {})
    assert !@data.next
  end
end
