package org.apache.qpid.proton.engine;

public interface Sasl
{
    public enum SaslState
    {
        PN_SASL_CONF,    /** Pending configuration by application */
        PN_SASL_IDLE,    /** Pending SASL Init */
        PN_SASL_STEP,    /** negotiation in progress */
        PN_SASL_PASS,    /** negotiation completed successfully */
        PN_SASL_FAIL     /** negotiation failed */
    }

    public enum SaslOutcome
    {
        PN_SASL_NONE((byte)-1),  /** negotiation not completed */
        PN_SASL_OK((byte)0),     /** authentication succeeded */
        PN_SASL_AUTH((byte)1),   /** failed due to bad credentials */
        PN_SASL_SYS((byte)2),    /** failed due to a system error */
        PN_SASL_PERM((byte)3),   /** failed due to unrecoverable error */
        PN_SASL_TEMP((byte)4);

        private final byte _code;

        /** failed due to transient error */

        SaslOutcome(byte code)
        {
            _code = code;
        }

        public byte getCode()
        {
            return _code;
        }
    }

    public int END_OF_STREAM = -1;
    /**
     * @param bytes input bytes for consumption
     * @param offset the offset within bytes where input begins
     * @param size the number of bytes available for input
     *
     * @return the number of bytes consumed
     */
    public int input(byte[] bytes, int offset, int size);

    /**
     * @param bytes array for output bytes
     * @param offset the offset within bytes where output begins
     * @param size the number of bytes available for output
     *
     * @return the number of bytes written
     */
    public int output(byte[] bytes, int offset, int size);

    /** Access the current state of the layer.
     *
     * @return The state of the sasl layer.
     */
    SaslState getState();

    /** Set the acceptable SASL mechanisms for the layer.
     *
     * @param mechanisms a list of acceptable SASL mechanisms
     */
    void setMechanisms(String[] mechanisms);

    /** Retrieve the list of SASL mechanisms provided by the remote.
     *
     * @return the SASL mechanisms advertised by the remote
     */
    String[] getRemoteMechanisms();

    /** Determine the size of the bytes available via recv().
     *
     * Returns the size in bytes available via recv().
     *
     * @return The number of bytes available, zero if no available data.
     */
    int pending();

    /** Read challenge/response data sent from the peer.
     *
     * Use pending to determine the size of the data.
     *
     * @param bytes written with up to size bytes of inbound data.
     * @param offset the offset in the array to begin writing at
     * @param size maximum number of bytes that bytes can accept.
     * @return The number of bytes written to bytes, or an error code if < 0.
     */
    int recv(byte[] bytes, int offset, int size);

    /** Send challenge or response data to the peer.
     *
     * @param bytes The challenge/response data.
     * @param offset the point within the array at which the data starts at
     * @param size The number of data octets in bytes.
     * @return The number of octets read from bytes, or an error code if < 0
     */
    int send(byte[] bytes, int offset, int size);


    /** Set the outcome of SASL negotiation
     *
     * Used by the server to set the result of the negotiation process.
     *
     * @todo
     */
    void done(SaslOutcome outcome);


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
