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
require 'weakref'

def show_registry(registry)
  registry.each_pair do |key, value|
    registry.delete(key) if value.weakref_alive?
  end
  puts "The contents of the registry: size=#{registry.size}"
end

def show_object_count(clazz)
  puts "There are #{ObjectSpace.each_object(clazz).count} instances of #{clazz}."
end

impl = Cproton.pn_transport
implclazz = impl.class
transport = Qpid::Proton::Transport.wrap(impl)

puts "Initial setup:"
show_object_count(Qpid::Proton::Transport)
show_object_count(implclazz)

transport = nil

show_registry(Qpid::Proton.registry)

ObjectSpace.garbage_collect

puts "After garbage collection:"
show_object_count(Qpid::Proton::Transport)
show_object_count(implclazz)

MAXCOUNT=100000
(1..MAXCOUNT).each do |which|
  nimpl = Cproton.pn_transport
  Cproton.pn_incref(nimpl)
  transport = Qpid::Proton::Transport.wrap(nimpl)
  transport = Qpid::Proton::Transport.wrap(nimpl)
end

transport = nil

puts "After creating #{MAXCOUNT} instances"
show_object_count(Qpid::Proton::Transport)
show_object_count(implclazz)
show_registry(Qpid::Proton.registry)

ObjectSpace.garbage_collect

transport = Qpid::Proton::Transport.wrap(impl)

puts "After garbage collection:"
puts "impl=#{impl}"
puts "transport=#{transport}"
show_object_count(Qpid::Proton::Transport)
show_object_count(implclazz)
show_registry(Qpid::Proton.registry)
