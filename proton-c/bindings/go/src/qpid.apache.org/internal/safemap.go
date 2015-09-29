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

import (
	"sync"
)

// SafeMap is a goroutine-safe map of interface{} to interface{}.
type SafeMap struct {
	m    map[interface{}]interface{}
	lock sync.Mutex
}

func MakeSafeMap() SafeMap { return SafeMap{m: make(map[interface{}]interface{})} }

func (m *SafeMap) Get(key interface{}) interface{} {
	m.lock.Lock()
	defer m.lock.Unlock()
	return m.m[key]
}

func (m *SafeMap) GetOk(key interface{}) (interface{}, bool) {
	m.lock.Lock()
	defer m.lock.Unlock()
	v, ok := m.m[key]
	return v, ok
}

func (m *SafeMap) Put(key, value interface{}) {
	m.lock.Lock()
	defer m.lock.Unlock()
	m.m[key] = value
}

func (m *SafeMap) Delete(key interface{}) {
	m.lock.Lock()
	defer m.lock.Unlock()
	delete(m.m, key)
}
