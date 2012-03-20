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
import org.apache.tools.ant.TaskContainer;

import java.util.ArrayList;
import java.util.List;

/**
 * Foreach -- an ant task that allows iteration.
 **/

public class Foreach extends BaseTask implements TaskContainer {

    private String property;
    private String list;
    private String delim = "\\s+";
    private String stop;
    private List<Task> tasks = new ArrayList<Task>();

    public void setProperty(String p) {
        property = p;
    }

    public void setList(String l) {
        list = l;
    }

    public void setDelim(String d) {
        delim = d;
    }

    public void setStop(String s) {
        stop = s;
    }

    public void addTask(Task t) {
        tasks.add(t);
    }

    public void execute() {
        validate("property", property).required().nonempty();
        validate("list", property).required();

        if (list.length() == 0) {
            return;
        }

        String[] values = list.split(delim);
        for (int i = 0; i < values.length; i++) {
            String value = values[i];
            if (stop != null && stop.length() > 0 &&
                value.equals(stop)) {
                break;
            }
            getProject().setProperty(property, value);
            for (Task t : tasks) {
                t.perform();
            }
        }
    }

}
