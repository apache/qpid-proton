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

=head2 MAPS

Moving values from a map within a B<qpid::proton::Data> object into a
Perl B<Hash> object is done using the following:

=over

=item %hash = qpid::proton::get_map_from( [DATA] );

=back

=cut

sub get_map_from {
    my $data = $_[0];

    die "data cannot be nil" unless defined($data);

    my $type = $data->get_type;

    die "current node is not a map" if !($type == qpid::proton::MAP);

    my $result;
    my $count = $data->get_map;

    $data->enter;
    for($i = 0; $i < $count/2; $i++) {
        $data->next;
        my $type = $data->get_type;
        my $key = $type->get($data);
        $data->next;
        $type = $data->get_type;
        my $value = $type->get($data);
        $result{$key} = $value;
    }
    $data->exit;

    return $result;
}

1;
