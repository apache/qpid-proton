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

##------------------------------------------------------
## Use Swig to generate a PHP binding to the Proton API
##------------------------------------------------------

# Uses the php-config command line tool from PHP to extract the location of the PHP header
# files

EXECUTE_PROCESS(COMMAND ${PHP_CONFIG_EXE} --includes
                OUTPUT_VARIABLE PHP_INCLUDES
                RESULT_VARIABLE retval
                ERROR_VARIABLE  errmsg
                OUTPUT_STRIP_TRAILING_WHITESPACE)
IF (NOT ${retval} EQUAL 0)
   MESSAGE(SEND_ERROR "Command \"${PHP_CONFIG_EXE} --include\" failed with output:\n${errmsg}")
ELSE()
   #MESSAGE(STATUS "PHP_INCLUDES=[${PHP_INCLUDES}]")
   set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/php.i PROPERTIES SWIG_FLAGS "-I${PROJECT_SOURCE_DIR}/include")
   swig_add_module(cproton php ${CMAKE_CURRENT_SOURCE_DIR}/php.i)
   set_source_files_properties(${swig_generated_file_fullname} PROPERTIES COMPILE_FLAGS "${PHP_INCLUDES}")
   swig_link_libraries(cproton qpidproton)
ENDIF()
