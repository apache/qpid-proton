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

#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "transform.h"

typedef struct {
  const char *start;
  size_t size;
} pn_group_t;

#define MAX_GROUP (64)

typedef struct {
  size_t groups;
  pn_group_t group[MAX_GROUP];
} pn_matcher_t;

typedef struct {
  pn_string_t *pattern;
  pn_string_t *substitution;
} pn_rule_t;

struct pn_transform_t {
  pn_list_t *rules;
  pn_matcher_t matcher;
  bool matched;
};

static void pn_rule_finalize(void *object)
{
  pn_rule_t *rule = (pn_rule_t *) object;
  pn_free(rule->pattern);
  pn_free(rule->substitution);
}

#define CID_pn_rule CID_pn_object
#define pn_rule_initialize NULL
#define pn_rule_hashcode NULL
#define pn_rule_compare NULL
#define pn_rule_inspect NULL

pn_rule_t *pn_rule(const char *pattern, const char *substitution)
{
  static const pn_class_t clazz = PN_CLASS(pn_rule);
  pn_rule_t *rule = (pn_rule_t *) pn_class_new(&clazz, sizeof(pn_rule_t));
  rule->pattern = pn_string(pattern);
  rule->substitution = pn_string(substitution);
  return rule;
}

static void pn_transform_finalize(void *object)
{
  pn_transform_t *transform = (pn_transform_t *) object;
  pn_free(transform->rules);
}

#define CID_pn_transform CID_pn_object
#define pn_transform_initialize NULL
#define pn_transform_hashcode NULL
#define pn_transform_compare NULL
#define pn_transform_inspect NULL

pn_transform_t *pn_transform()
{
  static const pn_class_t clazz = PN_CLASS(pn_transform);
  pn_transform_t *transform = (pn_transform_t *) pn_class_new(&clazz, sizeof(pn_transform_t));
  transform->rules = pn_list(PN_OBJECT, 0);
  transform->matched = false;
  return transform;
}

void pn_transform_rule(pn_transform_t *transform, const char *pattern,
                       const char *substitution)
{
  assert(transform);
  pn_rule_t *rule = pn_rule(pattern, substitution);
  pn_list_add(transform->rules, rule);
  pn_decref(rule);
}

static void pni_sub(pn_matcher_t *matcher, size_t group, const char *text, size_t matched)
{
  if (group > matcher->groups) {
    matcher->groups = group;
  }
  matcher->group[group].start = text - matched;
  matcher->group[group].size = matched;
}

static bool pni_match_r(pn_matcher_t *matcher, const char *pattern, const char *text, size_t group, size_t matched)
{
  bool match;

  char p = *pattern;
  char c = *text;

  switch (p) {
  case '\0': return c == '\0';
  case '%':
  case '*':
    switch (c) {
    case '\0':
      match = pni_match_r(matcher, pattern + 1, text, group + 1, 0);
      if (match) pni_sub(matcher, group, text, matched);
      return match;
    case '/':
      if (p == '%') {
        match = pni_match_r(matcher, pattern + 1, text, group + 1, 0);
        if (match) pni_sub(matcher, group, text, matched);
        return match;
      }
    default:
      match = pni_match_r(matcher, pattern, text + 1, group, matched + 1);
      if (!match) {
        match = pni_match_r(matcher, pattern + 1, text, group + 1, 0);
        if (match) pni_sub(matcher, group, text, matched);
      }
      return match;
    }
  default:
    return c == p && pni_match_r(matcher, pattern + 1, text + 1, group, 0);
  }
}

static bool pni_match(pn_matcher_t *matcher, const char *pattern, const char *text)
{
  text = text ? text : "";
  matcher->groups = 0;
  if (pni_match_r(matcher, pattern, text, 1, 0)) {
    matcher->group[0].start = text;
    matcher->group[0].size = strlen(text);
    return true;
  } else {
    matcher->groups = 0;
    return false;
  }
}

static size_t pni_substitute(pn_matcher_t *matcher, const char *pattern, char *dest, size_t limit)
{
  size_t result = 0;

  while (*pattern) {
    switch (*pattern) {
    case '$':
      pattern++;
      if (*pattern == '$') {
        if (result < limit) {
          *dest++ = *pattern;
        }
        pattern++;
        result++;
      } else {
        size_t idx = 0;
        while (isdigit(*pattern)) {
          idx *= 10;
          idx += *pattern++ - '0';
        }

        if (idx <= matcher->groups) {
          pn_group_t *group = &matcher->group[idx];
          for (size_t i = 0; i < group->size; i++) {
            if (result < limit) {
              *dest++ = group->start[i];
            }
            result++;
          }
        }
      }
      break;
    default:
      if (result < limit) {
        *dest++ = *pattern;
      }
      pattern++;
      result++;
      break;
    }
  }

  if (result < limit) {
    *dest = '\0';
  }

  return result;
}

int pn_transform_apply(pn_transform_t *transform, const char *src,
                       pn_string_t *dst)
{
  for (size_t i = 0; i < pn_list_size(transform->rules); i++)
  {
    pn_rule_t *rule = (pn_rule_t *) pn_list_get(transform->rules, i);
    if (pni_match(&transform->matcher, pn_string_get(rule->pattern), src)) {
      transform->matched = true;
      if (!pn_string_get(rule->substitution)) {
        return pn_string_set(dst, NULL);
      }

      while (true) {
        size_t capacity = pn_string_capacity(dst);
        size_t n = pni_substitute(&transform->matcher,
                                  pn_string_get(rule->substitution),
                                  pn_string_buffer(dst), capacity);
        int err = pn_string_resize(dst, n);
        if (err) return err;
        if (n <= capacity) {
          return 0;
        }
      }
    }
  }

  transform->matched = false;
  return pn_string_set(dst, src);
}

bool pn_transform_matched(pn_transform_t *transform)
{
  return transform->matched;
}

int pn_transform_get_substitutions(pn_transform_t *transform,
                                   pn_list_t *substitutions)
{
  int size = pn_list_size(transform->rules);
  for (size_t i = 0; i < (size_t)size; i++) {
    pn_rule_t *rule = (pn_rule_t *)pn_list_get(transform->rules, i);
    pn_list_add(substitutions, rule->substitution);
  }

  return size;
}
