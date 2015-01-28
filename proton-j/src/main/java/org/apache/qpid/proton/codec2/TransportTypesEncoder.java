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
package org.apache.qpid.proton.codec2;

import org.apache.qpid.proton.amqp.transport2.Flow;

public class TransportTypesEncoder
{
    public static void encodeOpen(Encoder encoder, Open open)
    {

    }

    public static void encodeFlow(Encoder encoder, Flow flow)
    {
        encoder.putDescriptor();
        encoder.putUlong(0x0000000000000013L);
        encoder.putList();
        // unsigned int ?
        encoder.putLong(flow.getNextIncomingId());
        encoder.putLong(flow.getIncomingWindow());
        encoder.putLong(flow.getNextOutgoingId());
        encoder.putLong(flow.getOutgoingWindow());
        encoder.putLong(flow.getHandle());
        encoder.putLong(flow.getDeliveryCount());
        encoder.putLong(flow.getLinkCredit());
        encoder.putLong(flow.getAvailable());
        if (flow.getDrain())
        {
            encoder.putBoolean(true);
        }
        if (flow.getEcho())
        {
            encoder.putBoolean(true);
        }
        if (flow.getProperties() != null && flow.getProperties().size() > 0)
        {
            encoder.putMap();
            // ..... handle map
            encoder.end();
        }
        encoder.end();
    }
}
