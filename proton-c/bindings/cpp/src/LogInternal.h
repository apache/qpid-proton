#ifndef PROTON_CPP_LOG_INTERNAL_H
#define PROTON_CPP_LOG_INTERNAL_H

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

#include "Msg.h"

namespace proton {
namespace reactor {

enum Level { trace, debug, info, notice, warning, error, critical };

class Logger
{
public:
    // TODO: build out to be ultra configurable as for corresponding QPID class + Statement
    static void log(Level level, const char* file, int line, const char* function, const std::string& message);
private:
    //This class has only one instance so no need to copy
    Logger();
    ~Logger();

    Logger(const Logger&);
    Logger operator=(const Logger&);
};

// Just do simple logging for now
#define PN_CPP_LOG(LEVEL, MESSAGE) Logger::log(LEVEL, 0, 0, 0, ::proton::reactor::Msg() << MESSAGE)

}} // namespace proton::reactor

#endif  /*!PROTON_CPP_LOG_INTERNAL_H*/
