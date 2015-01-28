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

public class TransportTypesDecoder
{
    public static Open decodeOpen(Decoder decoder)
    {

    }

    public static Flow decodeFlow(Decoder decoder)
    {
        final Flow flow = new Flow();
        DataHandler dh = new AbstractDataHandler()
        {

            int index = 0;

            public void onLong(org.apache.qpid.proton.codec2.Decoder d)
            {
                switch (index)
                {
                case 0:
                    flow.setNextIncomingId(d.getLongBits());
                    index++;
                    break;
                case 1:
                    flow.setIncomingWindow(d.getLongBits());
                    index++;
                    break;
                case 2:
                    flow.setNextOutgoingId(d.getLongBits());
                    index++;
                    break;
                case 3:
                    flow.setOutgoingWindow(d.getLongBits());
                    index++;
                    break;
                case 4:
                    flow.setHandle(d.getLongBits());
                    index++;
                    break;
                case 5:
                    flow.setDeliveryCount(d.getLongBits());
                    index++;
                    break;
                case 6:
                    flow.setLinkCredit(d.getLongBits());
                    index++;
                    break;
                case 7:
                    flow.setAvailable(d.getLongBits());
                    index++;
                    break;
                }
            }

            public void onBoolean(org.apache.qpid.proton.codec2.Decoder d)
            {
                switch (index)
                {
                case 8:
                    flow.setDrain(); // what should be used here?
                    index++;
                    break;
                case 9:
                    flow.setEcho(); // what should be used here?
                    index++;
                    break;
                }
            }

            public void onMap(org.apache.qpid.proton.codec2.Decoder d)
            {
                flow.setProperties(decodeMap(d)); //
            }
        };

        return flow;
    }
}
