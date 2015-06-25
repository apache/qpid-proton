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

describe "The extended array type" do

  before :each do
    @data        = Qpid::Proton::Codec::Data.new
    @list        = random_list(rand(100))
    @undescribed = random_array(rand(100))
    @description = random_string(128)
    @described   = random_array(rand(100), true, @description)
  end

  it "can be created like a normal array" do
    value = []

    expect(value).to respond_to(:proton_put)
    expect(value).to respond_to(:proton_array_header)
    expect(value.class).to respond_to(:proton_get)
    expect(value).to respond_to :proton_described?
  end

  it "raises an error when putting into a nil Data object" do
    expect {
      @list.proton_put(nil)
    }.to raise_error
  end

  it "raises an error when getting from a nil Data object" do
    expect {
      Array.proton_get(nil)
    }.to raise_error(TypeError)
  end

  it "raises an error when the data object is empty" do
    expect {
      Array.proton_get(@data)
    }.to raise_error(TypeError)
  end

  it "raises an error when the current object is not a list" do
    @data.string = random_string(128)
    @data.rewind

    expect {
      Array.proton_get(@data)
    }.to raise_error(TypeError)
  end

  it "does not have an array header when it's a simple list" do
    expect(@list.proton_described?).to eq(false)
  end

  it "can be put into a Data object as a list" do
    @list.proton_put(@data)
    result = Array.proton_get(@data)
    expect(result).to match_array(@list)
    expect(result.proton_array_header).to eq(nil)
  end

  it "has an array header when it's an AMQP array" do
    expect(@undescribed.proton_array_header).not_to be_nil
    expect(@described.proton_array_header).not_to be_nil
  end

  it "raises an error when the elements of an Array are dissimilar and is put into a Data object" do
    value = []
    value.proton_array_header = Qpid::Proton::Types::ArrayHeader.new(Qpid::Proton::Codec::INT)
    value << random_string(16)

    expect {
      value.proton_put(@data)
    }.to raise_error(TypeError)
  end

  it "can be put into a Data object as an undescribed array" do
    @undescribed.proton_put(@data)
    result = Array.proton_get(@data)
    be_close_array(@undescribed, result)

    expect(result.proton_array_header).not_to be_nil
    expect(result.proton_array_header).to eq(@undescribed.proton_array_header)
    expect(result.proton_array_header.described?).to eq(false)
  end

  it "can be put into a Data object as a described array" do
    @described.proton_put(@data)
    result = Array.proton_get(@data)
    be_close_array(@described, result)

    expect(result.proton_array_header).not_to be_nil
    expect(result.proton_array_header).to eq(@described.proton_array_header)
    expect(result.proton_array_header.described?).to eq(true)
  end

end
