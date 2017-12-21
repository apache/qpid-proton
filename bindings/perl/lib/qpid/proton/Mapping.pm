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

use qpid_proton;

package qpid::proton::Mapping;

our %by_type_value = ();

sub new {
    my ($class) = @_;
    my ($self) = {};

    my $name       = $_[1];
    my $type_value = $_[2];
    my $set_method = $_[3];
    my $get_method = $_[4];

    $self->{_name}       = $name;
    $self->{_type_value} = $type_value;
    $self->{_set_method} = $set_method;
    $self->{_get_method} = $get_method;

    bless $self, $class;

    $qpid::proton::Mapping::by_type_value{$type_value} = $self;

    return $self;
}

use overload (
    '""' => \& stringify,
    '==' => \& equals,
    );

sub stringify {
    my ($self) = @_;
    return $self->{_name};
}

sub equals {
    my ($self) = @_;
    my $that = $_[1];

    return ($self->get_type_value == $that->get_type_value);
}

sub getter_method {
    my ($self) = @_;

    return $self->{_get_method};
}

sub get_type_value {
    my ($self) = @_;
    my $type_value = $self->{_type_value};

    return $self->{_type_value};
}

=pod

=head1 MARSHALLING DATA

I<Mapping> can move data automatically into and out of a I<Data> object.

=over

=item $mapping->put( [DATA], [VALUE] );

=item $mapping->get( [DATA] );

=back

=cut

sub put {
    my ($self) = @_;
    my $data = $_[1];
    my $value = $_[2];
    my $setter_method = $self->{_set_method};

    $data->$setter_method($value);
}

sub get {
    my ($self) = @_;
    my $data = $_[1];
    my $getter_method = $self->{_get_method};

    my $result = $data->$getter_method;

    return $result;
}

sub find_by_type_value {
    my $type_value = $_[1];

    return undef if !defined($type_value);

    return $qpid::proton::Mapping::by_type_value{$type_value};
}

1;

