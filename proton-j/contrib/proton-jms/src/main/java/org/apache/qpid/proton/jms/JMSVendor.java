package org.apache.qpid.proton.jms;

import javax.jms.*;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * @author <a href="http://hiramchirino.com">Hiram Chirino</a>
 */
abstract public class JMSVendor {

    public abstract BytesMessage createBytesMessage();

    public abstract StreamMessage createStreamMessage();

    public abstract Message createMessage();

    public abstract TextMessage createTextMessage();

    public abstract ObjectMessage createObjectMessage();

    public abstract MapMessage createMapMessage();

    public abstract void setJMSXUserID(Message msg, String value);

    @Deprecated
    public Destination createDestination(String name) {
        return null;
    }

    @SuppressWarnings("deprecation")
    public <T extends Destination> T createDestination(String name, Class<T> kind) {
        return kind.cast(createDestination(name));
    }

    public abstract void setJMSXGroupID(Message msg, String groupId);

    public abstract void setJMSXGroupSequence(Message msg, int i);

    public abstract void setJMSXDeliveryCount(Message rc, long l);

    public abstract String toAddress(Destination msgDestination);

}
