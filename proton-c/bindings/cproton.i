/* Parse these interface header files to generate APIs for script languages */
%include "proton/types.h"
%ignore pn_error_format;
%ignore pn_error_vformat;
%include "proton/error.h"
%include "proton/engine.h"
%include "proton/message.h"
%include "proton/sasl.h"
%include "proton/driver.h"
%include "proton/messenger.h"
