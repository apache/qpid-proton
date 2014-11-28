/*
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

/**
 * The TestCase class provides a simple dependency-free Unit Test framework to
 * automagically invoke methods that start with "test" on classes that extend it.
 */

// TestCase Constructor
var TestCase = function() {};

// Enumerate all functions of the class and invoke those beginning with "test".
TestCase.prototype.run = function() {
    for (var property in this) {
        if ((typeof this[property] === 'function') &&
            property.lastIndexOf('test', 0) === 0) {
            this.setUp();
            this[property]();
            this.tearDown();
        }
    }
};

TestCase.prototype.setUp = function() {};
TestCase.prototype.tearDown = function() {};

module.exports.TestCase = TestCase;

