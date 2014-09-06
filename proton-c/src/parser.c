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

#include <proton/parser.h>
#include <proton/scanner.h>
#include <proton/error.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "platform.h"

struct pn_parser_t {
  pn_scanner_t *scanner;
  char *atoms;
  size_t size;
  size_t capacity;
  int error_code;
};

pn_parser_t *pn_parser()
{
  pn_parser_t *parser = (pn_parser_t *) malloc(sizeof(pn_parser_t));
  parser->scanner = pn_scanner();
  parser->atoms = NULL;
  parser->size = 0;
  parser->capacity = 0;
  return parser;
}

void pn_parser_ensure(pn_parser_t *parser, size_t size)
{
  while (parser->capacity - parser->size < size) {
    parser->capacity = parser->capacity ? 2 * parser->capacity : 1024;
    parser->atoms = (char *) realloc(parser->atoms, parser->capacity);
  }
}

void pn_parser_line_info(pn_parser_t *parser, int *line, int *col)
{
  pn_scanner_line_info(parser->scanner, line, col);
}

int pn_parser_err(pn_parser_t *parser, int code, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int err = pn_scanner_verr(parser->scanner, code, fmt, ap);
  va_end(ap);
  return err;
}

int pn_parser_errno(pn_parser_t *parser)
{
  return pn_scanner_errno(parser->scanner);
}

const char *pn_parser_error(pn_parser_t *parser)
{
  return pn_scanner_error(parser->scanner);
}

void pn_parser_free(pn_parser_t *parser)
{
  if (parser) {
    pn_scanner_free(parser->scanner);
    free(parser->atoms);
    free(parser);
  }
}

int pn_parser_shift(pn_parser_t *parser)
{
  return pn_scanner_shift(parser->scanner);
}

pn_token_t pn_parser_token(pn_parser_t *parser)
{
  return pn_scanner_token(parser->scanner);
}

int pn_parser_value(pn_parser_t *parser, pn_data_t *data);

int pn_parser_descriptor(pn_parser_t *parser, pn_data_t *data)
{
  if (pn_parser_token(parser).type == PN_TOK_AT) {
    int err = pn_parser_shift(parser);
    if (err) return err;

    err = pn_data_put_described(data);
    if (err) return pn_parser_err(parser, err, "error writing described");
    pn_data_enter(data);
    for (int i = 0; i < 2; i++) {
      err = pn_parser_value(parser, data);
      if (err) return err;
    }
    pn_data_exit(data);
    return 0;
  } else {
    return pn_parser_err(parser, PN_ERR, "expecting '@'");
  }
}

int pn_parser_map(pn_parser_t *parser, pn_data_t *data)
{
  if (pn_parser_token(parser).type == PN_TOK_LBRACE) {
    int err = pn_parser_shift(parser);
    if (err) return err;

    err = pn_data_put_map(data);
    if (err) return pn_parser_err(parser, err, "error writing map");

    pn_data_enter(data);

    if (pn_parser_token(parser).type != PN_TOK_RBRACE) {
      while (true) {
        err = pn_parser_value(parser, data);
        if (err) return err;

        if (pn_parser_token(parser).type == PN_TOK_EQUAL) {
          err = pn_parser_shift(parser);
          if (err) return err;
        } else {
          return pn_parser_err(parser, PN_ERR, "expecting '='");
        }

        err = pn_parser_value(parser, data);
        if (err) return err;

        if (pn_parser_token(parser).type == PN_TOK_COMMA) {
          err = pn_parser_shift(parser);
          if (err) return err;
        } else {
          break;
        }
      }
    }

    pn_data_exit(data);

    if (pn_parser_token(parser).type == PN_TOK_RBRACE) {
      return pn_parser_shift(parser);
    } else {
      return pn_parser_err(parser, PN_ERR, "expecting '}'");
    }
  } else {
    return pn_parser_err(parser, PN_ERR, "expecting '{'");
  }
}

int pn_parser_list(pn_parser_t *parser, pn_data_t *data)
{
  int err;

  if (pn_parser_token(parser).type == PN_TOK_LBRACKET) {
    err = pn_parser_shift(parser);
    if (err) return err;

    err = pn_data_put_list(data);
    if (err) return pn_parser_err(parser, err, "error writing list");

    pn_data_enter(data);

    if (pn_parser_token(parser).type != PN_TOK_RBRACKET) {
      while (true) {
        err = pn_parser_value(parser, data);
        if (err) return err;

        if (pn_parser_token(parser).type == PN_TOK_COMMA) {
          err = pn_parser_shift(parser);
          if (err) return err;
        } else {
          break;
        }
      }
    }

    pn_data_exit(data);

    if (pn_parser_token(parser).type == PN_TOK_RBRACKET) {
      return pn_parser_shift(parser);
    } else {
      return pn_parser_err(parser, PN_ERR, "expecting ']'");
    }
  } else {
    return pn_parser_err(parser, PN_ERR, "expecting '['");
  }
}

void pn_parser_append_tok(pn_parser_t *parser, char *dst, int *idx)
{
  memcpy(dst + *idx, pn_parser_token(parser).start, pn_parser_token(parser).size);
  *idx += pn_parser_token(parser).size;
}

int pn_parser_number(pn_parser_t *parser, pn_data_t *data)
{
  bool dbl = false;
  char number[1024];
  int idx = 0;
  int err;

  bool negate = false;

  if (pn_parser_token(parser).type == PN_TOK_NEG || pn_parser_token(parser).type == PN_TOK_POS) {
    if (pn_parser_token(parser).type == PN_TOK_NEG)
      negate = !negate;
    err = pn_parser_shift(parser);
    if (err) return err;
  }

  if (pn_parser_token(parser).type == PN_TOK_FLOAT || pn_parser_token(parser).type == PN_TOK_INT) {
    dbl = pn_parser_token(parser).type == PN_TOK_FLOAT;
    pn_parser_append_tok(parser, number, &idx);
    err = pn_parser_shift(parser);
    if (err) return err;
  } else {
    return pn_parser_err(parser, PN_ERR, "expecting FLOAT or INT");
  }

  number[idx] = '\0';

  if (dbl) {
    double value = atof(number);
    if (negate) {
      value = -value;
    }
    err = pn_data_put_double(data, value);
    if (err) return pn_parser_err(parser, err, "error writing double");
  } else {
    int64_t value = pn_i_atoll(number);
    if (negate) {
      value = -value;
    }
    err = pn_data_put_long(data, value);
    if (err) return pn_parser_err(parser, err, "error writing long");
  }

  return 0;
}

int pn_parser_unquote(pn_parser_t *parser, char *dst, const char *src, size_t *n)
{
  size_t idx = 0;
  bool escape = false;
  int start, end;
  if (src[0] != '"') {
    if (src[1] == '"') {
      start = 2;
      end = *n - 1;
    } else {
      start = 1;
      end = *n;
    }
  } else {
    start = 1;
    end = *n - 1;
  }
  for (int i = start; i < end; i++)
  {
    char c = src[i];
    if (escape) {
      switch (c) {
      case '"':
      case '\\':
      case '/':
        dst[idx++] = c;
        escape = false;
        break;
      case 'b':
        dst[idx++] = '\b';
        break;
      case 'f':
        dst[idx++] = '\f';
        break;
      case 'n':
        dst[idx++] = '\n';
        break;
      case 'r':
        dst[idx++] = '\r';
        break;
      case 't':
        dst[idx++] = '\t';
        break;
      case 'x':
        {
          char n1 = toupper(src[i+1]);
          char n2 = n1 ? toupper(src[i+2]) : 0;
          if (!n2) {
            return pn_parser_err(parser, PN_ERR, "truncated escape code");
          }
          int d1 = isdigit(n1) ? n1 - '0' : n1 - 'A' + 10;
          int d2 = isdigit(n2) ? n2 - '0' : n2 - 'A' + 10;
          dst[idx++] = d1*16 + d2;
          i += 2;
        }
        break;
      // XXX: need to handle unicode escapes: 'u'
      default:
        return pn_parser_err(parser, PN_ERR, "unrecognized escape code");
      }
      escape = false;
    } else {
      switch (c)
      {
      case '\\':
        escape = true;
        break;
      default:
        dst[idx++] = c;
        break;
      }
    }
  }
  dst[idx++] = '\0';
  *n = idx;
  return 0;
}

int pn_parser_value(pn_parser_t *parser, pn_data_t *data)
{
  int err;
  size_t n;
  char *dst;

  pn_token_t tok = pn_parser_token(parser);

  switch (tok.type)
  {
  case PN_TOK_AT:
    return pn_parser_descriptor(parser, data);
  case PN_TOK_LBRACE:
    return pn_parser_map(parser, data);
  case PN_TOK_LBRACKET:
    return pn_parser_list(parser, data);
  case PN_TOK_BINARY:
  case PN_TOK_SYMBOL:
  case PN_TOK_STRING:
    n = tok.size;
    pn_parser_ensure(parser, n);
    dst = parser->atoms + parser->size;
    err = pn_parser_unquote(parser, dst, tok.start, &n);
    if (err) return err;
    parser->size += n;
    switch (tok.type) {
    case PN_TOK_BINARY:
      err = pn_data_put_binary(data, pn_bytes(n - 1, dst));
      break;
    case PN_TOK_STRING:
      err = pn_data_put_string(data, pn_bytes(n - 1, dst));
      break;
    case PN_TOK_SYMBOL:
      err = pn_data_put_symbol(data, pn_bytes(n - 1, dst));
      break;
    default:
      return pn_parser_err(parser, PN_ERR, "internal error");
    }
    if (err) return pn_parser_err(parser, err, "error writing string/binary/symbol");
    return pn_parser_shift(parser);
  case PN_TOK_POS:
  case PN_TOK_NEG:
  case PN_TOK_FLOAT:
  case PN_TOK_INT:
    return pn_parser_number(parser, data);
  case PN_TOK_TRUE:
    err = pn_data_put_bool(data, true);
    if (err) return pn_parser_err(parser, err, "error writing boolean");
    return pn_parser_shift(parser);
  case PN_TOK_FALSE:
    err = pn_data_put_bool(data, false);
    if (err) return pn_parser_err(parser, err, "error writing boolean");
    return pn_parser_shift(parser);
  case PN_TOK_NULL:
    err = pn_data_put_null(data);
    if (err) return pn_parser_err(parser, err, "error writing null");
    return pn_parser_shift(parser);
  default:
    return pn_parser_err(parser, PN_ERR, "expecting one of '[', '{', STRING, "
                         "SYMBOL, BINARY, true, false, null, NUMBER");
  }
}

int pn_parser_parse_r(pn_parser_t *parser, pn_data_t *data)
{
  while (true) {
    int err;
    switch (pn_parser_token(parser).type)
    {
    case PN_TOK_EOS:
      return 0;
    case PN_TOK_ERR:
      return PN_ERR;
    default:
      err = pn_parser_value(parser, data);
      if (err) return err;
    }
  }
}

int pn_parser_parse(pn_parser_t *parser, const char *str, pn_data_t *data)
{
  int err = pn_scanner_start(parser->scanner, str);
  if (err) return err;
  parser->size = 0;
  return pn_parser_parse_r(parser, data);
}
