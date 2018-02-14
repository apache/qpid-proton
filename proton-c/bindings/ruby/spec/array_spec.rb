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

require 'spec_helper'

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

    value.must_respond_to(:proton_put)
    value.must_respond_to(:proton_array_header)
    value.class.must_respond_to(:proton_get)
  end

  it "raises an error when the current object is not a list" do
    @data.string = random_string(128)
    @data.rewind
    proc { @data.list }.must_raise(TypeError)
  end

  it "can be put into a Data object as a list" do
    @data.list= @list
    result = @data.list
    result.must_equal(@list)
  end

  it "raises an error when the elements of an Array are dissimilar and is put into a Data object" do
    value = Qpid::Proton::Types::UniformArray.new(Qpid::Proton::Codec::INT)
    value << random_string(16)
    proc { @data << value }.must_raise(TypeError)
  end

  it "can be put into a Data object as an undescribed array" do
    @data << @undescribed
    result = @data.array
    result.must_be_kind_of Qpid::Proton::Types::UniformArray
    @undescribed.must_equal(result)
  end

  it "can be put into a Data object as a described array" do
    @data << @described
    result = @data.array
    @described.must_equal(result)
    result.must_be_kind_of Qpid::Proton::Types::UniformArray
  end

end
