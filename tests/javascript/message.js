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
 * This is a fairly literal JavaScript port of message.py used to unit test the
 * proton.Message wrapper class.
 */

// Check if the environment is Node.js and if not log an error and exit.
if (!exports) {
    console.error("message.js should be run in Node.js");
    return;
}

var unittest = require("./unittest.js");
var assert = require("assert");
var proton = require("qpid-proton");

/**
 * JavaScript Implementation of Python's range() function taken from:
 * http://stackoverflow.com/questions/8273047/javascript-function-similar-to-python-range
 */
var range = function(start, stop, step) {
    if (typeof stop == 'undefined') {
        // one param defined
        stop = start;
        start = 0;
    };
    if (typeof step == 'undefined') {
        step = 1;
    };
    if ((step > 0 && start >= stop) || (step < 0 && start <= stop)) {
        return [];
    };
    var result = [];
    for (var i = start; step > 0 ? i < stop : i > stop; i += step) {
        result.push(i);
    };
    return result;
};

/**
 * Quick and dirty Object comparison using valueOf(). We need a comparison method.
 * as some of the types we might want to compare are Object types (proton.Data.Uuid,
 * proton.Data.Binary etc.). The approach here is absolutely *not* a robust deep
 * Object comparison method, but it is relatively simple & good enough for the test cases.
 */
var compare = function(x, y) {
    if (x === null) {
        assert(y === x);
    } else {
        assert(y.valueOf() === x.valueOf());
    }
};


// Extend TestCase by creating a new Test class as we're using it in a few places.
var Test = function() { // Test constructor.
    /**
     * The following call is the equivalent of "super" in Java. It's not necessary
     * here as the unittest.TestCase constructor doesn't set any state, but it
     * is the correct thing to do when implementing classical inheritance in
     * JavaScript so I've kept it here as a useful reminder of the "pattern".
     */
    //unittest.TestCase.prototype.constructor.call(this);
};

Test.prototype = new unittest.TestCase(); // Here's where the inheritance occurs.
Test.prototype.constructor = Test; // Otherwise instances of Test would have a constructor of unittest.TestCase.

Test.prototype.setUp = function() {
    this.msg = new proton.Message();
};

Test.prototype.tearDown = function() {
    this.msg.free();
    this.msg = null;
};


// Extend Test more simply by creating a prototype instance and adding test methods as properties.

var AccessorsTest = new Test();

AccessorsTest._test = function(name, defaultValue, values) {
    // First get the values passed to the method.
    var values = Array.prototype.slice.apply(arguments, [2]);
    // If the first element of values is actually an Array then use the Array.
    // This scenario is what happens in the tests that use the range() function.
    values = (Object.prototype.toString.call(values[0]) === '[object Array]') ? values[0] : values;

    // Work out the accessor/mutator names noting that boolean accessors use "is" not "get".
    var setter = 'set' + name;
    var getter = (typeof defaultValue === 'boolean') ? 'is' + name : 'get' + name;

    // Get the message's default value first.
    var d = this.msg[getter]();

    // Compare the message's default with the expected default value passed in the test case.
    compare(d, defaultValue);

    for (var i = 0; i < values.length; i++) {
        var v = values[i];
        /**
         * For the case of Binary values under test we retrieve their valueOf().
         * This is because the methods under test "consume" the data, in other
         * words ownership of the underlying raw data transfers to the Message
         * object so the v object becomes "empty" after calling the setter.
         */
        var value = (v instanceof proton.Data.Binary) ? v.valueOf() : v;

        this.msg[setter](v); // This call will "consume" Binary data.
        var gotten = this.msg[getter]();
        compare(value, gotten);
    }
};

AccessorsTest._testString = function(name) {
    this._test(name, "", "asdf", "fdsa", "");
};

AccessorsTest._testTime = function(name) {
    // The ExpiryTime and CreationTime methods can take either a number or a Date Object.
    this._test(name, new Date(0), new Date(0), 123456789, new Date(987654321));
};

AccessorsTest.testID = function() {
    console.log("testID");
    this._test("ID", null, "bytes", null, 123, "string", new proton.Data.Uuid(), new proton.Data.Binary("ВИНАРЫ"));
    console.log("OK\n");
};

AccessorsTest.testCorrelationID = function() {
    console.log("testCorrelationID");
    this._test("CorrelationID", null, "bytes", null, 123, "string", new proton.Data.Uuid(), new proton.Data.Binary("ВИНАРЫ"));
    console.log("OK\n");
};

AccessorsTest.testDurable = function() {
    console.log("testDurable");
    this._test("Durable", false, true, false);
    console.log("OK\n");
};

AccessorsTest.testPriority = function() {
    console.log("testPriority");
    this._test("Priority", proton.Message.DEFAULT_PRIORITY, range(0, 256));
    console.log("OK\n");
};

AccessorsTest.testTTL = function() {
    console.log("testTTL");
    this._test("TTL", 0, range(12345, 54321));
    console.log("OK\n");
};

AccessorsTest.testFirstAcquirer = function() {
    console.log("testFirstAcquirer");
    this._test("FirstAcquirer", false, true, false);
    console.log("OK\n");
};

AccessorsTest.testDeliveryCount = function() {
    console.log("testDeliveryCount");
    this._test("DeliveryCount", 0, range(0, 1024));
    console.log("OK\n");
};

AccessorsTest.testUserID = function() {
    console.log("testUserID");
    this._test("UserID", "", "asdf", "fdsa", new proton.Data.Binary("ВИНАРЫ"), "", new proton.Data.Uuid(), "");
    console.log("OK\n");
};

AccessorsTest.testAddress = function() {
    console.log("testAddress");
    this._testString("Address");
    console.log("OK\n");
};

AccessorsTest.testSubject = function() {
    console.log("testSubject");
    this._testString("Subject");
    console.log("OK\n");
};

AccessorsTest.testReplyTo = function() {
    console.log("testReplyTo");
    this._testString("ReplyTo");
    console.log("OK\n");
};

AccessorsTest.testContentType = function() {
    console.log("testContentType");
    this._testString("ContentType");
    console.log("OK\n");
};

AccessorsTest.testContentEncoding = function() {
    console.log("testContentEncoding");
    this._testString("ContentEncoding");
    console.log("OK\n");
};

AccessorsTest.testExpiryTime = function() {
    console.log("testExpiryTime");
    this._testTime("ExpiryTime");
    console.log("OK\n");
};

AccessorsTest.testCreationTime = function() {
    console.log("testCreationTime");
    this._testTime("CreationTime");
    console.log("OK\n");
};

AccessorsTest.testGroupID = function() {
    console.log("testGroupID");
    this._testString("GroupID");
    console.log("OK\n");
};

AccessorsTest.testGroupSequence = function() {
    console.log("testGroupSequence");
    this._test("GroupSequence", 0, 0, -10, 10, 20, -20);
    console.log("OK\n");
};

AccessorsTest.testReplyToGroupID = function() {
    console.log("testReplyToGroupID");
    this._testString("ReplyToGroupID");
    console.log("OK\n");
};


var CodecTest = new Test();

CodecTest.testRoundTrip = function() {
    console.log("testRoundTrip");
    this.msg.setID("asdf");
    this.msg.setCorrelationID(new proton.Data.Uuid());
    this.msg.setTTL(3);
    this.msg.setPriority(100);
    this.msg.setAddress("address");
    this.msg.setSubject("subject");
    this.msg.body = "Hello World!";

    var data = this.msg.encode();
    var msg2 = new proton.Message();
    msg2.decode(data);

    assert(this.msg.getID() === msg2.getID());
    assert(this.msg.getCorrelationID().toString() === msg2.getCorrelationID().toString());
    assert(this.msg.getTTL() === msg2.getTTL());
    assert(this.msg.getPriority() === msg2.getPriority());
    assert(this.msg.getAddress() === msg2.getAddress());
    assert(this.msg.getSubject() === msg2.getSubject());
    assert(this.msg.body === msg2.body);

    msg2.free();
    console.log("OK\n");
};

/*
// load and save are deprecated, might to an equivalent using encode/decode
var LoadSaveTest = new Test();


LoadSaveTest.testIntegral = function() {
    console.log("testIntegral");

    console.log("OK\n");
};

LoadSaveTest.testFloating = function() {
    console.log("testFloating");

    console.log("OK\n");
};
*/


AccessorsTest.run();
CodecTest.run();
//LoadSaveTest.run();


