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
use Test::Exception;

require 'utils.pm';

BEGIN {use_ok('qpid_proton');}
require_ok('qpid_proton');

my $data;
my @values;
my $result;
my $length;
my $descriptor;

#=============================================================================
# Getting an array from a nil Data instance raises an error.
#=============================================================================
$data = qpid::proton::Data->new;
dies_ok(sub {qpid::proton::get_array_from(undef);},
        "Raise an exception when getting from a nil Data object");


#=============================================================================
# Getting an array fails if the current node is not an array or a list.
#=============================================================================
$data = qpid::proton::Data->new;
$data->put_string("foo");
$data->rewind;
$data->next;
dies_ok(sub {qpid::proton::proton_get_array_from($data, undef);},
        "Raise an exception when getting from a non-list and non-array");


#=============================================================================
# Can get an undescribed array.
#=============================================================================
$length = int(rand(256) + 64);
$data = qpid::proton::Data->new;
@values= random_integers($length);
$data->put_array(0, qpid::proton::INT);
$data->enter;
foreach $value (@values) {
    $data->put_int($value);
}
$data->exit;
$data->rewind;

{
    $data->next;
    my ($described, $type, @results) = qpid::proton::get_array_from($data);

    ok(!$described, "Returned an undescribed array");
    ok($type == qpid::proton::INT, "Returned the correct array type");
    ok(scalar(@results) == $length, "Returns the correct number of elements");

    is_deeply([sort @results], [sort @values],
              "Returned the correct set of values");
}


#=============================================================================
# Raises an error when putting into a null Data object.
#=============================================================================
dies_ok(sub {qpid::proton::put_array_into(undef, qpid::proton::INT,  @values);},
        "Raises an error when putting into a null Data object");


#=============================================================================
# Raises an error when putting a null type into a Data object.
#=============================================================================
$data = qpid::proton::Data->new;
dies_ok(sub {qpid::proton::put_array_into($data, undef,  @values);},
        "Raises an error when putting into a null Data object");


#=============================================================================
# Raises an error when putting a null array into a Data object.
#=============================================================================
$data = qpid::proton::Data->new;
dies_ok(sub {qpid::proton::put_array_into($data, qpid::proton::INT);},
        "Raises an error when putting into a null Data object");


#=============================================================================
# Raises an error when putting a described array with no descriptor.
#=============================================================================
$data = qpid::proton::Data->new;
dies_ok(sub {qpid::proton::put_array_into($data, qpid::proton::INT, \@values, 1);},
        "Raises an error when putting a described array with no descriptor");


#=============================================================================
# Can put an undescribed array into a Data object.
#=============================================================================
$length = int(rand(256) + 64);
$data = qpid::proton::Data->new;
@values= random_integers($length);
qpid::proton::put_array_into($data, qpid::proton::INT, \@values, 0);
$data->rewind;

{
    $data->next;
    my ($described, $type, @results) = qpid::proton::get_array_from($data);

    ok(!$described, "Put an undescribed array");
    ok($type == qpid::proton::INT, "Put the correct array type");
    ok(scalar(@results) == $length, "Put the correct number of elements");

    is_deeply([sort @results], [sort @values],
              "Returned the correct set of values");
}


#=============================================================================
# Can get an described array.
#=============================================================================
$length = int(rand(256) + 64);
$data = qpid::proton::Data->new;
@values= random_strings($length);
$descriptor = random_string(64);
$data->put_array(1, qpid::proton::STRING);
$data->enter;
$data->put_symbol($descriptor);
foreach $value (@values) {
    $data->put_string($value);
}

$data->exit;
$data->rewind;

{
    $data->next;
    my ($described, $dtor, $type, @results) = qpid::proton::get_array_from($data);

    ok($described, "Returned a described array");
    ok($dtor eq $descriptor, "Returned the correct descriptor");
    ok($type == qpid::proton::STRING, "Returned the correct array type");
    ok(scalar(@results) == $length, "Returns the correct number of elements");

    is_deeply([sort @results], [sort @values],
              "Returned the correct set of values");
}


#=============================================================================
# Can put a described array into a Data object.
#=============================================================================
$length = int(rand(256) + 64);
$data = qpid::proton::Data->new;
@values= random_integers($length);
$descriptor = random_string(128);
qpid::proton::put_array_into($data, qpid::proton::INT, \@values, 1, $descriptor);
$data->rewind;

{
    $data->next;
    my ($described, $dtor, $type, @results) = qpid::proton::get_array_from($data);

    ok($described, "Put a described array");
    ok($dtor eq $descriptor, "Put the correct descriptor");
    ok($type == qpid::proton::INT, "Put the correct array type");
    ok(scalar(@results) == $length, "Put the correct number of elements");

    is_deeply([sort @results], [sort @values],
              "Returned the correct set of values");
}


#=============================================================================
# Raises an error when getting a list from a null Data instance
#=============================================================================
$data = qpid::proton::Data->new;
dies_ok(sub {qpid::proton::get_list_from(undef);},
        "Raises error when getting list from null Data object");


#=============================================================================
# Raises an error when the current node is not a list.
#=============================================================================
$data = qpid::proton::Data->new;
$data->put_string(random_string(64));
$data->rewind;
$data->next;

dies_ok(sub {qpid::proton::get_list_from($data);},
        "Raises an error when getting a list and it's not currently a list.");


#=============================================================================
# Can get an array
#=============================================================================
$length = int(rand(256) + 64);
$data = qpid::proton::Data->new;
@values = random_strings($length);
$data->put_list;
$data->enter;
foreach $value (@values) {
    $data->put_string($value);
}
$data->exit;
$data->rewind;

{
    my $result = $data->next;

    my @results = qpid::proton::get_list_from($data);

    ok(scalar(@results) == $length, "Returned the correct number of elements");

    is_deeply([sort @results], [sort @values],
              "Returned the correct list of values");
}
