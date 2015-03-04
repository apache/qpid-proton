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

module Qpid::Proton::Util

  # This mixin provides a method for mapping from an underlying Proton
  # C library class to a Ruby class.
  #
  # @private
  #
  module ClassWrapper

    WRAPPERS =
      {
        "pn_void" => proc {|x| Cproton.pn_void2rb(x)},
        "pn_rbref" => proc {|x| Cproton.pn_void2rb(x)},
        "pn_connection" => proc {|x| Qpid::Proton::Connection.wrap(Cproton.pn_cast_pn_connection(x))},
        "pn_session" => proc {|x| Qpid::Proton::Session.wrap(Cproton.pn_cast_pn_session(x))},
        "pn_link" => proc {|x| Qpid::Proton::Link.wrap(Cproton.pn_cast_pn_link(x))},
        "pn_delivery" => proc {|x| Qpid::Proton::Delivery.wrap(Cproton.pn_cast_pn_delivery(x))},
        "pn_transport" => proc {|x| Qpid::Proton::Transport.wrap(Cproton.pn_cast_pn_transport(x))},
        "pn_selectable" => proc {|x| Qpid::Proton::Selectable.wrap(Cproton.pn_cast_pn_selectable(x))},
        "pn_reactor" => proc {|x| Qpid::Proton::Reactor::Reactor.wrap(Cproton.pn_cast_pn_reactor(x))},
        "pn_task" => proc {|x| Qpid::Proton::Reactor::Task.wrap(Cproton.pn_cast_pn_task(x))},
      }

    def class_wrapper(clazz, c_impl, &block)
      proc_func = WRAPPERS[clazz]
      if !proc_func.nil?
        proc_func.yield(c_impl)
      elsif block_given?
        yield(c_impl)
      end
    end

  end

end
