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

#include <proton/util.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include "encodings.h"
#include "value-internal.h"

static char type_to_code(enum TYPE type)
{
  switch (type)
  {
  case EMPTY: return 'n';
  case BYTE: return 'b';
  case UBYTE: return 'B';
  case SHORT: return 'h';
  case USHORT: return 'H';
  case INT: return 'i';
  case UINT: return 'I';
  case LONG: return 'l';
  case ULONG: return 'L';
  case FLOAT: return 'f';
  case DOUBLE: return 'd';
  case CHAR: return 'C';
  case SYMBOL: return 's';
  case STRING: return 'S';
  case BINARY: return 'z';
  case LIST: return 't';
  case MAP: return 'm';
  default: return -1;
  }
}

static uint8_t type_to_amqp_code(enum TYPE type)
{
  switch (type)
  {
  case EMPTY: return PNE_NULL;
  case BOOLEAN: return PNE_BOOLEAN;
  case BYTE: return PNE_BYTE;
  case UBYTE: return PNE_UBYTE;
  case SHORT: return PNE_SHORT;
  case USHORT: return PNE_USHORT;
  case INT: return PNE_INT;
  case UINT: return PNE_UINT;
  case CHAR: return PNE_UTF32;
  case LONG: return PNE_LONG;
  case ULONG: return PNE_ULONG;
  case FLOAT: return PNE_FLOAT;
  case DOUBLE: return PNE_DOUBLE;
  case SYMBOL: return PNE_SYM32;
  case STRING: return PNE_STR32_UTF8;
  case BINARY: return PNE_VBIN32;
  case LIST: return PNE_LIST32;
  case MAP: return PNE_MAP32;
  case ARRAY: return PNE_ARRAY32;
  default:
    pn_fatal("no amqp code for type: %i", type);
    return -1;
  }
}

pn_array_t *pn_array(enum TYPE type, int capacity)
{
  pn_array_t *l = malloc(sizeof(pn_array_t) + capacity*sizeof(pn_value_t));
  if (l) {
    l->type = type;
    l->capacity = capacity;
    l->size = 0;
  }
  return l;
}

int pn_array_add(pn_array_t *a, pn_value_t v)
{
  if (a->capacity <= a->size) {
    fprintf(stderr, "wah!\n");
    return -1;
  }

  a->values[a->size++] = v;
  return 0;
}

void pn_free_array(pn_array_t *a)
{
  free(a);
}

void pn_visit_array(pn_array_t *a, void (*visitor)(pn_value_t))
{
  for (int i = 0; i < a->size; i++)
  {
    pn_visit(a->values[i], visitor);
  }
}

size_t pn_format_sizeof_array(pn_array_t *array)
{
  size_t result = 4;
  for (int i = 0; i < array->size; i++)
  {
    result += pn_format_sizeof(array->values[i]) + 2;
  }
  return result;
}

int pn_format_array(char **pos, char *limit, pn_array_t *array)
{
  int e;
  if ((e = pn_fmt(pos, limit, "@%c[", type_to_code(array->type)))) return e;
  if ((e = pn_format_value(pos, limit, array->values, array->size))) return e;
  if ((e = pn_fmt(pos, limit, "]"))) return e;
  return 0;
}

size_t pn_encode_sizeof_array(pn_array_t *array)
{
  size_t result = 9;
  for (int i = 0; i < array->size; i++)
  {
    // XXX: this is wrong, need to compensate for code
    result += pn_encode_sizeof(array->values[i]);
  }
  return result;
}

size_t pn_encode_array(pn_array_t *array, char *out)
{
  // code
  out[0] = (uint8_t) PNE_ARRAY32;
  // size will be backfilled
  // count
  *((uint32_t *) (out + 5)) = htonl(array->size);
  // element code
  out[9] = (uint8_t) type_to_amqp_code(array->type);

  char *vout = out + 10;
  for (int i = 0; i < array->size; i++)
  {
    char *codeptr = vout - 1;
    char codeval = *codeptr;
    vout += pn_encode(array->values[i], vout-1) - 1;
    *codeptr = codeval;
  }

  // backfill size
  *((uint32_t *) (out + 1)) = htonl(vout - out - 5);
  return vout - out;
}
