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

package util

import (
	"container/list"
	"qpid.apache.org/proton/amqp"
	"sync"
)

// Queue is a concurrent-safe queue of amqp.Message.
type Queue struct {
	name     string
	messages list.List // List of amqp.Message
	// Send to Push to push a message onto back of queue
	Push chan amqp.Message
	// Receive from Pop to pop a message from the front of the queue.
	Pop chan amqp.Message
	// Send to Putback to put an unsent message back on the front of the queue.
	Putback chan amqp.Message
}

func NewQueue(name string) *Queue {
	q := &Queue{
		name:    name,
		Push:    make(chan amqp.Message),
		Pop:     make(chan amqp.Message),
		Putback: make(chan amqp.Message),
	}
	go q.run()
	return q
}

// Close the queue. Any remaining messages on Pop can still be received.
func (q *Queue) Close() { close(q.Push); close(q.Putback) }

// Run runs the queue, returns when q.Close() is called.
func (q *Queue) run() {
	defer close(q.Pop)
	for {
		var pop chan amqp.Message
		var front amqp.Message
		if el := q.messages.Front(); el != nil {
			front = el.Value.(amqp.Message)
			pop = q.Pop // Only select for pop if there is something to pop.
		}
		select {
		case m, ok := <-q.Push:
			if !ok {
				return
			}
			Debugf("%s push: %s\n", q.name, FormatMessage(m))
			q.messages.PushBack(m)
		case m, ok := <-q.Putback:
			Debugf("%s put-back: %s\n", q.name, FormatMessage(m))
			if !ok {
				return
			}
			q.messages.PushFront(m)
		case pop <- front:
			Debugf("%s pop: %s\n", q.name, FormatMessage(front))
			q.messages.Remove(q.messages.Front())
		}
	}
}

// QueueMap is a concurrent-safe map of queues that creates new queues
// on demand.
type QueueMap struct {
	lock sync.Mutex
	m    map[string]*Queue
}

func MakeQueueMap() QueueMap { return QueueMap{m: make(map[string]*Queue)} }

func (qm *QueueMap) Get(name string) *Queue {
	if name == "" {
		panic("Attempt to get queue with no name")
	}
	qm.lock.Lock()
	defer qm.lock.Unlock()
	q := qm.m[name]
	if q == nil {
		q = NewQueue(name)
		qm.m[name] = q
		Debugf("queue %s create", name)
	}
	return q
}
