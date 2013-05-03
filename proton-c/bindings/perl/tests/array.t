#!/bin/env perl -w
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

use Test::More qw(no_plan);
use Test::Number::Delta within => 1e-3;
use Test::Exception;

require 'utils.pm';

BEGIN {use_ok('qpid_proton');}
require_ok('qpid_proton');

my $array;
my $value;
my @values;
my $data;

# can create an array
lives_ok(sub {$array = qpid::proton::Array->new(0, qpid::proton::INT);},
         "Can create an array");

# cannot push onto a nil array reference
dies_ok(sub {push(undef, "foo");}, "Cannot push onto a null array");

# data can be pushed onto and popped off of an array
$value = int(rand(2**16));
$array = qpid::proton::Array->new(0, qpid::proton::INT);
push(@{$array}, $value);
ok(pop($array) == $value, "Can pop values from the array");

# normal arrays still work correctly
$array = ();
$value = int(rand(2**16));
push(@{$array}, $value);
ok(pop($array) == $value, "Can still use normal arrays");

# data from an array can be put into a data object
$data = qpid::proton::Data->new;
$array = qpid::proton::Array->new(0, qpid::proton::INT);
@values = random_integers(100);
foreach $value (@values) {
    push(@{$array}, $value);
}
$array->put($data);
$data->enter;
$success = 1;
foreach $value (@values) {
    $data->next;
    $success = ($success && ($data->get_int == $value));
}

ok($success == 1, "Array was properly marshalled into a data type");

# dies when creating an array from a null reference
dies_ok(sub {qpid::proton::Array->get(undef);},
        "Raises an error when getting from a null array");

# can create an array object from a data array
@values = random_integers(100);
$data = qpid::proton::Data->new;
$data->put_array(0, qpid::proton::INT);
$data->enter;
foreach $value (@values) {
    $data->put_int($value);
}
$data->exit;

$array = qpid::proton::Array->get($data);
isa_ok($array, "qpid::proton::Array");

is_deeply([sort @values], [sort @{$array}],
          "Arrays are properly created from a data object");
