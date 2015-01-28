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


/**
 * DataHandler
 *
 */

public interface DataHandler
{

    void onNull(Decoder decoder);

    void onBoolean(Decoder decoder);

    void onByte(Decoder decoder);

    void onShort(Decoder decoder);

    void onInt(Decoder decoder);

    void onLong(Decoder decoder);

    void onUbyte(Decoder decoder);

    void onUshort(Decoder decoder);

    void onUint(Decoder decoder);

    void onUlong(Decoder decoder);

    void onFloat(Decoder decoder);

    void onDouble(Decoder decoder);

    void onDecimal32(Decoder decoder);

    void onDecimal64(Decoder decoder);

    void onDecimal128(Decoder decoder);

    void onChar(Decoder decoder);

    void onTimestamp(Decoder decoder);

    void onUUID(Decoder decoder);

    void onBinary(Decoder decoder);

    void onString(Decoder decoder);

    void onSymbol(Decoder decoder);

    void onList(Decoder decoder);
    void onListEnd(Decoder decoder);

    void onMap(Decoder decoder);
    void onMapEnd(Decoder decoder);

    void onArray(Decoder decoder);
    void onArrayEnd(Decoder decoder);

    void onDescriptor(Decoder decoder);

}
