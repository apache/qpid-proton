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
package org.apache.qpid.proton.driver;

import org.apache.qpid.proton.engine.EndpointError;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Transport;

import java.util.Iterator;
import java.util.Map;

public class DelegatingTransport implements BytesTransport
{
    private Transport _delegate;
    
    private final Transport _default;
    private final Map<byte[], Transport> _potentialDelegates;
    private State _state = State.CHOOSING;
    private int _bytesRead;

    enum State
    {
        DELEGATING,
        CHOOSING
    }
    
    public DelegatingTransport(final Map<byte[], Transport> potentialDelegates, final Transport defaultDelegate)
    {
        _potentialDelegates = potentialDelegates;
        _default = defaultDelegate;
    }

    
    public int input(final byte[] bytes, final int offset, final int size)
    {
        switch (_state)
        {
            case DELEGATING:
                return _delegate.input(bytes, offset, size);
            default:
                Iterator<Map.Entry<byte[], Transport>> mapEntriesIter = _potentialDelegates.entrySet().iterator();
                while (mapEntriesIter.hasNext())
                {
                    Map.Entry<byte[], Transport> entry = mapEntriesIter.next();
                    int headerOffset = _bytesRead;
                    int count = Math.min(size, entry.getKey().length-headerOffset);
                    for(int i = 0; i<count; i++)
                    {
                        if(entry.getKey()[headerOffset+i] != bytes[offset+i])
                        {
                            mapEntriesIter.remove();
                            break;
                        }
                    }
                }
                if(_potentialDelegates.size() == 0)
                {
                    _delegate = _default;
                    _state = State.DELEGATING;
                    return _delegate.input(bytes,offset,size);
                }
                else if(_potentialDelegates.size() == 1 && _bytesRead+size >= _potentialDelegates.keySet().iterator()
                        .next().length)
                {
                    _delegate = _potentialDelegates.values().iterator().next();
                    _state = State.DELEGATING;
                    return _delegate.input(bytes,offset,size);

                }
        }
        _bytesRead+=size;
        return size;
        
    }

    public int output(final byte[] bytes, final int offset, final int size)
    {
        if(_delegate == null)
        {
            return 0;
        }
        return _delegate.output(bytes,offset,size);
    }

    public EndpointState getLocalState()
    {
        return null;  //TODO.
    }

    public EndpointState getRemoteState()
    {
        return null;  //TODO.
    }

    public EndpointError getLocalError()
    {
        return null;  //TODO.
    }

    public EndpointError getRemoteError()
    {
        return null;  //TODO.
    }

    public void free()
    {
        //TODO.
    }

}
