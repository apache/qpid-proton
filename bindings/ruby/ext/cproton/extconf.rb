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

require 'mkmf'

# set the ruby version compiler flag
runtime_version = RUBY_VERSION.gsub(/\./,'')[0,2]
$CFLAGS << " -DRUBY#{runtime_version}"

dir_config("qpid-proton")

REQUIRED_LIBRARIES = [
                      "qpid-proton",
                     ]

REQUIRED_LIBRARIES.each do |library|
  abort "Missing library: #{library}" unless have_library library
end

REQUIRED_HEADERS = [
                    "proton/engine.h",
                    "proton/message.h",
                    "proton/sasl.h",
                    "proton/messenger.h"
                   ]

REQUIRED_HEADERS.each do |header|
  abort "Missing header: #{header}" unless have_header header
end

create_makefile('cproton')

