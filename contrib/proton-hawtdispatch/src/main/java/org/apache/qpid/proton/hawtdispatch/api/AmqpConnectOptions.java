/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.apache.qpid.proton.hawtdispatch.api;

import org.fusesource.hawtdispatch.DispatchQueue;
import org.fusesource.hawtdispatch.transport.TcpTransport;

import javax.net.ssl.SSLContext;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.*;

/**
 * @author <a href="http://hiramchirino.com">Hiram Chirino</a>
 */
public class AmqpConnectOptions implements Cloneable {

    private static final long KEEP_ALIVE = Long.parseLong(System.getProperty("amqp.thread.keep_alive", ""+1000));
    private static final long STACK_SIZE = Long.parseLong(System.getProperty("amqp.thread.stack_size", ""+1024*512));
    private static ThreadPoolExecutor blockingThreadPool;

    public synchronized static ThreadPoolExecutor getBlockingThreadPool() {
        if( blockingThreadPool == null ) {
            blockingThreadPool = new ThreadPoolExecutor(0, Integer.MAX_VALUE, KEEP_ALIVE, TimeUnit.MILLISECONDS, new SynchronousQueue<Runnable>(), new ThreadFactory() {
                    public Thread newThread(Runnable r) {
                        Thread rc = new Thread(null, r, "AMQP Task", STACK_SIZE);
                        rc.setDaemon(true);
                        return rc;
                    }
                }) {

                    @Override
                    public void shutdown() {
                        // we don't ever shutdown since we are shared..
                    }

                    @Override
                    public List<Runnable> shutdownNow() {
                        // we don't ever shutdown since we are shared..
                        return Collections.emptyList();
                    }
                };
        }
        return blockingThreadPool;
    }
    public synchronized static void setBlockingThreadPool(ThreadPoolExecutor pool) {
        blockingThreadPool = pool;
    }

    private static final URI DEFAULT_HOST;
    static {
        try {
            DEFAULT_HOST = new URI("tcp://localhost");
        } catch (URISyntaxException e) {
            throw new RuntimeException(e);
        }
    }

    URI host = DEFAULT_HOST;
    URI localAddress;
    SSLContext sslContext;
    DispatchQueue dispatchQueue;
    Executor blockingExecutor;
    int maxReadRate;
    int maxWriteRate;
    int trafficClass = TcpTransport.IPTOS_THROUGHPUT;
    boolean useLocalHost;
    int receiveBufferSize = 1024*64;
    int sendBufferSize = 1024*64;
    String localContainerId;
    String remoteContainerId;
    String user;
    String password;


    @Override
    public AmqpConnectOptions clone() {
        try {
            return (AmqpConnectOptions) super.clone();
        } catch (CloneNotSupportedException e) {
            throw new RuntimeException(e);
        }
    }

    public String getLocalContainerId() {
        return localContainerId;
    }

    public void setLocalContainerId(String localContainerId) {
        this.localContainerId = localContainerId;
    }

    public String getPassword() {
        return password;
    }

    public void setPassword(String password) {
        this.password = password;
    }

    public String getRemoteContainerId() {
        return remoteContainerId;
    }

    public void setRemoteContainerId(String remoteContainerId) {
        this.remoteContainerId = remoteContainerId;
    }

    public String getUser() {
        return user;
    }

    public void setUser(String user) {
        this.user = user;
    }

    public Executor getBlockingExecutor() {
        return blockingExecutor;
    }

    public void setBlockingExecutor(Executor blockingExecutor) {
        this.blockingExecutor = blockingExecutor;
    }

    public DispatchQueue getDispatchQueue() {
        return dispatchQueue;
    }

    public void setDispatchQueue(DispatchQueue dispatchQueue) {
        this.dispatchQueue = dispatchQueue;
    }

    public URI getLocalAddress() {
        return localAddress;
    }

    public void setLocalAddress(String localAddress) throws URISyntaxException {
        this.setLocalAddress(new URI(localAddress));
    }
    public void setLocalAddress(URI localAddress) {
        this.localAddress = localAddress;
    }

    public int getMaxReadRate() {
        return maxReadRate;
    }

    public void setMaxReadRate(int maxReadRate) {
        this.maxReadRate = maxReadRate;
    }

    public int getMaxWriteRate() {
        return maxWriteRate;
    }

    public void setMaxWriteRate(int maxWriteRate) {
        this.maxWriteRate = maxWriteRate;
    }

    public int getReceiveBufferSize() {
        return receiveBufferSize;
    }

    public void setReceiveBufferSize(int receiveBufferSize) {
        this.receiveBufferSize = receiveBufferSize;
    }

    public URI getHost() {
        return host;
    }
    public void setHost(String host, int port) throws URISyntaxException {
        this.setHost(new URI("tcp://"+host+":"+port));
    }
    public void setHost(String host) throws URISyntaxException {
        this.setHost(new URI(host));
    }
    public void setHost(URI host) {
        this.host = host;
    }

    public int getSendBufferSize() {
        return sendBufferSize;
    }

    public void setSendBufferSize(int sendBufferSize) {
        this.sendBufferSize = sendBufferSize;
    }

    public SSLContext getSslContext() {
        return sslContext;
    }

    public void setSslContext(SSLContext sslContext) {
        this.sslContext = sslContext;
    }

    public int getTrafficClass() {
        return trafficClass;
    }

    public void setTrafficClass(int trafficClass) {
        this.trafficClass = trafficClass;
    }

    public boolean isUseLocalHost() {
        return useLocalHost;
    }

    public void setUseLocalHost(boolean useLocalHost) {
        this.useLocalHost = useLocalHost;
    }
}
