package org.apache.qpid.proton.driver;

import java.nio.channels.SelectableChannel;
import java.nio.channels.ServerSocketChannel;

public interface Driver
{
    /** Force pn_driver_wait() to return
     *
     */
    void wakeup();

    /** Wait for an active connector or listener
     *
     * @param[in] timeout maximum time in milliseconds to wait, -1 means
     *                    infinite wait
     */
    void doWait(int timeout);

    /** Get the next listener with pending data in the driver.
     *
     * @return NULL if no active listener available
     */
    Listener listener();

    /** Get the next active connector in the driver.
     *
     * Returns the next connector with pending inbound data, available
     * capacity for outbound data, or pending tick.
     *
     * @return NULL if no active connector available
     */
    Connector connector();

    /** Destruct the driver and all associated
     *  listeners and connectors.
     */
    void destroy();



    /** Construct a listener for the given address.
     *
     * @param host local host address to listen on
     * @param port local port to listen on
     * @param context application-supplied, can be accessed via
     *                    pn_listener_context()
     * @return a new listener on the given host:port, NULL if error
     */
    <C> Listener<C> createListener(String host, int port, C context);

    /** Create a listener using the existing channel.
     *
     * @param c existing file descriptor for listener to listen on
     * @param context application-supplied, can be accessed via
     *                    pn_listener_context()
     * @return a new listener on the given channel, NULL if error
     */
    <C> Listener<C>  createListener(ServerSocketChannel c, C context);


    /** Construct a connector to the given remote address.
     *
     * @param host remote host to connect to.
     * @param port remote port to connect to.
     * @param context application supplied, can be accessed via
     *                    pn_connector_context() @return a new connector
     *                    to the given remote, or NULL on error.
     */
    <C> Connector<C> createConnector(String host, int port, C context);

    /** Create a connector using the existing file descriptor.
     *
     * @param fd existing file descriptor to use for this connector.
     * @param context application-supplied, can be accessed via
     *                    pn_connector_context()
     * @return a new connector to the given host:port, NULL if error.
     */
    <C> Connector<C> createConnector(SelectableChannel fd, C context);

    /** Set the tracing level for the given connector.
     *
     * @param trace the trace level to use.
     */
    //void pn_connector_trace(pn_connector_t *connector, pn_trace_t trace);


}
