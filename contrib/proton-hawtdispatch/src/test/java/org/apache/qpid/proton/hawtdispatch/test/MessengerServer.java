package org.apache.qpid.proton.hawtdispatch.test;

import java.io.IOException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

import org.apache.qpid.proton.InterruptException;
import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.amqp.messaging.Section;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.messenger.Messenger;
import org.apache.qpid.proton.messenger.Tracker;

public class MessengerServer {
	public static final String REJECT_ME = "*REJECT-ME*";
	private int timeout = 1000;
	private String host = "127.0.0.1";
	private int port = 55555;
	private Messenger msgr;
	private AtomicInteger messagesReceived = new AtomicInteger(0);
	private AtomicInteger messagesSent = new AtomicInteger(0);
	private AtomicBoolean serverShouldRun = new AtomicBoolean();
	private AtomicReference<Throwable> issues = new AtomicReference<Throwable>();
	private Thread thread;
	private CountDownLatch serverStart;

	public MessengerServer() {
	}
	public void start() {
		if (!serverShouldRun.compareAndSet(false, true)) {
			throw new IllegalStateException("started twice");
		}
		msgr = Proton.messenger();
		serverStart = new CountDownLatch(1);
		thread = new Thread(new Runnable() {

			@Override
			public void run() {
				try {
					msgr.start();
					msgr.subscribe("amqp://~"+host+":"+String.valueOf(port));
					serverStart.countDown();
					try {
						while(serverShouldRun.get()) {
							msgr.recv(100);
							while (msgr.incoming() > 0) {
								Message msg = msgr.get();
								messagesReceived.incrementAndGet();
								Tracker tracker = msgr.incomingTracker();
								if (REJECT_ME.equals(msg.getBody())) {
									msgr.reject(tracker , 0);
								} else {
									msgr.accept(tracker, 0);
								}
								String reply_to = msg.getReplyTo();
								if (reply_to != null) {
									msg.setAddress(reply_to);
									msgr.put(msg);
									msgr.settle(msgr.outgoingTracker(), 0);
								}
							}
						}
					} finally {
						msgr.stop();
					}
				} catch (InterruptException ex) {
					// we're done
				} catch (Exception ex) {
					issues.set(ex);
				}
			}

		});
		thread.setName("MessengerServer");
		thread.setDaemon(true);
		thread.start();
		try {
			serverStart.await();
		} catch (InterruptedException e) {
			msgr.interrupt();
		}
	}

	public void stop() {
		if (!serverShouldRun.compareAndSet(true, false)) {
			return;
		}
		if (serverStart.getCount() == 0)
			msgr.interrupt();
		try {
			thread.join(timeout);
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		thread = null;
		if (!msgr.stopped())
			msgr.stop();
		Throwable throwable = issues.get();
		if (throwable != null)
			throw new RuntimeException("Messenger server had problems", throwable);
	}

	public String getHost() {
		return host;
	}

	public int getPort() {
		return port;
	}

	public void setHost(String host) {
		this.host = host;
	}

	public void setPort(int port) {
		this.port = port;
	}
	public int getTimeout() {
		return timeout;
	}
	public void setTimeout(int timeout) {
		this.timeout = timeout;
	}

	public int getMessagesReceived() {
		return messagesReceived.get();
	}

	public int getMessagesSent() {
		return messagesSent.get();
	}
}
