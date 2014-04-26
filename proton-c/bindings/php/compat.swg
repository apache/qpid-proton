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
 */

%define CONVERT_LONG_LONG_IN(lvar,t,invar)
  switch ((*(invar))->type) {
      case IS_DOUBLE:
          lvar = (t) (*(invar))->value.dval;
          break;
      case IS_STRING: {
          char * endptr;
          errno = 0;
          lvar = (t) strtoll((*(invar))->value.str.val, &endptr, 10);
          if (*endptr && !errno) break;
          /* FALL THRU */
      }
      default:
          convert_to_long_ex(invar);
          lvar = (t) (*(invar))->value.lval;
  }
%enddef

%pass_by_val(long long, CONVERT_LONG_LONG_IN);

%typemap(out) long long
%{
  if ((long long)LONG_MIN <= $1 && $1 <= (long long)LONG_MAX) {
    return_value->value.lval = (long)($1);
    return_value->type = IS_LONG;
  } else {
    char temp[256];
    sprintf(temp, "%lld", (long long)$1);
    ZVAL_STRING(return_value, temp, 1);
  }
%}
