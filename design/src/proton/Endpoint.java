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
package proton;


/**
 * Endpoint
 *
 * @opt operations
 * @opt types
 *
 * @assoc - local 1 Endpoint.State
 * @assoc - remote 1 Endpoint.State
 * @assoc - local 0..1 Endpoint.Error
 * @assoc - remote 0..1 Endpoint.Error
 */

public interface Endpoint
{

    /**
     * Represents the state of a communication endpoint.
     */
    public static final class State {

        private String name;

        private State(String name)
        {
            this.name = name;
        }

        public String toString()
        {
            return name;
        }

    };

    public static final State UNINIT = new State("UNINIT");
    public static final State ACTIVE = new State("ACTIVE");
    public static final State CLOSED = new State("CLOSED");

    /**
     * Holds information about an endpoint error.
     */
    public static final class Error {

        private String name;
        private String description;

        public Error(String name, String description)
        {
            this.name = name;
            this.description = description;
        }

        public Error(String name)
        {
            this(name, null);
        }

        public String toString()
        {
            if (description == null)
            {
                return name;
            }
            else
            {
                return String.format("%s -- %s", name, description);
            }
        }
    }

    /**
     * @return the local endpoint state
     */
    public State getLocalState();

    /**
     * @return the remote endpoint state (as last communicated)
     */
    public State getRemoteState();

    /**
     * @return the local endpoint error, or null if there is none
     */
    public Error getLocalError();

    /**
     * @return the remote endpoint error, or null if there is none
     */
    public Error getRemoteError();

    /**
     * free the endpoint and any associated resources
     */
    public void free();

}
