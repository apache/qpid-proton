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

  # Provides methods for converting between milliseconds, seconds
  # and timeout values.
  #
  # @private
  module Timeout

    def sec_to_millis(s)
      return (s * 1000).to_int
    end

    def millis_to_sec(ms)
      return (ms.to_f / 1000.0).to_int
    end

    def timeout_to_millis(s)
      return Cproton::PN_MILLIS_MAX if s.nil?

      return sec_to_millis(s)
    end

    def millis_to_timeout(ms)
      return nil if ms == Cproton::PN_MILLIS_MAX

      return millis_to_sec(ms)
    end

  end

end
