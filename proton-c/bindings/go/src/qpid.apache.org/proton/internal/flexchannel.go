/*
Licensed to the Apache Software Foundation (ASF) under one
or more contributor license agreements.  See the NOTICE file
distributed with this work for additional information
regarding copyright ownership.  The ASF licenses this file
to you under the Apache License, Version 2.0 (the
"License"); you may not use this file except in compliance
with the License.  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied.  See the License for the
specific language governing permissions and limitations
under the License.
*/

package internal

// FlexChannel acts like a channel with an automatically sized buffer, see NewFlexChannel().
type FlexChannel struct {
	// In channel to send to. close(In) will close the FlexChannel once buffer has drained.
	In chan<- interface{}
	// Out channel to receive from. Out closes when In has closed and the buffer is empty.
	Out <-chan interface{}

	in, out chan interface{}
	buffer  []interface{}
	limit   int
}

// NewFlexChannel creates a FlexChannel, a channel with an automatically-sized buffer.
//
// Initially the buffer size is 0, the buffer grows as needed up to limit. limit < 0 means
// there is no limit.
//
func NewFlexChannel(limit int) *FlexChannel {
	fc := &FlexChannel{
		in:     make(chan interface{}),
		out:    make(chan interface{}),
		buffer: make([]interface{}, 0),
		limit:  limit,
	}
	fc.In = fc.in
	fc.Out = fc.out
	go fc.run()
	return fc
}

func (fc *FlexChannel) run() {
	defer func() { // Flush the channel on exit
		for _, data := range fc.buffer {
			fc.out <- data
		}
		close(fc.out)
	}()

	for {
		var usein, useout chan interface{}
		var outvalue interface{}
		if len(fc.buffer) > 0 {
			useout = fc.out
			outvalue = fc.buffer[0]
		}
		if len(fc.buffer) < fc.limit || fc.limit < 0 {
			usein = fc.in
		}
		Assert(usein != nil || useout != nil)
		select {
		case useout <- outvalue:
			fc.buffer = fc.buffer[1:]
		case data, ok := <-usein:
			if ok {
				fc.buffer = append(fc.buffer, data)
			} else {
				return
			}
		}
	}
}
