#!/usr/bin/env ruby

require 'test/unit'
require 'qpid_proton'

if ((RUBY_VERSION.split(".").map {|x| x.to_i}  <=> [1, 9]) < 0)
  require 'pathname'
  class File
    def self.absolute_path(name)
      return Pathname.new(name).realpath
    end
  end
end

class InteropTest < Test::Unit::TestCase
  Data = Qpid::Proton::Data
  Message = Qpid::Proton::Message

  def setup
    @data = Data.new
    @message = Message.new
  end

  # Walk up the directory tree to find the tests directory.
  def get_data(name)
    path = File.absolute_path(__FILE__)
    while path and File.basename(path) != "tests" do path = File.dirname(path); end
    path = File.join(path,"interop")
    raise "Can't find test/interop directory from #{__FILE__}" unless File.directory?(path)
    path = File.join(path,"#{name}.amqp")
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
    self.decode_data(message.content)
  end

  def assert_next(type, value)
    assert @data.next
    assert_equal(type, @data.type)
    assert_equal(value, type.get(@data))
  end

  def assert_array_next(expected, header)
    assert_next(Qpid::Proton::ARRAY, expected)
    result = @data.type.get(@data)
    assert_equal(result.proton_array_header, header)
  end

  def test_message
    decode_message_file("message")
    assert_next(Qpid::Proton::STRING, "hello")
    assert !@data.next
  end

  def test_primitives
    decode_data_file("primitives")
    assert_next(Qpid::Proton::BOOL, true)
    assert_next(Qpid::Proton::BOOL, false)
    assert_next(Qpid::Proton::UBYTE, 42)
    assert_next(Qpid::Proton::USHORT, 42)
    assert_next(Qpid::Proton::SHORT, -42)
    assert_next(Qpid::Proton::UINT, 12345)
    assert_next(Qpid::Proton::INT, -12345)
    assert_next(Qpid::Proton::ULONG, 12345)
    assert_next(Qpid::Proton::LONG, -12345)
    assert_next(Qpid::Proton::FLOAT, 0.125)
    assert_next(Qpid::Proton::DOUBLE, 0.125)
    assert !@data.next
  end

  def test_strings
    decode_data_file("strings")
    assert_next(Qpid::Proton::BINARY, "abc\0defg")
    assert_next(Qpid::Proton::STRING, "abcdefg")
    assert_next(Qpid::Proton::SYMBOL, "abcdefg")
    assert_next(Qpid::Proton::BINARY, "")
    assert_next(Qpid::Proton::STRING, "")
    assert_next(Qpid::Proton::SYMBOL, "")
    assert !@data.next
  end

  def test_described
    decode_data_file("described")
    assert_next(Qpid::Proton::DESCRIBED, Qpid::Proton::Described.new("foo-descriptor", "foo-value"))
    assert(@data.described?)
    assert_next(Qpid::Proton::DESCRIBED, Qpid::Proton::Described.new(12, 13))
    assert(@data.described?)
    assert !@data.next
  end

  def test_described_array
    decode_data_file("described_array")
    assert_array_next((0...10).to_a,
                       Qpid::Proton::ArrayHeader.new(Qpid::Proton::INT,
                                                     "int-array"))
  end

  def test_arrays
    decode_data_file("arrays")
    assert_array_next((0...100).to_a,
                      Qpid::Proton::ArrayHeader.new(Qpid::Proton::INT))
    assert_array_next(["a", "b", "c"],
                      Qpid::Proton::ArrayHeader.new(Qpid::Proton::STRING))
    assert_array_next([],
                      Qpid::Proton::ArrayHeader.new(Qpid::Proton::INT))
    assert !@data.next
  end

  def test_lists
    decode_data_file("lists")
    assert_next(Qpid::Proton::LIST, [32, "foo", true])
    assert_next(Qpid::Proton::LIST, [])
    assert !@data.next
  end

  def test_maps
    decode_data_file("maps")
    assert_next(Qpid::Proton::MAP, {"one" => 1, "two" => 2, "three" => 3 })
    assert_next(Qpid::Proton::MAP, {1 => "one", 2 => "two", 3 => "three"})
    assert_next(Qpid::Proton::MAP, {})
    assert !@data.next
  end
end
