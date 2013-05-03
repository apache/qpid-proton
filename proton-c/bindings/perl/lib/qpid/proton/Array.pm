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

=pod

=head1 NAME

qpid::proton::Array;

=head1 DESCRIPTION

A convenience class for adding the elements of an array to a
B<qpid::proton::Data> instance.

=cut

package qpid::proton::Array;

=pod

=head1 CONSTRUCTOR

Creates a new B<Array> type that can be used as a drop-in replacement for
standard Perl arrays.

=cut

sub new {
    my ($class) = @_;
    my ($self) = {};
    my $described = $_[1];
    my $array_type = $_[2];

    die "the array must declare a type." if !defined($array_type);
    unless (UNIVERSAL::isa($array_type, "qpid::proton::TypeHelper")) {
        die "invalid type argument: $array_type";
    }

    $self->{_described} = $described;
    $self->{_type} = $array_type;

    # stores the actual elements that are added
    my @elements = ();

    $self->{_elements} = \@elements;

    bless $self, $class;
    return $self;
}

use overload (
    '@{}' => \&arrayify,
    );

sub arrayify {
    my ($self) = @_;
    my $elements = $self->{_elements};

    return \@{$elements};
}

sub push {
    my ($self) = @_;
    my ($elements) = $self->{_elements};
    my $value = $_[1];

    CORE::push(@{$elements}, $value);
}

sub pop {
    my ($self) = @_;
    my ($elements) = $self->{_elements};

    my $value = CORE::pop(@{$elements});

    return $value;
}

sub get_type {
    my ($self) = @_;
    my $array_type = $self->{_type};

    return $array_type;
}

=pod

=head1 MOVING INTO AND OUT OF A DATA OBJECT

Data can be moved into and out of a B<qpid::proton::Data> object using the
I<put> and I<get> methods.

=over

=item $array->put( DATA );

Copies the elements from the array into the specified B<DATA> instance. If
the B<qpid::proton::Array> was described then the array created will also
be described.

=back

=cut

sub put {
    my ($self) = @_;
    my $data = $_[1];
    my $elements = $self->{_elements};
    my $described = $self->{_described} || 0;
    my $array_type = $self->{_type};

    $array_type->put($data, $described, $elements);
}

=pod

=over

=item $data = $array->get( ARRAY );

Creates a new B<qpid::proton::Data> and populates it with an array that contains
the elements from the provided B<qpid::proton::Array>.

=back

=cut

sub count {
    my ($self) = @_;
    my ($elements) = $self->{_elements};
}

sub get {
    my $data = $_[1];

    die "data type undefined" if !defined($data);

    my $result = qpid::proton::TypeHelper->get($data);

    $result->count;

    return $result;
}

package main;

# redefine the push and pop methods to work with our Array type
BEGIN {
    sub proton_array_push {
        my $array = $_[0];
        my $value = $_[1];

        if(UNIVERSAL::isa($array, "qpid::proton::Array")) {
            $array->push($value);
        } else {
            CORE::push(@{$array}, $value);
        };
    };

    sub proton_array_pop {
        my $array = $_[0];

        if(UNIVERSAL::isa($array, "qpid::proton::Array")) {
            return $array->pop;
        } else {
            return CORE::pop(@{$array});
        }
    };

    *CORE::GLOBAL::push = \proton_array_push;
    *CORE::GLOBAL::pop  = \&proton_array_pop;
}

1;
