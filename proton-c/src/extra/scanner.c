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

#include "scanner.h"

#include "platform/platform.h"

#include <proton/error.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ERROR_SIZE (1024)

struct pn_scanner_t {
  const char *input;
  const char *position;
  pn_token_t token;
  char *atoms;
  size_t size;
  size_t capacity;
  pn_error_t *error;
};

static const char *pni_token_type(pn_token_type_t type)
{
  switch (type)
  {
  case PN_TOK_LBRACE: return "LBRACE";
  case PN_TOK_RBRACE: return "RBRACE";
  case PN_TOK_LBRACKET: return "LBRACKET";
  case PN_TOK_RBRACKET: return "RBRACKET";
  case PN_TOK_EQUAL: return "EQUAL";
  case PN_TOK_COMMA: return "COMMA";
  case PN_TOK_POS: return "POS";
  case PN_TOK_NEG: return "NEG";
  case PN_TOK_DOT: return "DOT";
  case PN_TOK_AT: return "AT";
  case PN_TOK_DOLLAR: return "DOLLAR";
  case PN_TOK_BINARY: return "BINARY";
  case PN_TOK_STRING: return "STRING";
  case PN_TOK_SYMBOL: return "SYMBOL";
  case PN_TOK_ID: return "ID";
  case PN_TOK_FLOAT: return "FLOAT";
  case PN_TOK_INT: return "INT";
  case PN_TOK_TRUE: return "TRUE";
  case PN_TOK_FALSE: return "FALSE";
  case PN_TOK_NULL: return "NULL";
  case PN_TOK_EOS: return "EOS";
  case PN_TOK_ERR: return "ERR";
  default: return "<UNKNOWN>";
  }
}

pn_scanner_t *pn_scanner()
{
  pn_scanner_t *scanner = (pn_scanner_t *) malloc(sizeof(pn_scanner_t));
  if (scanner) {
    scanner->input = NULL;
    scanner->error = pn_error();
  }
  return scanner;
}

void pn_scanner_free(pn_scanner_t *scanner)
{
  if (scanner) {
    pn_error_free(scanner->error);
    free(scanner);
  }
}

pn_token_t pn_scanner_token(pn_scanner_t *scanner)
{
  if (scanner) {
    return scanner->token;
  } else {
    pn_token_t tok = {PN_TOK_ERR, 0, (size_t)0};
    return tok;
  }
}

void pn_scanner_line_info(pn_scanner_t *scanner, int *line, int *col)
{
  *line = 1;
  *col = 0;

  for (const char *c = scanner->input; *c && c <= scanner->token.start; c++) {
    if (*c == '\n') {
      *line += 1;
      *col = -1;
    } else {
      *col += 1;
    }
  }
}

int pn_scanner_err(pn_scanner_t *scanner, int code, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int err = pn_scanner_verr(scanner, code, fmt, ap);
  va_end(ap);
  return err;
}

int pn_scanner_verr(pn_scanner_t *scanner, int code, const char *fmt, va_list ap)
{
  char error[ERROR_SIZE];

  int line, col;
  pn_scanner_line_info(scanner, &line, &col);
  int size = scanner->token.size;
  int ln = pni_snprintf(error, ERROR_SIZE,
                    "input line %i column %i %s:'%.*s': ", line, col,
                    pni_token_type(scanner->token.type),
                    size, scanner->token.start);
  if (ln >= ERROR_SIZE) {
    return pn_scanner_err(scanner, code, "error info truncated");
  } else if (ln < 0) {
    error[0] = '\0';
  }

  int n = pni_snprintf(error + ln, ERROR_SIZE - ln, fmt, ap);

  if (n >= ERROR_SIZE - ln) {
    return pn_scanner_err(scanner, code, "error info truncated");
  } else if (n < 0) {
    error[0] = '\0';
  }

  return pn_error_set(scanner->error, code, error);
}

int pn_scanner_errno(pn_scanner_t *scanner)
{
  return pn_error_code(scanner->error);
}

const char *pn_scanner_error(pn_scanner_t *scanner)
{
  return pn_error_text(scanner->error);
}

static void pni_scanner_emit(pn_scanner_t *scanner, pn_token_type_t type, const char *start, size_t size)
{
  scanner->token.type = type;
  scanner->token.start = start;
  scanner->token.size = size;
}

static int pni_scanner_quoted(pn_scanner_t *scanner, const char *str, int start,
                      pn_token_type_t type)
{
  bool escape = false;

  for (int i = start; true; i++) {
    char c = str[i];
    if (escape) {
      escape = false;
    } else {
      switch (c) {
      case '\0':
      case '"':
        pni_scanner_emit(scanner, c ? type : PN_TOK_ERR,
                        str, c ? i + 1 : i);
        return c ? 0 : pn_scanner_err(scanner, PN_ERR, "missmatched quote");
      case '\\':
        escape = true;
        break;
      }
    }
  }
}

static int pni_scanner_binary(pn_scanner_t *scanner, const char *str)
{
  return pni_scanner_quoted(scanner, str, 2, PN_TOK_BINARY);
}

static int pni_scanner_string(pn_scanner_t *scanner, const char *str)
{
  return pni_scanner_quoted(scanner, str, 1, PN_TOK_STRING);
}

static int pni_scanner_alpha_end(pn_scanner_t *scanner, const char *str, int start)
{
  for (int i = start; true; i++) {
    char c = str[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
      return i;
    }
  }
}

static int pni_scanner_alpha(pn_scanner_t *scanner, const char *str)
{
  int n = pni_scanner_alpha_end(scanner, str, 0);
  pn_token_type_t type;
  if (!strncmp(str, "true", n)) {
    type = PN_TOK_TRUE;
  } else if (!strncmp(str, "false", n)) {
    type = PN_TOK_FALSE;
  } else if (!strncmp(str, "null", n)) {
    type = PN_TOK_NULL;
  } else {
    type = PN_TOK_ID;
  }

  pni_scanner_emit(scanner, type, str, n);
  return 0;
}

static int pni_scanner_symbol(pn_scanner_t *scanner, const char *str)
{
  char c = str[1];

  if (c == '"') {
    return pni_scanner_quoted(scanner, str, 2, PN_TOK_SYMBOL);
  } else {
    int n = pni_scanner_alpha_end(scanner, str, 1);
    pni_scanner_emit(scanner, PN_TOK_SYMBOL, str, n);
    return 0;
  }
}

static int pni_scanner_number(pn_scanner_t *scanner, const char *str)
{
  bool dot = false;
  bool exp = false;

  int i = 0;

  if (str[i] == '+' || str[i] == '-') {
    i++;
  }

  for ( ; true; i++) {
    char c = str[i];
    switch (c) {
    case '0': case '1': case '2': case '3': case '4': case '5': case '6':
    case '7': case '8': case '9':
      continue;
    case '.':
      if (dot) {
        pni_scanner_emit(scanner, PN_TOK_FLOAT, str, i);
        return 0;
      } else {
        dot = true;
      }
      continue;
    case 'e':
    case 'E':
      if (exp) {
        pni_scanner_emit(scanner, PN_TOK_FLOAT, str, i);
        return 0;
      } else {
        dot = true;
        exp = true;
        if (str[i+1] == '+' || str[i+1] == '-') {
          i++;
        }
        continue;
      }
    default:
      if (dot || exp) {
        pni_scanner_emit(scanner, PN_TOK_FLOAT, str, i);
        return 0;
      } else {
        pni_scanner_emit(scanner, PN_TOK_INT, str, i);
        return 0;
      }
    }
  }
}

static int pni_scanner_single(pn_scanner_t *scanner, const char *str, pn_token_type_t type)
{
  pni_scanner_emit(scanner, type, str, 1);
  return 0;
}

int pn_scanner_start(pn_scanner_t *scanner, const char *input)
{
  if (!scanner || !input) return PN_ARG_ERR;
  scanner->input = input;
  scanner->position = input;
  return pn_scanner_scan(scanner);
}

int pn_scanner_scan(pn_scanner_t *scanner)
{
  const char *str = scanner->position;
  char n;

  for (char c; true; str++) {
    c = *str;
    switch (c)
    {
    case '{':
      return pni_scanner_single(scanner, str, PN_TOK_LBRACE);
    case '}':
      return pni_scanner_single(scanner, str, PN_TOK_RBRACE);
    case'[':
      return pni_scanner_single(scanner, str, PN_TOK_LBRACKET);
    case ']':
      return pni_scanner_single(scanner, str, PN_TOK_RBRACKET);
    case '=':
      return pni_scanner_single(scanner, str, PN_TOK_EQUAL);
    case ',':
      return pni_scanner_single(scanner, str, PN_TOK_COMMA);
    case '.':
      n = *(str+1);
      if ((n >= '0' && n <= '9')) {
        return pni_scanner_number(scanner, str);
      } else {
        return pni_scanner_single(scanner, str, PN_TOK_DOT);
      }
    case '@':
      return pni_scanner_single(scanner, str, PN_TOK_AT);
    case '$':
      return pni_scanner_single(scanner, str, PN_TOK_DOLLAR);
    case '-':
      n = *(str+1);
      if ((n >= '0' && n <= '9') || n == '.') {
        return pni_scanner_number(scanner, str);
      } else {
        return pni_scanner_single(scanner, str, PN_TOK_NEG);
      }
    case '+':
      n = *(str+1);
      if ((n >= '0' && n <= '9') || n == '.') {
        return pni_scanner_number(scanner, str);
      } else {
        return pni_scanner_single(scanner, str, PN_TOK_POS);
      }
    case ' ': case '\t': case '\r': case '\v': case '\f': case '\n':
      break;
    case '0': case '1': case '2': case '3': case '4': case '5': case '6':
    case '7': case '8': case '9':
      return pni_scanner_number(scanner, str);
    case ':':
      return pni_scanner_symbol(scanner, str);
    case '"':
      return pni_scanner_string(scanner, str);
    case 'b':
      if (str[1] == '"') {
        return pni_scanner_binary(scanner, str);
      }
    case 'a': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h':
    case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o':
    case 'p': case 'q': case 'r': case 's': case 't': case 'u': case 'v':
    case 'w': case 'x': case 'y': case 'z': case 'A': case 'B': case 'C':
    case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
    case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q':
    case 'R': case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
    case 'Y': case 'Z':
      return pni_scanner_alpha(scanner, str);
    case '\0':
      pni_scanner_emit(scanner, PN_TOK_EOS, str, 0);
      return PN_EOS;
    default:
      pni_scanner_emit(scanner, PN_TOK_ERR, str, 1);
      return pn_scanner_err(scanner, PN_ERR, "illegal character");
    }
  }
}

int pn_scanner_shift(pn_scanner_t *scanner)
{
  scanner->position = scanner->token.start + scanner->token.size;
  int err = pn_scanner_scan(scanner);
  if (err == PN_EOS) {
    return 0;
  } else {
    return err;
  }
}
