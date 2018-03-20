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


require 'uri'

# Extend the standard ruby {URI} with AMQP and AMQPS schemes
module URI
  # AMQP URI scheme for the AMQP protocol
  class AMQP < Generic
    DEFAULT_PORT = 5672

    # @return [String] The AMQP address is the {#path} stripped of any leading "/"
    def amqp_address() path[0] == "/" ? path[1..-1] : path; end
  end
  @@schemes['AMQP'] = AMQP

  # AMQPS URI scheme for the AMQP protocol over TLS
  class AMQPS < AMQP
    DEFAULT_PORT = 5671
  end
  @@schemes['AMQPS'] = AMQPS
end

module Qpid::Proton
  private
  # Make sure to allow empty hostnames, Ruby 2.0.0 does not.
  DEFAULT_URI_PARSER = URI::Parser.new(:HOSTNAME => /(?:#{URI::PATTERN::HOSTNAME})|/)

  public

  # Convert +s+ to an amqp: or amqps: URI
  #
  # This does not give the same result as the standard URI parser in all cases.
  # Short-cut strings like "host:port" are allowed, an "amqp://" prefix is added if +s+ does
  # not already look like an 'amqp:' or 'amqps:' URI.
  #
  # @param s [String,URI] String to convert to a URI, or a URI object.
  # @return [URI] A valid AMQP or AMQPS URI
  # @raise [URI::BadURIError] s is a URI object with a non-AMQP scheme
  # @raise [URI::InvalidURIError] s cannot be parsed as a URI or shortcut
  # @raise [::ArgumentError] s is not a string or URI
  #
  def self.uri(s)
    case s
    when URI::AMQP then s       # This is already an AMQP or AMQPS URL.
    when URI::Generic           # Re-parse a generic URI that was not parsed as AMQP/AMQPS class
      s.scheme ||= 'amqp'       # Default to amqp: scheme
      u = DEFAULT_URI_PARSER.parse(s.to_s)
      raise URI::BadURIError, "Not an AMQP URI: '#{u}'" unless u.is_a? URI::AMQP
      u
    else
      s = String.try_convert s
      raise ::ArgumentError, "bad argument (expected URI object or URI string)" unless s
      case s
      when %r{^amqps?:} then DEFAULT_URI_PARSER.parse(s)      # Looks like an AMQP URI
      when %r{^//} then DEFAULT_URI_PARSER.parse("amqp:#{s}") # Looks like an authority with no scheme
      else DEFAULT_URI_PARSER.parse("amqp://#{s}")            # Treat as a bare host:port/path string
      end
    end
  rescue =>e
    raise e.class, "#{self}.#{__method__}(#{s.inspect}): #{e}"
  end
end
