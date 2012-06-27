package org.apache.qpid.proton.engine;

public interface SaslClient extends Sasl
{
    /** Configure the SASL layer to use the "PLAIN" mechanism.
     *
     * A utility function to configure a simple client SASL layer using
     * PLAIN authentication.
     *
     * @param username credential for the PLAIN authentication
     *                     mechanism
     * @param password credential for the PLAIN authentication
     *                     mechanism
     */
    void plain(String username, String password);

    /** Retrieve the outcome of SASL negotiation.
     *
     */
    SaslOutcome getOutcome();


}
