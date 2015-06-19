# Notes for example developers

Use the C++ std library/boost conventions. File names are (*.hpp, *.cpp) and
identifiers are lowercase_underscore names not CamelCase.

No "using namespace proton" in examples. This is not a general rule, but for
_example_ code the explicit `proton::` qualifier makes it easier to see what is
part of the proton library vs. standard library or code that is just part of the
example.
