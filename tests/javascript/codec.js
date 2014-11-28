#!/usr/bin/env node
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
 * This is a fairly literal JavaScript port of codec.py used to unit test the
 * proton.Data wrapper class. This suite of tests is actually testing the low
 * level implementation methods used to access the AMQP type system and in
 * practice most normal users wouldn't need to call these directly, rather users
 * should simply use the putObject() and getObject() methods.
 */

// Check if the environment is Node.js and if not log an error and exit.
if (typeof process === 'object' && typeof require === 'function') {
    var unittest = require("./unittest.js");
    var assert = require("assert");

    // Increase the virtual heap available to the emscripten compiled C runtime.
    // This allows us to test a really big string.
    PROTON_TOTAL_MEMORY = 140000000;
    PROTON_TOTAL_STACK = 25000000; // Needs to be bigger than the biggest string.
    var proton = require("qpid-proton");

    // Extend TestCase by creating a prototype instance and adding test methods as properties.
    var DataTest = new unittest.TestCase();

    DataTest.setUp = function() {
        this.data = new proton.Data();
    };

    DataTest.tearDown = function() {
        this.data.free();
        this.data = null;
    };
    
    DataTest.testTopLevelNext = function() {
        console.log("testTopLevelNext");
        assert(this.data.next() === null);
        this.data.putNULL();
        this.data.putBOOL(false);
        this.data.putINT(0);
        assert(this.data.next() === null);
        this.data.rewind();
        assert(this.data.next() === proton.Data.NULL);
        assert(this.data.next() === proton.Data.BOOL);
        assert(this.data.next() === proton.Data.INT);
        assert(this.data.next() === null);
        console.log("OK\n");
    };
    
    DataTest.testNestedNext = function() {
        console.log("testNestedNext");
        assert(this.data.next() === null);
        this.data.putNULL();
        assert(this.data.next() === null);
        this.data.putLISTNODE();
        assert(this.data.next() === null);
        this.data.putBOOL(false);
        assert(this.data.next() === null);
        this.data.rewind();
        assert(this.data.next() === proton.Data.NULL);
        assert(this.data.next() === proton.Data.LIST);
        this.data.enter();
        assert(this.data.next() === null);
        this.data.putUBYTE(0);
        assert(this.data.next() === null);
        this.data.putUINT(0);
        assert(this.data.next() === null);
        this.data.putINT(0);
        assert(this.data.next() === null);
        this.data.exit();
        assert(this.data.next() === proton.Data.BOOL);
        assert(this.data.next() === null);
    
        this.data.rewind();
        assert(this.data.next() === proton.Data.NULL);
        assert(this.data.next() === proton.Data.LIST);
        assert(this.data.enter());
        assert(this.data.next() === proton.Data.UBYTE);
        assert(this.data.next() === proton.Data.UINT);
        assert(this.data.next() === proton.Data.INT);
        assert(this.data.next() === null);
        assert(this.data.exit());
        assert(this.data.next() === proton.Data.BOOL);
        assert(this.data.next() === null);
        console.log("OK\n");
    };
    
    DataTest.testEnterExit = function() {
        console.log("testEnterExit");
        assert(this.data.next() === null);
        assert(!this.data.enter());
        this.data.putLISTNODE();
        assert(this.data.enter());
        assert(this.data.next() === null);
        this.data.putLISTNODE();
        assert(this.data.enter());
        this.data.putLISTNODE();
        assert(this.data.enter());
        assert(this.data.exit());
        assert(this.data.getLISTNODE() === 0);
        assert(this.data.exit());
        assert(this.data.getLISTNODE() === 1);
        assert(this.data.exit());
        assert(this.data.getLISTNODE() === 1);
        assert(!this.data.exit());
        assert(this.data.getLISTNODE() === 1);
        assert(this.data.next() === null);
    
        this.data.rewind();
        assert(this.data.next() === proton.Data.LIST);
        assert(this.data.getLISTNODE() === 1);
        assert(this.data.enter());
        assert(this.data.next() === proton.Data.LIST);
        assert(this.data.getLISTNODE() === 1);
        assert(this.data.enter());
        assert(this.data.next() === proton.Data.LIST);
        assert(this.data.getLISTNODE() === 0);
        assert(this.data.enter());
        assert(this.data.next() === null);
        assert(this.data.exit());
        assert(this.data.getLISTNODE() === 0);
        assert(this.data.exit());
        assert(this.data.getLISTNODE() === 1);
        assert(this.data.exit());
        assert(this.data.getLISTNODE() === 1);
        assert(!this.data.exit());
        console.log("OK\n");
    };
    
    /**
     * This tests the "low level" putARRAYNODE/getARRAYNODE methods.
     * In general though applications would create a proton.Data.Array and use the
     * higher level putARRAY/getARRAY
     */
    DataTest._testArray = function(dtype, descriptor, atype, values) {
        var values = Array.prototype.slice.apply(arguments, [3]);
        dtype = (dtype == null) ? null : dtype.toUpperCase();
        atype = atype.toUpperCase();
    
        // Create an array node, enter it and put the descriptor (if present) and values.
        this.data.putARRAYNODE(dtype != null, proton.Data[atype]);
        this.data.enter();
        if (dtype != null) {
            var putter = 'put' + dtype;
            this.data[putter](descriptor);
        }
        var putter = 'put' + atype;
        for (var i = 0; i < values.length; i++) {
            this.data[putter](values[i]);
        }
        this.data.exit();
    
        // Check that we did indeed add an Array node
        this.data.rewind();
        assert(this.data.next() === proton.Data.ARRAY);
    
        // Get the count, described and type metadata from the array node and compare
        // with the values we passed to putARRAYNODE.
        var metadata = this.data.getARRAYNODE();
        var count = metadata.count;
        var described = metadata.described;
        var type = metadata.type;
    
        assert(count === values.length);
        if (dtype == null) {
            assert(described === false);
        } else {
            assert(described === true);
        }
        assert(type === proton.Data[atype]);
    
        // Enter the array node and compare the descriptor and values with those that
        // we put into the array.
        assert(this.data.enter());
        if (described) {
            assert(this.data.next() === proton.Data[dtype]);
            var getter = 'get' + dtype;
            var gotten = this.data[getter]();
            assert(gotten.toString() === descriptor.toString());
        }
        var getter = 'get' + atype;
        for (var i = 0; i < values.length; i++) {
            assert(this.data.next() === proton.Data[atype]);
            var gotten = this.data[getter]();
            assert(gotten.toString() === values[i].toString());
        }
        assert(this.data.next() === null);
        assert(this.data.exit());
    };
    
    DataTest.testStringArray = function() {
        console.log("testStringArray");
        this._testArray(null, null, "string", "one", "two", "three");
    
        // Now try using the proton.Data.Array class.
        this.data.clear();
        var put = new proton.Data.Array("STRING", ["four", "five", "six"]);
        this.data.putARRAY(put);
        var get = this.data.getARRAY();
        assert(get.equals(put));
        console.log("OK\n");
    };
    
    DataTest.testDescribedStringArray = function() {
        console.log("testDescribedStringArray");
        this._testArray("symbol", "url", "string", "one", "two", "three");
    
        // Now try using the proton.Data.Array class.
        this.data.clear();
        var put = new proton.Data.Array("STRING", ["four", "five", "six"], new proton.Data.Symbol("url"));
        this.data.putARRAY(put);
        var get = this.data.getARRAY();
        assert(get.equals(put));
        console.log("OK\n");
    };
    
    DataTest.testIntArray = function() {
        console.log("testIntArray");
        this._testArray(null, null, "int", 1, 2, 3);
    
        // Now try using the proton.Data.Array class.
        this.data.clear();
        var put = new proton.Data.Array("INT", [4, 5, 6]);
        this.data.putARRAY(put);
        var get = this.data.getARRAY();
        assert(get.equals(put));
        console.log("OK\n");
    };
    
    DataTest.testUUIDArray = function() {
        console.log("testUUIDArray");
        this._testArray(null, null, "uuid", new proton.Data.Uuid(), new proton.Data.Uuid(), new proton.Data.Uuid());
    
        // Now try using the proton.Data.Array class.
        this.data.clear();
        var put = new proton.Data.Array("UUID", [new proton.Data.Uuid(), new proton.Data.Uuid(), new proton.Data.Uuid()]);
        this.data.putARRAY(put);
        var get = this.data.getARRAY();
        assert(get.equals(put));
        console.log("OK\n");
    };

    DataTest.testEmptyArray = function() {
        console.log("testEmptyArray");
        this._testArray(null, null, "null");

        // Now try using the proton.Data.Array class.
        this.data.clear();
        var put = new proton.Data.Array();
        this.data.putARRAY(put);
        var get = this.data.getARRAY();
        assert(get.equals(put));
        console.log("OK\n");
    };

    DataTest.testDescribedEmptyArray = function() {
        console.log("testDescribedEmptyArray");
        this._testArray("long", 0, "null");
    
        // Now try using the proton.Data.Array class.
        this.data.clear();
        var put = new proton.Data.Array((0).long());
        this.data.putARRAY(put);
        var get = this.data.getARRAY();
        assert(get.equals(put));
        console.log("OK\n");
    };  

    DataTest._test = function(dtype, values) {
        var values = Array.prototype.slice.apply(arguments, [1]);
        var lastValue = values[values.length - 1];

        // Default equality function. Note that we use valueOf because some of the
        // types we are trying to compare (Symbol, Timestamp, Uuid etc.) are object
        // types and we want to compare their value not whether they are the same object.
        var eq = function(x, y) {return x.valueOf() === y.valueOf();};
    
        if (typeof lastValue === 'function') {
            eq = values.pop();
        }
    
        dtype = dtype.toUpperCase();
        var ntype = proton.Data[dtype];
        var putter = 'put' + dtype;
        var getter = 'get' + dtype;
    
        for (var i = 0; i < values.length; i++) {
            var v = values[i];
            /*
             * Replace the array element with its value. We do this to make testing
             * simpler for Binary elements. In the case of Binary putBINARY "consumes"
             * the data, in other words ownership of the underlying raw data transfers
             * to the Data object so the v object becomes "empty" after calling the
             * putter. Calling its valueOf() happens to call its toString() which
             * provides a stringified version of the Binary whilst also working for
             * the other data types we want to test too.
             */
            values[i] = v.valueOf();
            this.data[putter](v);
            var gotten = this.data[getter]();
            assert(eq(gotten, values[i]));
        }
    
        this.data.rewind();
    
        for (var i = 0; i < values.length; i++) {
            var v = values[i];
            var vtype = this.data.next();
            assert(vtype === ntype);
            var gotten = this.data[getter]();
            assert(eq(gotten, v));
        }
    
        // Test encode and decode methods.
        var encoded = this.data.encode();
        var copy = new proton.Data();
        while (encoded) {
            encoded = copy.decode(encoded);
        }
        copy.rewind();
    
        for (var i = 0; i < values.length; i++) {
            var v = values[i];
            var vtype = copy.next();
            assert(vtype === ntype);
            var gotten = copy[getter]();
            assert(eq(gotten, v));
        }
        copy.free();
    };  
    
    DataTest.testInt = function() {
        console.log("testInt");
        this._test("int", 1, 2, 3, -1, -2, -3);
        console.log("OK\n");
    
    };  

    DataTest.testString = function() {
        console.log("testString");
        this._test("string", "one", "two", "three", "this is a test", "");
        console.log("OK\n");
    };  

    DataTest.testBigString = function() {
        // Try a 2MB string, this is about as big as we can cope with using the default
        // emscripten heap size.
        console.log("testBigString");
        var data = "";
        for (var i = 0; i < 2000000; i++) {
            data += "*";
        }
        var string = "start\n" + data + "\nfinish\n";
        this._test("string", string);
        console.log("OK\n");
    };  

/* TODO - currently emscripten isn't respecting Module['TOTAL_STACK'] so setting PROTON_TOTAL_STACK doesn't actually increase the stack size.
    DataTest.testReallyBigString = function() {
        // Try a 20MB string, this needs a bigger emscripten heap size and more stack
        // as the default stack is 5MB and strings are allocated on the stack.
        console.log("testReallyBigString");
        var data = "";
        for (var i = 0; i < 20000000; i++) {
            data += "*";
        }
        var string = "start\n" + data + "\nfinish\n";
        this._test("string", string);
        console.log("OK\n");
    };
*/

    DataTest.testFloat = function() {
        console.log("testFloat");
        // We have to use a special comparison here because JavaScript internally
        // only uses doubles and converting between floats and doubles is imprecise.
        this._test("float", 0, 1, 2, 3, 0.1, 0.2, 0.3, -1, -2, -3, -0.1, -0.2, -0.3,
                   function(x, y) {return (x - y < 0.000001);});
        console.log("OK\n");
    };  

    DataTest.testDouble = function() {
        console.log("testDouble");
        this._test("double", 0, 1, 2, 3, 0.1, 0.2, 0.3, -1, -2, -3, -0.1, -0.2, -0.3);
        console.log("OK\n");
    };  

    DataTest.testBinary = function() {
        console.log("testBinary");
        this._test("binary", new proton.Data.Binary(["t".char(), "h".char(), "i".char(), "s".char()]),
                   new proton.Data.Binary("is"), new proton.Data.Binary("a"), new proton.Data.Binary("test"),
                   new proton.Data.Binary("of"), new proton.Data.Binary("двоичные данные"));
        console.log("OK\n");
    };  

    DataTest.testSymbol = function() {
        console.log("testSymbol");
        this._test("symbol", new proton.Data.Symbol("this is a symbol test"),
                             new proton.Data.Symbol("bleh"), new proton.Data.Symbol("blah"));
        console.log("OK\n");
    };

    DataTest.testTimestamp = function() {
        console.log("testTimestamp");
        this._test("timestamp", new Date(0), new Date(12345), new Date(1000000));
        console.log("OK\n");
    };  

    DataTest.testChar = function() {
        console.log("testChar");
        this._test("char", 'a', 'b', 'c', '\u1234');
        console.log("OK\n");
    };  

    DataTest.testUUID = function() {
        console.log("testUUID");
        this._test("uuid", new proton.Data.Uuid(), new proton.Data.Uuid(), new proton.Data.Uuid());
        console.log("OK\n");
    };

    /* TODO
    DataTest.testDecimal32 = function() {
        console.log("testDecimal32");
        //this._test("decimal32", 0, 1, 2, 3, 4, Math.pow(2, 30));
    };

    DataTest.testDecimal64 = function() {
        console.log("testDecimal64");
        //this._test("decimal64", 0, 1, 2, 3, 4, Math.pow(2, 60));
    };  

    DataTest.testDecimal128 = function() {
        console.log("testDecimal128");
        // TODO
    };
    */

    DataTest.testCopy = function() {
        console.log("testCopy");
        this.data.putDESCRIBEDNODE();
        this.data.enter();
        this.data.putULONG(123);
        this.data.putMAPNODE();
        this.data.enter();
        this.data.putSTRING("pi");
        this.data.putDOUBLE(3.14159265359);
        this.data.exit();
        this.data.exit();
    
        var dst = this.data.copy();
        var copy = dst.format();
        var orig = this.data.format();
        assert(copy === orig);
        dst.free();
        console.log("OK\n");
    };  

    DataTest.testCopyNested = function() {
        console.log("testCopyNested");
        var nested = [1, 2, 3, [4, 5, 6], 7, 8, 9];
        this.data.putObject(nested);
        var dst = this.data.copy();
        assert(dst.format() === this.data.format());
        dst.free();
        console.log("OK\n");
    };  

    DataTest.testCopyNestedArray = function() {
        console.log("testCopyNestedArray");
        var nested = [new proton.Data.Array("LIST", [
                        ["first",  [new proton.Data.Array("INT", [1, 2, 3]), "this"]],
                        ["second", [new proton.Data.Array("INT", [1, 2, 3]), "is"]],
                        ["third",  [new proton.Data.Array("INT", [1, 2, 3]), "fun"]]
                        ]),
                    "end"];
        this.data.putObject(nested);
        var dst = this.data.copy();
        assert(dst.format() === this.data.format());
        dst.free();
        console.log("OK\n");
    };

    DataTest.testRoundTrip = function() {
        console.log("testRoundTrip");
        var obj = {key: new Date(1234),
                   123: "blah",
                   c: "bleh",
                   desc: new proton.Data.Described("http://example.org", new proton.Data.Symbol("url")),
                   array: new proton.Data.Array("INT", [1, 2, 3]),
                   list: [1, 2, 3, null, 4],
                   boolean: true};
        // Serialise obj into this.data.
        this.data.putObject(obj);
    
        // Encode this.data into a Binary representation.
        var enc = this.data.encode();
    
        // Create a new Data instance and decode from the Binary representation
        // consuming the Binary contents in the process.
        var data = new proton.Data();
        data.decode(enc);
    
        assert(data.format() === this.data.format());
    
        // Deserialise from the copied Data instance into a new JavaScript Object.
        data.rewind();
        assert(data.next());
        var copy = data.getObject();
    
        // Create yet another Data instance and serialise the new Object into that.
        var data1 = new proton.Data();
        data1.putObject(copy);
    
        // Compare the round tripped Data with the original one.
        assert(data1.format() === this.data.format());
    
        data.free();
        data1.free();
        console.log("OK\n");
    };

    DataTest.testLookup = function() {
        console.log("testLookup");
        var obj = {key: "value",
                   pi: 3.14159,
                   list: [1, 2, 3, 4]};
        // Serialise obj into this.data.
        this.data.putObject(obj);
        this.data.rewind();
        this.data.next();
        this.data.enter();
        this.data.narrow();
        assert(this.data.lookup("pi"));
        assert(this.data.getObject() === 3.14159);
        this.data.rewind();
        assert(this.data.lookup("key"));
        assert(this.data.getObject() === "value");
        this.data.rewind();
        assert(this.data.lookup("list"));
        assert(this.data.getObject().toString() === "1,2,3,4");
        this.data.widen();
        this.data.rewind();
        assert(!this.data.lookup("pi"));
        console.log("OK\n");
    };  

    DataTest.run();
} else {
    console.error("codec.js should be run in Node.js");
}

