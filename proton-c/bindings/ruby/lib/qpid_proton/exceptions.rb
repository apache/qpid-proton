#
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
#

module Qpid

  module Proton

    module Error

      NONE = 0
      EOS = Cproton::PN_EOS
      ERROR = Cproton::PN_ERR
      OVERFLOW = Cproton::PN_OVERFLOW
      UNDERFLOW = Cproton::PN_UNDERFLOW
      STATE = Cproton::PN_STATE_ERR
      ARGUMENT = Cproton::PN_ARG_ERR
      TIMEOUT = Cproton::PN_TIMEOUT
      INTERRUPTED = Cproton::PN_INTR
      INPROGRESS = Cproton::PN_INPROGRESS

    end

    # Represents a generic error at the messaging level.
    #
    class ProtonError < RuntimeError
    end

    # Represents an end-of-stream error while messaging.
    #
    class EOSError < ProtonError
    end

    # Represents a data overflow exception while messaging.
    #
    class OverflowError < ProtonError
    end

    # Represents a data underflow exception while messaging.
    #
    class UnderflowError < ProtonError
    end

    # Represents an invalid, missing or illegal argument while messaging.
    #
    class ArgumentError < ProtonError
    end

    # Represents a timeout during messaging.
    #
    class TimeoutError < ProtonError
    end

    # Represents an interrupting during a blocking I/O operation.
    #
    class InterruptedError < ProtonError
    end

    class InProgressError < ProtonError
    end

  end

end
