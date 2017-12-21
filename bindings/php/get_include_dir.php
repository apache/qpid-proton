<?php
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


$prefix = $argv[1];
$include_path = ini_get("include_path");

$php_dir = null;
$pear_dir = null;
$abs_dir = null;

foreach (explode(PATH_SEPARATOR, $include_path) as $include_dir) {
  if (strpos($include_dir, ".") === false &&
      strpos($include_dir, $prefix) === 0) {
    $abs_dir = $include_dir;
    $suffix = substr($abs_dir, strlen($prefix));
    if (strpos($suffix, "php") !== false) {
      $php_dir = $abs_dir;
    }
    if (strpos($suffix, "pear") !== false) {
      $pear_dir = $abs_dir;
    }
  }
}

if ($php_dir) {
  print $php_dir;
} else if ($pear_dir) {
  print $pear_dir;
} else if ($abs_dir) {
  print $abs_dir;
}

print "\n";

?>
