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
import org.apache.qpid.proton.engine.TransportOutput;
import org.apache.qpid.proton.engine.TransportWrapper;

public class PlainTransportWrapper implements TransportWrapper
{
    private final TransportOutput _outputProcessor;
    private final TransportInput _inputProcessor;

    public PlainTransportWrapper(TransportOutput outputProcessor,
            TransportInput inputProcessor)
    {
        _outputProcessor = outputProcessor;
        _inputProcessor = inputProcessor;
    }

    @Override
    public int output(byte[] bytes, int offset, int size)
    {
        return _outputProcessor.output(bytes, offset, size);
    }

    @Override
    public int input(byte[] bytes, int offset, int size)
    {
        return _inputProcessor.input(bytes, offset, size);
    }
}