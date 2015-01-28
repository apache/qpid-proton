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
 * AbstractDataHandler
 *
 */

public abstract class AbstractDataHandler implements DataHandler
{

    public void onNull(Decoder decoder) {}

    public void onBoolean(Decoder decoder) {}

    public void onByte(Decoder decoder) {}

    public void onShort(Decoder decoder) {}

    public void onInt(Decoder decoder) {}

    public void onLong(Decoder decoder) {}

    public void onUbyte(Decoder decoder) {}

    public void onUshort(Decoder decoder) {}

    public void onUint(Decoder decoder) {}

    public void onUlong(Decoder decoder) {}

    public void onFloat(Decoder decoder) {}

    public void onDouble(Decoder decoder) {}

    public void onDecimal32(Decoder decoder) {}

    public void onDecimal64(Decoder decoder) {}

    public void onDecimal128(Decoder decoder) {}

    public void onChar(Decoder decoder) {}

    public void onTimestamp(Decoder decoder) {}

    public void onUUID(Decoder decoder) {}

    public void onBinary(Decoder decoder) {}

    public void onString(Decoder decoder) {}

    public void onSymbol(Decoder decoder) {}

    public void onList(Decoder decoder) {}
    public void onListEnd(Decoder decoder) {}

    public void onMap(Decoder decoder) {}
    public void onMapEnd(Decoder decoder) {}

    public void onArray(Decoder decoder) {}
    public void onArrayEnd(Decoder decoder) {}

    public void onDescriptor(Decoder decoder) {}

}
