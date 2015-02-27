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

module Qpid::Proton::Reactor

  class Acceptor

    include Qpid::Proton::Util::Wrapper

    def initialize(impl)
      @impl = impl
      self.class.store_instance(self)
    end

    def set_ssl_domain(ssl_domain)
      Cproton.pn_acceptor_set_ssl_domain(@impl, ssl_domain.impl)
    end

    def close
      Cproton.pn_acceptor_close(@impl)
    end

  end

end
