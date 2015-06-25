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

describe "The extended hash type" do

  before :each do
    @data = Qpid::Proton::Codec::Data.new
    @hash = random_hash(rand(128) + 64)
  end

  it "raises an error when put into a nil Data instance" do
    expect {
      @hash.proton_data_put(nil)
    }.to raise_error(TypeError)
  end

  it "can be put into an instance of Data" do
    @hash.proton_data_put(@data)
    result = Hash.proton_data_get(@data)
    expect(result.keys).to match_array(@hash.keys)
    expect(result.values).to match_array(@hash.values)
  end

  it "raises an error when retrieved from a nil Data instance" do
    expect {
      Hash.proton_data_get(nil)
    }.to raise_error(TypeError)
  end

  it "raises an error when trying to get what is not a Hash" do
    @data.string = random_string(128)
    @data.rewind

    expect {
      Hash.proton_data_get(@data)
    }.to raise_error(TypeError)
  end

end
