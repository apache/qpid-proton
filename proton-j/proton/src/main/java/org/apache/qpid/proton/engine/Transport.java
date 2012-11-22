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
package org.apache.qpid.proton.engine;


/**
 * Transport
 *
 */

public interface Transport extends Endpoint
{

    public int END_OF_STREAM = -1;

    public void bind(Connection connection);

    /**
     * @param bytes input bytes for consumption
     * @param offset the offset within bytes where input begins
     * @param size the number of bytes available for input
     *
     * @return the number of bytes consumed
     */
    public int input(byte[] bytes, int offset, int size);

    /**
     * @param bytes array for output bytes
     * @param offset the offset within bytes where output begins
     * @param size the number of bytes available for output
     *
     * @return the number of bytes written
     */
    public int output(byte[] bytes, int offset, int size);


    Sasl sasl();

}
