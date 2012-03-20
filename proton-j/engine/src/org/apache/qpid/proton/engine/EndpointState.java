package org.apache.qpid.proton.engine;

/**
 * Represents the state of a communication endpoint.
 */
public enum EndpointState
{
    UNINITIALIZED,
    ACTIVE,
    CLOSED;
}
