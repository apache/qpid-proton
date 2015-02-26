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

  class GlobalOverrides

    def initialize(base)
      @base = base
    end

    def on_unhandled(name, event)
      event.dispatch(@base) unless self.override?(event)
    end

    def override?(event)
      conn = event.connection
      if !conn.nil? && conn.overrides?
        overrides = conn.overrides
        result = event.dispatch(overrides)
        return result
      end
      false
    end

  end

end
