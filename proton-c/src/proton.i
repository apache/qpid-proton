%module proton
%{
/* Includes the header in the wrapper code */
#include <proton/engine.h>
%}

/* Parse the header file to generate wrappers */
%include "proton/engine.h"
