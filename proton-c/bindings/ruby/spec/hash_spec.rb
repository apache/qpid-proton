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

  it "can be put into an instance of Data" do
    @data.map = @hash
    result = @data.map
    result.keys.must_equal(@hash.keys)
    result.values.must_equal(@hash.values)
  end

  it "raises an error when trying to get what is not a Hash" do
    @data.string = random_string(128)
    @data.rewind

    proc {
      @data.map
    }.must_raise(TypeError)
  end

end
