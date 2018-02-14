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


require 'test_tools'
require "securerandom"
require 'qpid_proton'

class TestData < MiniTest::Test
  include Qpid::Proton

  def assert_from_to(*values)
    d = Codec::Data.new
    values.each do |x|
      Codec::Data.from_object(d.impl, x)
      assert_equal x, Codec::Data.to_object(d.impl)
    end
  end

  def test_from_to
    assert_from_to({ 1 => :one, 2=>:two })
    assert_from_to([{:a => 1, "b" => 2}, 3, 4.4, :five])
    assert_from_to(Types::UniformArray.new(Types::INT, [1, 2, 3, 4]))
  end

  def rnum(*arg) SecureRandom.random_number(*arg); end
  def rstr(*arg) SecureRandom.base64(*arg); end

  def test_nil()
    assert_nil((Codec::Data.new << nil).object)
  end

  def assert_convert(*values)
    values.each { |x| assert_equal x, ((Codec::Data.new << x).object) }
  end

  def test_bool()
    assert_convert(true, false)
  end

  def test_float()
    assert_convert(0.0, 1.0, -1.0, 1.23e123, rnum(), rnum(), rnum(), rnum())
  end

  def test_string()
    assert_convert("", "foo", rstr(100000), rstr(rnum(1000)), rstr(rnum(1000)))
  end

  def test_symbol()
    assert_convert(:"", :foo, rstr(256).to_sym)
  end
end
