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
package org.apache.qpid.tasks;

import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.Task;

import java.util.HashSet;
import java.util.Set;

/**
 * BaseTask -- an abstract base task for blaze specific tasks.
 **/

public abstract class BaseTask extends Task {

    private static Set EMPTY = new HashSet();
    {
        EMPTY.add(0);
        EMPTY.add("");
    }

    public static class Validator {

        private String name;
        private Object value;

        private  Validator(String name, Object value) {
            this.name = name;
            this.value = value;
        }

        public Validator required() {
            if (value == null) {
                error("value is required");
            }
            return this;
        }

        public Validator nonempty() {
            if (EMPTY.contains(value)) {
                error("value is empty");
            }
            return this;
        }

        private void error(String msg) {
            throw new BuildException(name + ": " + msg);
        }
    }

    public Validator validate(String name, Object value) {
        return new Validator(name, value);
    }

}
