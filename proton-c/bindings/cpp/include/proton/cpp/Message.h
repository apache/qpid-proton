#ifndef PROTON_CPP_MESSAGE_H
#define PROTON_CPP_MESSAGE_H

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
#include "proton/cpp/ProtonHandle.h"
#include "proton/message.h"
#include <string>


namespace proton {
namespace reactor {

class Message : public ProtonHandle<pn_message_t>
{
  public:
    PROTON_CPP_EXTERN Message();
    PROTON_CPP_EXTERN Message(pn_message_t *);
    PROTON_CPP_EXTERN Message(const Message&);
    PROTON_CPP_EXTERN Message& operator=(const Message&);
    PROTON_CPP_EXTERN ~Message();

    PROTON_CPP_EXTERN pn_message_t *getPnMessage() const;

    PROTON_CPP_EXTERN void setId(uint64_t id);
    PROTON_CPP_EXTERN uint64_t getId();
    PROTON_CPP_EXTERN pn_type_t getIdType();

    PROTON_CPP_EXTERN void setBody(const std::string &data);
    PROTON_CPP_EXTERN std::string getBody();

    PROTON_CPP_EXTERN void getBody(std::string &str);

    PROTON_CPP_EXTERN void setBody(const char *, size_t len);
    PROTON_CPP_EXTERN size_t getBody(char *, size_t len);
    PROTON_CPP_EXTERN size_t getBinaryBodySize();


    PROTON_CPP_EXTERN void encode(std::string &data);
    PROTON_CPP_EXTERN void decode(const std::string &data);

  private:
    friend class ProtonImplRef<Message>;
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_MESSAGE_H*/
