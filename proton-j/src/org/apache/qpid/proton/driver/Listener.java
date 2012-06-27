package org.apache.qpid.proton.driver;

public interface Listener<C>
{
    /** pn_listener - the server API **/


    /**
     * @todo pn_listener_trace needs documentation
     */
    //void pn_listener_trace(pn_listener_t *listener, pn_trace_t trace);

    /** Accept a connection that is pending on the listener.
     *
     * @param[in] listener the listener to accept the connection on
     * @return a new connector for the remote, or NULL on error
     */
    Connector accept();

    /** Access the application context that is associated with the listener.
     *
     * @param[in] listener the listener whose context is to be returned
     * @return the application context that was passed to pn_listener() or
     *         pn_listener_fd()
     */
    C getContext();

    /** Close the socket used by the listener.
     *
     * @param[in] listener the listener whose socket will be closed.
     */
    void close();

    /** Destructor for the given listener.
     *
     * Assumes the listener's socket has been closed prior to call.
     *
     * @param[in] listener the listener object to destroy, no longer valid
     *            on return
     */
    void destroy();


}
