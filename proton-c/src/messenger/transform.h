#ifndef _PROTON_TRANSFORM_H
#define _PROTON_TRANSFORM_H 1

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
#include <proton/buffer.h>

typedef struct pn_transform_t pn_transform_t;

pn_transform_t *pn_transform(void);
void pn_transform_rule(pn_transform_t *transform, const char *pattern,
                       const char *substitution);
int pn_transform_apply(pn_transform_t *transform, const char *src,
                       pn_string_t *dest);
bool pn_transform_matched(pn_transform_t *transform);
int pn_transform_get_substitutions(pn_transform_t *transform,
                                   pn_list_t *substitutions);

#endif /* transform.h */
