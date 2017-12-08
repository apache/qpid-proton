This is a tool for looking at the ELF symbols exported by a C/C++ shared library.

Currently it has some rough edges, but it will take a file of expected symbols and
tell you the difference from what is expected.

Currently you also need to compile the C++ programs in the same directory to support the script

Besides the compiled programs in this directory it also relies on GNU nm to extract the symbols
and some standard POSIX utilities for text manipulation.