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

qpid::proton;

=head1 DESCRIPTION

=cut

package qpid::proton;

=pod

=head1 MOVING DATA OUT OF A DATA OBJECT

=over

=item qpid::proton::put_array_into( [DATA], [TYPE], [ELEMENTS], [DESCRIBED], [DESCRIPTOR] );

Puts the specified elements into the I<qpid::proton::Data> object specified
using the specified B<type> value. If the array is described (def. undescribed)
then the supplied B<descriptor> is used.

=item ($described, $type, @elements) = qpid::proton::get_array_from( [DATA] );

=item ($described, $descriptor, $type, @elements) = qpid::proton::get_array_from( [DATA] );

Retrieves the descriptor, size, type and elements for an array from the
specified instance of I<qpid::proton::Data>.

If the array is B<described> then the I<descriptor> for the array is returned as well.

=item @elements = qpid::proton::get_list_from( [DATA] );

Retrieves the elements for a list from the specified instance of
I<qpid::proton::Data>.

=back

=cut

sub put_array_into {
    my $data = $_[0];
    my $type = $_[1];
    my ($values) = $_[2];
    my $described = $_[3] || 0;
    my $descriptor = $_[4];

    die "data cannot be nil" if !defined($data);
    die "type cannot be nil" if !defined($type);
    die "values cannot be nil" if !defined($values);
    die "descriptor cannot be nil" if $described && !defined($descriptor);

    $data->put_array($described, $type);
    $data->enter;

    if ($described && defined($descriptor)) {
        $data->put_symbol($descriptor);
    }

    foreach $value (@{$values}) {
        $type->put($data, $value);
    }
    $data->exit;
}

sub get_array_from {
    my $data = $_[0];

    die "data cannot be nil" if !defined($data);

    # ensure we're actually on an array
    my $type = $data->get_type;

    die "current node is not an array" if !defined($type) ||
        !($type == qpid::proton::ARRAY);

    my ($count, $described, $rtype) = $data->get_array;
    my @elements = ();

    $data->enter;

    if (defined($described) && $described) {
        $data->next;
        $descriptor = $data->get_symbol;
    }

    for ($i = 0; $i < $count; $i++) {
        $data->next;
        my $type    = $data->get_type;
        my $element = $type->get($data);
        push(@elements, $element);
    }

    $data->exit;

    if (defined($described) && $described) {
        return ($described, $descriptor, $rtype, @elements) if $described;
    } else {
        return ($described, $rtype, @elements);
    }
}

sub get_list_from {
    my $data = $_[0];

    die "data can not be nil" if !defined($data);

    # ensure we're actually on a list
    my $type = $data->get_type;

    die "current node is not a list" if !defined($type) ||
        !($type == qpid::proton::LIST);

    my $count = $data->get_list;
    $data->enter;
    for($i = 0; $i < $count; $i++) {
        $data->next;
        my $type = $data->get_type;
        my $element = $type->get($data);
        push(@elements, $element);
    }

    return @elements;
}

1;
