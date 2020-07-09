## Logging {#logging}

### Backward compatible logging control

Proton has always supported some control over its internal logging using environment variables. The one that has seen
the most use is `PN_TRACE_FRM` which when set to '1', 'on' or 'true' turns on logging of protocol frame traces.
This is commonly used on a Unix shell command line like so:

    PN_TRACE_FRM=1 ./proton_program

Setting `PN_TRACE_FRM` is equivalent to using the @ref PN_LEVEL_FRAME log level setting in the proton @ref logger API.

There are several other (less used) environment variables which are supported:

* `PN_TRACE_RAW` - This turns on raw protocol frame tracing and is equivalent to @ref PN_LEVEL_RAW.
* `PN_TRACE_EVT` - This is useful for tracing events, but has no direct equivalent in the Logger API.
* `PN_TRACE_DRV` - This turns on very verbose miscellaneous tracing, again it has no direct equivalent in the Logger API.

The last two variables are still supported, but their effect may not be to turn on exactly the same logging messages as
prior to the introduction of the Logger API.

### Logger control introduced with the logger API

The @ref logger API uses a single environment variable to control the default logging state - `PN_LOG`. This can include
a number of strings which correspond to log levels to turn on, these are in descending order of importance (case is not significant):

* Error
* Warning
* Info
* Debug
* Trace

These strings can also be suffixed with '+' to mean this level and all the ones above. So specifying 'Info+' means turn on 'Error', 'Warning' and 'Info' levels.
For example:

    PN_LOG='info+' ./broker

* Frame
* Raw

These strings are equivalent to the frame and raw frame protocol traces from `PN_TRACE_FRM` and `PN_TRACE_RAW`, they will ignore any '+' appended to them. For example:

    PN_LOG='frame' ./proton_program

will have the same effect as the first example in this document.

* All

This will turn all known logging levels on. This is probably hardly ever useful as it will produce huge amounts of output. Appending '+' will again have no effect as all logging is already turned on!

Multiple specifiers can be in the string for more logging levels:

    PN_LOG='error frame' ./proton_program

this would be useful to see the frame trace logged together with any errors.


