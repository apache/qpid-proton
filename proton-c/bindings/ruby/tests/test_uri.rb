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

require 'minitest/autorun'
require 'qpid_proton'

class TestURI < Minitest::Test

  def amqp_uri(u) Qpid::Proton::amqp_uri(u); end

  def test_amqp_uri
    assert_equal URI("amqp:").port, 5672
    assert_equal URI("amqps:").port, 5671
    assert_equal URI("amqp://user:pass@host:1234/path"), amqp_uri("//user:pass@host:1234/path")
    assert_equal URI("amqp://user:pass@host:1234/path"), amqp_uri("amqp://user:pass@host:1234/path")
    assert_equal URI("amqps://user:pass@host:1234/path"), amqp_uri("amqps://user:pass@host:1234/path")
    assert_equal URI("amqp://host:1234/path"), amqp_uri("//host:1234/path")
    assert_equal URI("amqp://host:1234"), amqp_uri("//host:1234")
    assert_equal URI("amqp://host"), amqp_uri("//host")
    assert_equal URI("amqp://:1234"), amqp_uri("//:1234")
    assert_raises(URI::BadURIError) { amqp_uri("http://foo") }
  end

end
