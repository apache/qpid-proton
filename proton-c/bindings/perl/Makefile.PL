#!/usr/bin/perl -w

use ExtUtils::MakeMaker;

WriteMakefile(
    NAME         => 'qpid::cproton',
    DISTNAME     => 'perl-qpid_proton',
    VERSION      => '0.3',
    PREREQ_PM    => {},
    LIBS         => ["-lqpid-proton"],
    C            => ['cproton_perl.c'],
);
