package org.apache.qpid.proton.reactor.impl;

/**
 * Thrown by the reactor when it encounters an internal error condition.
 * This is analogous to an assertion failure in the proton-c reactor
 * implementation.
 */
class ReactorInternalException extends RuntimeException {

    private static final long serialVersionUID = 8979674526584642454L;

    protected ReactorInternalException(String msg) {
        super(msg);
    }

    protected ReactorInternalException(Throwable cause) {
        super(cause);
    }

    protected ReactorInternalException(String msg, Throwable cause) {
        super(msg, cause);
    }
}
