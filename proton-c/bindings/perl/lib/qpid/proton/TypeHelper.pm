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

package qpid::proton::TypeHelper;

our %by_type_value = ();

sub new {
    my ($class) = @_;
    my ($self) = {};

    my $type_value = $_[1];
    my $set_method = $_[2];
    my $get_method = $_[3];

    $self->{_type_value} = $type_value;
    $self->{_set_method} = $set_method;
    $self->{_get_method} = $get_method;

    bless $self, $class;

    $qpid::proton::TypeHelper::by_type_value{$type_value} = $self;

   return $self;
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

sub put {
    my ($self) = @_;
    my $data = $_[1];
    my $described = $_[2];
    my $elements = $_[3];
    my $array_type = $self->{_type_value};
    my $setter_method = $self->{_set_method};

    $data->put_array($described,
                     qpid::proton::TypeHelper->find_by_type_value($array_type));
    $data->enter;
    foreach $value (@${elements}) {
        $data->$setter_method($value);
    }
    $data->exit;
}

sub get {
    my $data = $_[1];

    my ($size, $described, $type_value) = $data->get_array;
    my $get_method = $type_value->getter_method;
    my $result = qpid::proton::Array->new($described, $type_value);

    $data->enter;
    while($data->next) {
        my $next_value = $data->$get_method();
        $result->push($next_value);
    }
    $data->exit;

    return $result;
}

sub find_by_type_value {
    my $type_value = $_[1];

    return undef if !defined($type_value);

    return $qpid::proton::TypeHelper::by_type_value{$type_value};
}

1;

