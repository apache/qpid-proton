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

use strict;
use warnings;
use cproton_perl;
use Switch;

use feature qw(switch);

package qpid::proton;

sub check_for_error {
    my $rc = $_[0];

    switch($rc) {
            case 'qpid::proton::Errors::NONE' {next;}
            case 'qpid::proton::Errors::EOS' {next;}
            case 'qpid::proton::Errors::ERROR' {next;}
            case 'qpid::proton::Errors::OVERFLOW' {next;}
            case 'qpid::proton::Errors::UNDERFLOW' {next;}
            case 'qpid::proton::Errors::STATE' {next;}
            case 'qpid::proton::Errors::ARGUMENT' {next;}
            case 'qpid::proton::Errors::TIMEOUT' {next;}
            case 'qpid::proton::Errors::INTERRUPTED' {
                my $source = $_[1];
                my $trace = Devel::StackTrace->new;

                print $trace->as_string;
                die "ERROR[$rc]" . $source->get_error() . "\n";
            }
    }
}

package qpid::proton::ExceptionHandling;

1;
