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

module Qpid::Proton::Handler

  # @private
  class CAdaptor

    def initialize(handler, on_error = nil)
      @handler = handler
      @on_error = on_error
    end

    def dispatch(cevent, ctype)
      event = Qpid::Proton::Event::Event.wrap(cevent, ctype)
      # TODO add a variable to enable this programmatically
      # print "EVENT: #{event} going to #{@handler}\n"
      event.dispatch(@handler)
    end

    def exception(error)
      if @on_error.nil?
        raise error
      else
        @on_error.call(error)
      end
    end

  end

end
