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

  class SessionPerConnection

    include Qpid::Proton::Util::Reactor

    def initialize
      @default_session = nil
    end

    def session(connection)
      if @default_session.nil?
        @default_session = self.create_session
        @default_session.context = self
      end
      return @default_session
    end

    def on_session_remote_close(event)
      event.connection.close
      @default_session = nil
    end

  end

end
