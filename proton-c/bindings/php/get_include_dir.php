<?php

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
