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

require 'uri'

module URI
  # AMQP URI scheme for the AMQP protocol
  class AMQP < Generic
    DEFAULT_PORT = 5672
  end
  @@schemes['AMQP'] = AMQP

  # AMQPS URI scheme for the AMQP protocol over TLS
  class AMQPS < AMQP
    DEFAULT_PORT = 5671
  end
  @@schemes['AMQPS'] = AMQPS
end

module Qpid::Proton
  # Convert s to an {URI::AMQP} or {URI::AMQPS}
  # @param s [String,URI] If s has no scheme, use the {URI::AMQP} scheme
  # @return [URI::AMQP]
  # @raise [BadURIError] If s has a scheme that is not "amqp" or "amqps"
  def self.amqp_uri(s)
    u = URI(s)
    u.host ||= ""               # Behaves badly with nil host
    return u if u.is_a? URI::AMQP
    raise URI::BadURIError, "Not an AMQP URI: '#{u}'" if u.scheme
    u.scheme = "amqp" unless u.scheme
    u = URI::parse(u.to_s)      # Re-parse with amqp scheme
    return u
  end
end
