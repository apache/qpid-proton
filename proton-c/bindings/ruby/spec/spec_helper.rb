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

begin
  require "simplecov"
  puts "simplecov available"

  SimpleCov.start do
    add_filter "/lib/*/*.rb"
    add_filter "message_format.rb"
  end

rescue
  puts "simplecov not available"
end

require "securerandom"
require "qpid_proton"

# Generates a random string of the specified length
def random_string(length = 8)
  (0...length).map{65.+(rand(25)).chr}.join
end

# Generates a random list of the specified length.
def random_list(length)
  result = []
  (0...length).each do |element|
    type = rand(8192) % 4
    low = rand(512)
    high = rand(8192)

    case
    when element == 0 then result << rand(128)
    when element == 1 then result << random_string(rand(128))
    when element == 2 then result << rand * (low - high).abs + low
    when element == 3 then result << SecureRandom.uuid
    end
  end

  return result
end

# Generates a random array of a random type.
# Returns both the array and the type.
def random_array(length, described = false, description = nil)
  result = []
  type = rand(128) % 4
  low = rand(512)
  high = rand(8192)

  (0...length).each do |element|
    case
      when type == 0 then result << rand(1024)
      when type == 1 then result << random_string(rand(128))
      when type == 2 then result << rand * (low - high).abs + low
      when type == 3 then result << SecureRandom.uuid
    end
  end

  # create the array header
  case
    when type == 0 then type = Qpid::Proton::INT
    when type == 1 then type = Qpid::Proton::STRING
    when type == 2 then type = Qpid::Proton::FLOAT
    when type == 3 then type = Qpid::Proton::UUID
  end

  result.proton_array_header = Qpid::Proton::ArrayHeader.new(type, description)

  return result
end

# Generates a random hash of values.
def random_hash(length)
  result = {}
  values = random_list(length)
  values.each do |value|
    result[random_string(64)] = value
  end
  return result
end

# taken from http://stackoverflow.com/questions/6855944/rounding-problem-with-rspec-tests-when-comparing-float-arrays
RSpec::Matchers.define :be_close_array do |expected, truth|
  match do |actual|
    same = 0
    for i in 0..actual.length-1
      same +=1 if actual[i].round(truth) == expected[i].round(truth)
    end
    same == actual.length
  end

  failure_message_for_should do |actual|
    "expected that #{actual} would be close to #{expected}"
  end

  failure_message_for_should_not do |actual|
    "expected that #{actual} would not be close to #{expected}"
  end

  description do
    "be a close to #{expected}"
  end
end
