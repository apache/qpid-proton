package org.apache.qpid.proton.driver;

import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Sasl;

public interface Connector<C>
{
    /** Service the given connector.
         *
         * Handle any inbound data, outbound data, or timing events pending on
         * the connector.
         *
         */
        void process();

        /** Access the listener which opened this connector.
         *
         * @return the listener which created this connector, or NULL if the
         *         connector has no listener (e.g. an outbound client
         *         connection)
         */
        Listener listener();

        /** Access the Authentication and Security context of the connector.
         *
         * @return the Authentication and Security context for the connector,
         *         or NULL if none
         */
        Sasl sasl();

        /** Access the AMQP Connection associated with the connector.
         *
         * @return the connection context for the connector, or NULL if none
         */
        Connection getConnection();

        /** Assign the AMQP Connection associated with the connector.
         *
         * @param connection the connection to associate with the
         *                       connector
         */
        void setConnection(Connection connection);

        /** Access the application context that is associated with the
         *  connector.
         *
         * @return the application context that was passed to pn_connector()
         *         or pn_connector_fd()
         */
        C getContext();

        /** Assign a new application context to the connector.
         *
         * @param[in] context new application context to associate with the
         *                    connector
         */
        void setContext(C context);

        /** Close the socket used by the connector.
         *
        */
        void close();

        /** Determine if the connector is closed.
         *
         * @return True if closed, otherwise false
         */
        boolean isClosed();

        /** Destructor for the given connector.
         *
         * Assumes the connector's socket has been closed prior to call.
         *
         */
        void destroy();

}
