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

my $data;
my $value;

# Create without capacity
$data = qpid::proton::Data->new();
isa_ok($data, 'qpid::proton::Data');

# Create with capacity
$data = qpid::proton::Data->new(24);
isa_ok($data, 'qpid::proton::Data');

# can put a null
$data = qpid::proton::Data->new();
$data->put_null();
ok($data->is_null(), "Data can put a null");

# raises an error on a null boolean
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_bool;}, "Cannot put a null bool");

# can put a true boolean
$data = qpid::proton::Data->new();
$data->put_bool(1);
ok($data->get_bool(), "Data can put a true bool");

# can put a false boolean
$data = qpid::proton::Data->new();
$data->put_bool(0);
ok(!$data->get_bool(), "Data can put a false bool");

# raises an error on a negative ubyte
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_ubyte(0 - (rand(2**7) + 1));},
        "Cannot have a negative ubyte");

# raises an error on a null ubyte
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_ubyte;}, "Cannot put a null ubyte");

# can put a zero ubyte
$data = qpid::proton::Data->new();
$data->put_ubyte(0);
ok($data->get_ubyte() == 0, "Can put a zero ubyte");

# will convert a float to an int ubyte
$data = qpid::proton::Data->new();
$value = rand(2**7) + 1;
$data->put_ubyte($value);
ok ($data->get_ubyte() == int($value), "Can put a float ubyte");

# can put a ubyte
$data = qpid::proton::Data->new();
$value = int(rand(2**7) + 1);
$data->put_ubyte($value);
ok($data->get_ubyte() == $value, "Can put a ubyte");

# raises an error on a null byte
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_byte;}, "Cannot put a null byte");

# can put a negative byte
$data = qpid::proton::Data->new();
$value = int(0 - (1 + rand(2**7)));
$data->put_byte($value);
ok($data->get_byte() == $value, "Can put a negative byte");

# can put a zero byte
$data = qpid::proton::Data->new();
$data->put_byte(0);
ok($data->get_byte() == 0, "Can put a zero byte");

# can put a float as a byte
$data = qpid::proton::Data->new();
$value = rand(2**7) + 1;
$data->put_byte($value);
ok($data->get_byte() == int($value), "Can put a float as a byte");

# can put a byte
$data = qpid::proton::Data->new();
$value = int(1 + rand(2**7));
$data->put_byte($value);
ok($data->get_byte() == $value, "Can put a byte");

# raise an error on a null ushort
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_ushort;}, "Cannot put a null ushort");

# raises an error on a negative ushort
$data = qpid::proton::Data->new();
$value = 0 - (1 + rand((2**15)));
dies_ok(sub {$data->put_ushort($value);}, "Cannot put a negative ushort");

# can put a zero ushort
$data = qpid::proton::Data->new();
$data->put_ushort(0);
ok($data->get_ushort() == 0, "Can put a zero ushort");

# can handle a float ushort value
$data = qpid::proton::Data->new();
$value = 1 + rand((2**15));
$data->put_ushort($value);
ok($data->get_ushort() == int($value), "Can put a float ushort");

# can put a ushort
$data = qpid::proton::Data->new();
$value = int(1 + rand((2**15)));
$data->put_ushort($value);
ok($data->get_ushort() == $value, "Can put a ushort");

# raises an error on a null short
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_short;}, "Cannot put a null short");

# can put a negative short
$data = qpid::proton::Data->new();
$value = int(0 - (1 + rand((2**15))));
$data->put_short($value);
ok($data->get_short() == $value, "Can put a negative short");

# can put a zero short
$data = qpid::proton::Data->new();
$data->put_short(0);
ok($data->get_short() == 0, "Can put a zero short");

# can put a float as a short
$data = qpid::proton::Data->new();
$value = 1 + rand(2**15);
$data->put_short($value);
ok($data->get_short() == int($value), "Can put a float as a short");

# can put a short
$data = qpid::proton::Data->new();
$value = int(1 + rand(2**15));
$data->put_short($value);
ok($data->get_short() == $value, "Can put a short");

# raises an error on a null uint
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_uint;}, "Cannot set a null uint");

# raises an error on a negative uint
$data = qpid::proton::Data->new();
$value = 0 - (1 + rand(2**31));
dies_ok(sub {$data->put_uint($value);}, "Cannot set a negative uint");

# can put a zero uint
$data = qpid::proton::Data->new();
$data->put_uint(0);
ok($data->get_uint() == 0, "Can put a zero uint");

# can put a float as a uint
$data = qpid::proton::Data->new();
$value = 1 + rand(2**31);
$data->put_uint($value);
ok($data->get_uint() == int($value), "Can put a float as a uint");

# can put a uint
$data = qpid::proton::Data->new();
$value = int(1 + rand(2**31));
$data->put_uint($value);
ok($data->get_uint() == $value, "Can put a uint");

# raise an error on a null integer
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_int;}, "Cannot put a null int");

# can put a negative integer
$data = qpid::proton::Data->new();
$value = int(0 - (1 + rand(2**31)));
$data->put_int($value);
ok($data->get_int() == $value, "Can put a negative int");

# can put a zero integer
$data = qpid::proton::Data->new();
$data->put_int(0);
ok($data->get_int() == 0, "Can put a zero int");

# can put a float as an integer
$data = qpid::proton::Data->new();
$value = 1 + (rand(2**31));
$data->put_int($value);
ok($data->get_int() == int($value), "Can put a float as an int");

# can put an integer
$data = qpid::proton::Data->new();
$value = int(1 + rand(2**31));
$data->put_int($value);
ok($data->get_int() == $value, "Can put an int");

# raises an error on a null character
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_char;}, "Cannot put a null char");

# can put a float as a char
$data = qpid::proton::Data->new();
$value = 1 + rand(255);
$data->put_char($value);
ok($data->get_char() == int($value), "Can put a float as a char");

# can put a character
$data = qpid::proton::Data->new();
$value = int(1 + rand(255));
$data->put_char($value);
ok($data->get_char() == $value, "Can put a char");

# raise an error on a null ulong
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_ulong;}, "Cannot put a null ulong");

# raises an error on a negative ulong
$data = qpid::proton::Data->new();
$value = 0 - (1 + rand(2**63));
dies_ok(sub {$data->put_ulong($value);}, "Cannot put a negative ulong");

# can put a zero ulong
$data = qpid::proton::Data->new();
$data->put_ulong(0);
ok($data->get_ulong() == 0, "Can put a zero ulong");

# can put a float as a ulong
$data = qpid::proton::Data->new();
$value = 1 + rand(2**63);
$data->put_ulong($value);
ok($data->get_ulong() == int($value), "Can put a float as a ulong");

# can put a ulong
$data = qpid::proton::Data->new();
$value = int(1 + rand(2**63));
$data->put_ulong($value);
ok($data->get_ulong() == $value, "Can put a ulong");

# raises an error on a null long
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_long;}, "Cannot put a null long");

# can put a negative long
$data = qpid::proton::Data->new();
$value = int(0 - (1 + rand(2**63)));
$data->put_long($value);
ok($data->get_long() == $value, "Can put a negative long");

# can put a zero long
$data = qpid::proton::Data->new();
$data->put_long(0);
ok($data->get_long() == 0, "Can put a zero long");

# can put a float as a long
$data = qpid::proton::Data->new();
$value = 1 + rand(2**63);
$data->put_long($value);
ok($data->get_long() == int($value), "Can put a float as a long");

# can put a long
$data = qpid::proton::Data->new();
$value = int(1 + rand(2**63));
$data->put_long($value);
ok($data->get_long() == $value, "Can put a long value");

# raises an error on a null timestamp
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_timestamp;}, "Cannot put a null timestamp");

# can put a negative timestamp
$data = qpid::proton::Data->new();
$value = int(0 - (1 + rand(2**32)));
$data->put_timestamp($value);
ok($data->get_timestamp() == $value, "Can put a negative timestamp");

# can put a zero timestamp
$data = qpid::proton::Data->new();
$data->put_timestamp(0);
ok($data->get_timestamp() == 0, "Can put a zero timestamp");

# can put a float as a timestamp
$data = qpid::proton::Data->new();
$value = 1 + (rand(2**32));
$data->put_timestamp($value);
ok($data->get_timestamp() == int($value), "Can put a float as a timestamp");

# can put a timestamp
$data = qpid::proton::Data->new();
$value = int(1 + rand(2**32));
$data->put_timestamp($value);
ok($data->get_timestamp() == $value, "Can put a timestamp");

# raises an error on a null float
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_float;}, "Cannot put a null float");

# can put a negative float
$data = qpid::proton::Data->new();
$value = 0 - (1 + rand(2**15));
$data->put_float($value);
delta_ok($data->get_float(), $value, "Can put a negative float");

# can put a zero float
$data = qpid::proton::Data->new();
$data->put_float(0.0);
delta_ok($data->get_float(), 0.0, "Can put a zero float");

# can put a float
$data = qpid::proton::Data->new();
$value = 1.0 + rand(2**15);
$data->put_float($value);
delta_ok($data->get_float(), $value, "Can put a float");

# raises an error on a null double
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_double;}, "Cannot set a null double");

# can put a negative double
$data = qpid::proton::Data->new();
$value = 0 - (1 + rand(2**31));
$data->put_double($value);
delta_ok($data->get_double(), $value, "Can put a double value");

# can put a zero double
$data = qpid::proton::Data->new();
$data->put_double(0.0);
delta_ok($data->get_double(), 0.0, "Can put a zero double");

# can put a double
$data = qpid::proton::Data->new();
$value = 1.0 + rand(2**15);
$data->put_double($value);
delta_ok($data->get_double(), $value, "Can put a double");

# raises an error on a null decimal32
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_decimal32;}, "Cannot put a null decimal32");

# can put a decimal32
$data = qpid::proton::Data->new();
$value = int(rand(2**32));
$data->put_decimal32($value);
ok($data->get_decimal32() == $value, "Can put a decimal32 value");

# raises an error on a null decimal64
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_decimal64();}, "Cannot put a null decimal64");

# can put a decimal64
$data = qpid::proton::Data->new();
$value = int(rand(2**64));
$data->put_decimal64($value);
ok($data->get_decimal64() == $value, "Can put a decimal64 value");

# raises an error on a null decimal128
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_decimal128;}, "Cannot put a null decimal128");

# can put a decimal128
$data = qpid::proton::Data->new();
$value = int(rand(2**31));
$data->put_decimal128($value);
ok($data->get_decimal128() == $value, "Can put a decimal128 value");

# raises an error on a null UUID
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_uuid;}, "Cannot put a null UUID");

# raises an error on a malformed UUID
$data = qpid::proton::Data->new();
$value = random_string(36);
dies_ok(sub {$data->put_uuid($value);}, "Cannot put a malformed UUID");

# can put a UUID
$data = qpid::proton::Data->new();
$data->put_uuid("fd0289a5-8eec-4a08-9283-81d02c9d2fff");
ok($data->get_uuid() eq "fd0289a5-8eec-4a08-9283-81d02c9d2fff",
   "Can store a string UUID");

# cannot put a null binary
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_binary;}, "Cannot put a null binary");

# can put an empty binary string
$data = qpid::proton::Data->new();
$data->put_binary("");
ok($data->get_binary() eq "", "Can put an empty binary");

# can put a binary
$data = qpid::proton::Data->new();
$value = random_string(128);
$data->put_binary($value);
ok($data->get_binary() eq $value, "Can put a binary value");

# cannot put a null string
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_string;}, "Cannot put a null string");

# can put an empty string
$data = qpid::proton::Data->new();
$data->put_string("");
ok($data->get_string() eq "", "Can put an empty string");

# can put a string
$data = qpid::proton::Data->new();
$value = random_string(128);
$data->put_string($value);
ok($data->get_string() eq $value, "Can put an arbitrary string");

# cannot put a null symbol
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_symbol;}, "Cannot put a null symbol");

# can put a symbol
$data = qpid::proton::Data->new();
$value = random_string(64);
$data->put_symbol($value);
ok($data->get_symbol eq $value, "Can put a symbol");

# can hold a described value
$data = qpid::proton::Data->new();
$data->put_described;
ok($data->is_described, "Can hold a described value");

# can put an array with undef as described flag
$data = qpid::proton::Data->new();
my @values = map { rand } (1..100, );
lives_ok(sub {$data->put_array(undef, qpid::proton::INT);},
         "Array can have null for described flag");

# arrays must have a specified type
$data = qpid::proton::Data->new();
dies_ok(sub {$data->put_array;},
        "Array type cannot be null");

# can put an array
$data = qpid::proton::Data->new();
@values = random_integers(100);
$data->put_array(0, qpid::proton::INT);
$data->enter;
foreach $value (@values) {
    $data->put_int($value);
}
$data->exit;

@result = ();
$data->enter;
foreach $value (@values) {
    $data->next;
    push @result, $data->get_int;
}
$data->exit;
is_deeply((\@result, \@values), "Array was populated correctly");

# can put a described array
$data = qpid::proton::Data->new();
@values = random_integers(100);
$data->put_array(1, qpid::proton::INT);
$data->enter;
foreach $value (@values) {
    $data->put_int($value);
}
$data->exit;

@result = ();
$data->enter;
foreach $value (@values) {
    $data->next;
    push @result, $data->get_int;
}
is_deeply((\@result, \@values), "Array was populated correctly");

# can put a list
$data = qpid::proton::Data->new();
@values = random_integers(100);
$data->put_list;
$data->enter;
foreach $value (@values) {
    $data->put_int($value);
}
$data->exit;

@result = ();
$data->enter;
foreach $value (@values) {
    $data->next;
    push @result, $data->get_int;
}
$data->exit;
is_deeply((\@result, \@values), "List was populated correctly");


# can put a map
$data = qpid::proton::Data->new();
my $map = random_hash(100);
$data->put_map;
$data->enter;
foreach my $key (keys %{$map}) {
    $data->put_string($key);
    $data->put_string($map->{$key});
}
$data->exit;

my $result = {};
$data->enter;
foreach my $key (keys %{$map}) {
    $data->next;
    my $rkey = $data->get_string;
    $data->next;
    my $rval = $data->get_string;
    $result{$rkey} = $rval;
}
$data->exit;
ok(eq_hash(\%result, \%{$map}), "Map was populated correctly");
