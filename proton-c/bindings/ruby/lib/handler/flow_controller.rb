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

# @private
module Qpid::Proton::Handler

  # Mixin to establish automatic flow control for a prefetch window
  # Uses {#@prefetch}
  #
  module FlowController

    def on_link_local_open(event) topup(event); super; end
    def on_link_remote_open(event) topup(event); super; end
    def on_delivery(event) topup(event); super; end
    def on_link_flow(event) topup(event); super; end

    def add_credit(event)
      r = event.receiver
      if r && r.open? && (r.drained == 0) && @handler.prefetch && (@handler.prefetch > r.credit)
        r.flow(@handler.prefetch - r.credit)
      end
    end
  end
end
