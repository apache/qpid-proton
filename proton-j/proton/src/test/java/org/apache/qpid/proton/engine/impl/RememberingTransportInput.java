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
package org.apache.qpid.proton.engine.impl;

import org.apache.qpid.proton.engine.TransportInput;

class RememberingTransportInput implements TransportInput
{
    private StringBuilder _receivedInput = new StringBuilder();
    private boolean _hasAcceptedLimit = false;
    private int _acceptLimit = 0;

    @Override
    public int input(byte[] bytes, int offset, int size)
    {
        final int returnSize;
        if (!_hasAcceptedLimit)
        {
            _receivedInput.append(new String(bytes, offset, size));
            returnSize = size;
        }
        else
        {
            int currentSize = _receivedInput.length();
            int spareCapacity = _acceptLimit - currentSize;
            if (spareCapacity < 0)
            {
                throw new IllegalStateException("Could not write " + size
                        + " bytes into buffer of size " + currentSize
                        + " with accept limit of " + _acceptLimit + ". Test error??");
            }
            int effectiveSize = Math.min(size, spareCapacity);
            _receivedInput.append(new String(bytes, offset, effectiveSize));
            returnSize = effectiveSize;
        }
        return returnSize;
    }

    public void removeAcceptLimit()
    {
        _hasAcceptedLimit = false;
    }

    public void setAcceptLimit(int acceptLimit)
    {
        _hasAcceptedLimit = true;
        _acceptLimit = acceptLimit;
    }

    String getAcceptedInput()
    {
        return _receivedInput.toString();
    }

}