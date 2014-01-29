package org.apache.qpid.proton.amqp.transport;
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


import org.apache.qpid.proton.amqp.Binary;

public interface FrameBody
{
    interface FrameBodyHandler<E>
    {
        void handleOpen(Open open, Binary payload, E context);
        void handleBegin(Begin begin, Binary payload, E context);
        void handleAttach(Attach attach, Binary payload, E context);
        void handleFlow(Flow flow, Binary payload, E context);
        void handleTransfer(Transfer transfer, Binary payload, E context);
        void handleDisposition(Disposition disposition, Binary payload, E context);
        void handleDetach(Detach detach, Binary payload, E context);
        void handleEnd(End end, Binary payload, E context);
        void handleClose(Close close, Binary payload, E context);

    }

    <E> void invoke(FrameBodyHandler<E> handler, Binary payload, E context);
}
