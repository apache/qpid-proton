#ifndef PROTON_CPP_EXCEPTIONS_H
#define PROTON_CPP_EXCEPTIONS_H

/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */
#include "proton/cpp/ImportExport.h"
#include <string>
#include <exception>

namespace proton {
namespace reactor {

class ProtonException : public std::exception
{
  public:
    PROTON_CPP_EXTERN explicit ProtonException(const std::string& message=std::string()) throw();
    PROTON_CPP_EXTERN virtual ~ProtonException() throw();
    PROTON_CPP_EXTERN virtual const char* what() const throw();

  private:
    const std::string message;
};

class MessageReject : public ProtonException
{
  public:
    PROTON_CPP_EXTERN explicit MessageReject(const std::string& message=std::string()) throw();
};

class MessageRelease : public ProtonException
{
  public:
    PROTON_CPP_EXTERN explicit MessageRelease(const std::string& message=std::string()) throw();
};

}} // namespace proton::reactor

#endif  /*!PROTON_CPP_EXCEPTIONS_H*/
