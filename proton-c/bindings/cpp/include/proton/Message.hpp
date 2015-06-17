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
#include "proton/ImportExport.hpp"
#include "proton/ProtonHandle.hpp"
#include "proton/message.h"
#include <string>


namespace proton {
namespace reactor {

class Message : public ProtonHandle<pn_message_t>
{
  public:
    PN_CPP_EXTERN Message();
    PN_CPP_EXTERN Message(pn_message_t *);
    PN_CPP_EXTERN Message(const Message&);
    PN_CPP_EXTERN Message& operator=(const Message&);
    PN_CPP_EXTERN ~Message();

    PN_CPP_EXTERN pn_message_t *getPnMessage() const;

    // FIXME aconway 2015-06-11: get rid of get/set prefixes

    // FIXME aconway 2015-06-11: use Value not string to allow full range of AMQP types.
    PN_CPP_EXTERN void setId(uint64_t id);
    PN_CPP_EXTERN uint64_t getId();
    PN_CPP_EXTERN void setId(const std::string &id);
    PN_CPP_EXTERN std::string getStringId();
    PN_CPP_EXTERN void setId(const char *p, size_t len);
    PN_CPP_EXTERN size_t getId(const char **p);
    PN_CPP_EXTERN pn_type_t getIdType();

    PN_CPP_EXTERN void setUserId(const std::string &id);
    PN_CPP_EXTERN std::string getUserId();

    PN_CPP_EXTERN void setAddress(const std::string &addr);
    PN_CPP_EXTERN std::string getAddress();

    PN_CPP_EXTERN void setSubject(const std::string &s);
    PN_CPP_EXTERN std::string getSubject();

    PN_CPP_EXTERN void setReplyTo(const std::string &s);
    PN_CPP_EXTERN std::string getReplyTo();

    PN_CPP_EXTERN void setCorrelationId(uint64_t id);
    PN_CPP_EXTERN uint64_t getCorrelationId();
    PN_CPP_EXTERN void setCorrelationId(const std::string &id);
    PN_CPP_EXTERN std::string getStringCorrelationId();
    PN_CPP_EXTERN void setCorrelationId(const char *p, size_t len);
    PN_CPP_EXTERN size_t getCorrelationId(const char **p);

    // FIXME aconway 2015-06-11: use Value not string to allow full range of AMQP types.
    PN_CPP_EXTERN pn_type_t getCorrelationIdType();

    PN_CPP_EXTERN void setContentType(const std::string &s);
    PN_CPP_EXTERN std::string getContentType();

    PN_CPP_EXTERN void setContentEncoding(const std::string &s);
    PN_CPP_EXTERN std::string getContentEncoding();

    PN_CPP_EXTERN void setExpiry(pn_timestamp_t t);
    PN_CPP_EXTERN pn_timestamp_t getExpiry();

    PN_CPP_EXTERN void setCreationTime(pn_timestamp_t t);
    PN_CPP_EXTERN pn_timestamp_t getCreationTime();

    PN_CPP_EXTERN void setGroupId(const std::string &s);
    PN_CPP_EXTERN std::string getGroupId();

    PN_CPP_EXTERN void setReplyToGroupId(const std::string &s);
    PN_CPP_EXTERN std::string getReplyToGroupId();

    // FIXME aconway 2015-06-11: use Values for body.
    PN_CPP_EXTERN void setBody(const std::string &data);
    PN_CPP_EXTERN std::string getBody();

    PN_CPP_EXTERN void getBody(std::string &str);

    PN_CPP_EXTERN void setBody(const char *, size_t len);
    PN_CPP_EXTERN size_t getBody(char *, size_t len);
    PN_CPP_EXTERN size_t getBinaryBodySize();


    PN_CPP_EXTERN void encode(std::string &data);
    PN_CPP_EXTERN void decode(const std::string &data);

  private:
    friend class ProtonImplRef<Message>;
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_MESSAGE_H*/
