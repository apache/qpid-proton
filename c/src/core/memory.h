#ifndef MEMORY_H
#define MEMORY_H 1

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

#include <proton/object.h>

#include <stddef.h>

void pni_init_memory(void);
void pni_fini_memory(void);

void pni_mem_setup_logging(void);

void *pni_mem_allocate(const pn_class_t *clazz, size_t size);
void *pni_mem_zallocate(const pn_class_t *clazz, size_t size);
void pni_mem_deallocate(const pn_class_t *clazz, void *object);

void *pni_mem_suballocate(const pn_class_t *clazz, void *object, size_t size);
void *pni_mem_subreallocate(const pn_class_t *clazz, void *object, void *buffer, size_t size);
void pni_mem_subdeallocate(const pn_class_t *clazz, void *object, void *buffer);

#endif // MEMORY_H
