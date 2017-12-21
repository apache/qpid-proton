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


require 'minitest/autorun'
require 'qpid_proton'

class TestURI < Minitest::Test

  def uri(u) Qpid::Proton::uri(u); end

  # Extension to standard URI parser
  def test_standard
    u = URI("amqp://u:p@h/x")
    assert_equal URI::AMQP, u.class
    assert_equal ['amqp', 'u:p', 'h', 5672, '/x'], u.select(:scheme, :userinfo, :host, :port, :path)

    u = URI("amqps://u:p@h/x")
    assert_equal URI::AMQPS, u.class
    assert_equal ['amqps', 'u:p', 'h', 5671, '/x'], u.select(:scheme, :userinfo, :host, :port, :path)

    assert_equal ['amqp', '[::1:2:3]', 5672], URI('amqp://[::1:2:3]').select(:scheme, :host, :port)
  end

  # Proton::uri on valid URIs
  def test_valid
    u = uri("amqp://u:p@h:1/x")
    assert_equal URI::AMQP, u.class
    assert_equal u.select(:scheme, :userinfo, :host, :port, :path), ['amqp', 'u:p', 'h', 1, '/x']

    u = uri("amqps://u:p@h:1/x")
    assert_equal URI::AMQPS, u.class
    assert_equal u.select(:scheme, :userinfo, :host, :port, :path), ['amqps', 'u:p', 'h', 1, '/x']

    # Schemeless string -> amqp
    assert_equal URI("amqp://h:1/x"), uri("//h:1/x")
    assert_equal URI("amqp:/x"), uri("/x")
    assert_equal URI("amqp:"), uri("//")
    assert_equal URI("amqp:"), uri("")
    assert_equal URI("amqp://[::1]"), uri("//[::1]")

    # Schemeless URI -> amqp, no re-parse for ambiguous case of path only
    assert_equal URI("amqp:x"), uri(URI("x"))
    assert_equal URI("amqp:/x"), uri(URI("/x"))

    # Pass-through
    u = uri('')
    assert_same u, uri(u)
  end

  # Proton::uri non-standard shortcuts
  def test_shortcut
    assert_equal URI("amqp://u:p@h:1/x"), uri("u:p@h:1/x")
    assert_equal URI("amqp://h:1"), uri("h:1")
    assert_equal URI("amqp://h"), uri("h")
    assert_equal URI("amqp://h"), uri("h:")
    assert_equal URI("amqp://:1"), uri(":1")
    assert_equal URI("amqp://[::1:2]:1"), uri("[::1:2]:1")
    assert_equal URI("amqp://[::1:2]"), uri("[::1:2]")
  end

  def test_error
    assert_raises(::ArgumentError) { uri(nil) }
    assert_raises(URI::BadURIError) { uri(URI("http:x")) } # Don't re-parse a URI with wrong scheme
    assert_raises(URI::InvalidURIError) { uri("x:y:z") } # Nonsense
    assert_raises(URI::InvalidURIError) { uri("amqp://[foobar]") } # Bad host
  end

end
