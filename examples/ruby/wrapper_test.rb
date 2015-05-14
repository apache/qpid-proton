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

require 'qpid_proton'

def how_many_transports?(expected)
  count = ObjectSpace.each_object(Qpid::Proton::Transport).count
  if expected.min == expected.max
    expectation = "#{expected.min}"
  else
    expectation = "#{expected.min} <= count <= #{expected.max}"
  end
  puts "Transport count: found #{count}, expected #{expectation} (#{expected.include?(count) ? 'Good' : 'Bad'})"
end

transport = Qpid::Proton::Transport.new
timpl = transport.impl

puts "================================="
puts "= Storing my original transport ="
puts "================================="
puts "   Stored transport=#{transport} (#{Cproton.pni_address_of(timpl).to_s(16)})"
how_many_transports?(1..1)
puts "================================="
transport.instance_eval { @first_name = "Darryl"; @last_name = "Pierce", @instance_id = 717 }
transport = nil


puts ""
max = 1000
puts "Creating #{max} instances of Transport"
(0...max).each do |which|
  t = Qpid::Proton::Transport.new
  t.instance_eval { @instance_id = which }
  t = nil
end

puts ""
puts "===================================="
puts "= Retrieving my original transport ="
puts "===================================="
transport = Qpid::Proton::Transport.wrap(timpl)
puts "Retrieved transport=#{transport} (#{Cproton.pni_address_of(timpl).to_s(16)})"
how_many_transports?(1..1001)
puts "===================================="
puts "My transport attributes:"
puts transport

transport = nil
GC.start
how_many_transports?(1..1)

puts ""
puts "======================================"
puts "= Throwing away the Transport object ="
puts "======================================"
transport = nil
timpl.instance_eval { @proton_wrapper = nil }
GC.start
begin
  transport = Qpid::Proton::Transport.wrap(timpl)
  puts "!!! This should fail!"
rescue Qpid::Proton::ProtonError => error
  puts "Good, it failed..."
end
how_many_transports?(0..0)
