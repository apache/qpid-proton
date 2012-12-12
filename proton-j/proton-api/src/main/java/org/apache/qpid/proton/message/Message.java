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
package org.apache.qpid.proton.message;

import org.apache.qpid.proton.amqp.messaging.ApplicationProperties;
import org.apache.qpid.proton.amqp.messaging.DeliveryAnnotations;
import org.apache.qpid.proton.amqp.messaging.Footer;
import org.apache.qpid.proton.amqp.messaging.Header;
import org.apache.qpid.proton.amqp.messaging.MessageAnnotations;
import org.apache.qpid.proton.amqp.messaging.Properties;
import org.apache.qpid.proton.amqp.messaging.Section;

public interface Message
{
    boolean isDurable();

    long getDeliveryCount();

    short getPriority();

    boolean isFirstAcquirer();

    long getTtl();

    void setDurable(boolean durable);

    void setTtl(long ttl);

    void setDeliveryCount(long deliveryCount);

    void setFirstAcquirer(boolean firstAcquirer);

    void setPriority(short priority);

    Object getMessageId();

    long getGroupSequence();

    String getReplyToGroupId();

    long getCreationTime();

    String getAddress();

    byte[] getUserId();

    String getReplyTo();

    String getGroupId();

    String getContentType();

    long getExpiryTime();

    Object getCorrelationId();

    String getContentEncoding();

    String getSubject();

    void setGroupSequence(long groupSequence);

    void setUserId(byte[] userId);

    void setCreationTime(long creationTime);

    void setSubject(String subject);

    void setGroupId(String groupId);

    void setAddress(String to);

    void setExpiryTime(long absoluteExpiryTime);

    void setReplyToGroupId(String replyToGroupId);

    void setContentEncoding(String contentEncoding);

    void setContentType(String contentType);

    void setReplyTo(String replyTo);

    void setCorrelationId(Object correlationId);

    void setMessageId(Object messageId);

    Header getHeader();

    DeliveryAnnotations getDeliveryAnnotations();

    MessageAnnotations getMessageAnnotations();

    Properties getProperties();

    ApplicationProperties getApplicationProperties();

    Section getBody();

    Footer getFooter();

    void setHeader(Header header);

    void setDeliveryAnnotations(DeliveryAnnotations deliveryAnnotations);

    void setMessageAnnotations(MessageAnnotations messageAnnotations);

    void setProperties(Properties properties);

    void setApplicationProperties(ApplicationProperties applicationProperties);

    void setBody(Section body);

    void setFooter(Footer footer);

    int decode(byte[] data, int offset, int length);

    int encode(byte[] data, int offset, int length);

    void load(Object data);

    Object save();

    String toAMQPFormat(Object value);

    Object parseAMQPFormat(String value);

    void setMessageFormat(MessageFormat format);

    MessageFormat getMessageFormat();

    void clear();

    MessageError getError();
}
