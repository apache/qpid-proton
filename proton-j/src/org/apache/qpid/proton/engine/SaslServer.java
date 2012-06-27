package org.apache.qpid.proton.engine;

public interface SaslServer extends Sasl
{
    /** Set the outcome of SASL negotiation
     *
     * Used by the server to set the result of the negotiation process.
     *
     * @todo
     */
    void done(SaslOutcome outcome);
}
