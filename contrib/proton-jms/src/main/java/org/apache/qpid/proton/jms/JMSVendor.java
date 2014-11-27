package org.apache.qpid.proton.jms;

import javax.jms.BytesMessage;
import javax.jms.Destination;
import javax.jms.MapMessage;
import javax.jms.Message;
import javax.jms.ObjectMessage;
import javax.jms.StreamMessage;
import javax.jms.TextMessage;

/**
 * @author <a href="http://hiramchirino.com">Hiram Chirino</a>
 */
abstract public class JMSVendor {

    public static final byte QUEUE_TYPE = 0x00;
    public static final byte TOPIC_TYPE = 0x01;
    public static final byte TEMP_QUEUE_TYPE = 0x02;
    public static final byte TEMP_TOPIC_TYPE = 0x03;

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
