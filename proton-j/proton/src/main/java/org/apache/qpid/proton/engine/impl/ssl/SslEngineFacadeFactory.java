/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */
package org.apache.qpid.proton.engine.impl.ssl;

import java.io.FileReader;
import java.io.IOException;
import java.io.Reader;
import java.security.KeyManagementException;
import java.security.KeyPair;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.Security;
import java.security.UnrecoverableKeyException;
import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLEngine;
import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactory;
import javax.net.ssl.X509TrustManager;

import org.apache.qpid.proton.engine.SslDomain;
import org.apache.qpid.proton.engine.SslPeerDetails;
import org.bouncycastle.jce.provider.BouncyCastleProvider;
import org.bouncycastle.openssl.PEMException;
import org.bouncycastle.openssl.PEMReader;
import org.bouncycastle.openssl.PasswordFinder;

public class SslEngineFacadeFactory
{
    private static final Logger _logger = Logger.getLogger(SslEngineFacadeFactory.class.getName());

    /**
     * The protocol name used to create an {@link SSLContext}, taken from Java's list of
     * standard names at http://docs.oracle.com/javase/6/docs/technotes/guides/security/StandardNames.html
     *
     * TODO allow the protocol name to be overridden somehow
     */
    private static final String TLS_PROTOCOL = "TLS";

    static
    {
        Security.addProvider(new BouncyCastleProvider());
    }

    /**
     * This is a list of all anonymous cipher suites supported by Java 6, excluding those that
     * use MD5.  These are all supported by both Oracle's and IBM's Java 6 implementation.
     */
    private static final List<String> ANONYMOUS_CIPHER_SUITES = Arrays.asList(
            "TLS_DH_anon_WITH_AES_128_CBC_SHA",
            "SSL_DH_anon_WITH_3DES_EDE_CBC_SHA",
            "SSL_DH_anon_WITH_DES_CBC_SHA",
            "SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA");

    /** lazily initialized */
    private SSLContext _sslContext;


    /**
     * Returns a {@link ProtonSslEngine}. May cache the domain's settings so callers should invoke
     * {@link #resetCache()} if the domain changes.
     *
     * @param peerDetails may be used to return an engine that supports SSL resume.
     */
    public ProtonSslEngine createProtonSslEngine(SslDomain domain, SslPeerDetails peerDetails)
    {
        SSLEngine engine = createAndInitialiseSslEngine(domain, peerDetails);
        if(_logger.isLoggable(Level.FINE))
        {
            _logger.fine("Created SSL engine: " + engineToString(engine));
        }
        return new DefaultSslEngineFacade(engine);
    }


    /**
     * Guarantees that no cached settings are used in subsequent calls to
     * {@link #createProtonSslEngine(SslDomain, SslPeerDetails)}.
     */
    public void resetCache()
    {
        _sslContext = null;
    }


    private SSLEngine createAndInitialiseSslEngine(SslDomain domain, SslPeerDetails peerDetails)
    {
        SslDomain.Mode mode = domain.getMode();

        SSLContext sslContext = getOrCreateSslContext(domain);
        SSLEngine sslEngine = createSslEngine(sslContext, peerDetails);

        if (domain.getPeerAuthentication() == SslDomain.VerifyMode.ANONYMOUS_PEER)
        {
            addAnonymousCipherSuites(sslEngine);
        }
        else
        {
            if (mode == SslDomain.Mode.SERVER)
            {
                sslEngine.setNeedClientAuth(true);
            }
        }

        if(_logger.isLoggable(Level.FINE))
        {
            _logger.log(Level.FINE, mode + " Enabled cipher suites " + Arrays.asList(sslEngine.getEnabledCipherSuites()));
        }

        boolean useClientMode = mode == SslDomain.Mode.CLIENT ? true : false;
        sslEngine.setUseClientMode(useClientMode);

        return sslEngine;
    }

    /**
     * @param sslPeerDetails is allowed to be null. A non-null value is used to hint that SSL resumption
     * should be attempted
     */
    private SSLEngine createSslEngine(SSLContext sslContext, SslPeerDetails sslPeerDetails)
    {
        final SSLEngine sslEngine;
        if(sslPeerDetails == null)
        {
            sslEngine = sslContext.createSSLEngine();
        }
        else
        {
            sslEngine = sslContext.createSSLEngine(sslPeerDetails.getHostname(), sslPeerDetails.getPort());
        }
        return sslEngine;
    }

    private SSLContext getOrCreateSslContext(SslDomain sslDomain)
    {
        if(_sslContext == null)
        {
            if(_logger.isLoggable(Level.FINE))
            {
                _logger.fine("lazily creating new SSLContext using domain " + sslDomain);
            }

            final char[] dummyPassword = "unused-passphrase".toCharArray(); // Dummy password required by KeyStore and KeyManagerFactory, but never referred to again

            try
            {
                SSLContext sslContext = SSLContext.getInstance(TLS_PROTOCOL);
                KeyStore ksKeys = createKeyStoreFrom(sslDomain, dummyPassword);

                KeyManagerFactory kmf = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
                kmf.init(ksKeys, dummyPassword);

                final TrustManager[] trustManagers;
                if (sslDomain.getPeerAuthentication() == SslDomain.VerifyMode.ANONYMOUS_PEER)
                {
                    trustManagers = new TrustManager[] { new AlwaysTrustingTrustManager() };
                }
                else
                {
                    TrustManagerFactory tmf = TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
                    tmf.init(ksKeys);
                    trustManagers = tmf.getTrustManagers();
                }

                sslContext.init(kmf.getKeyManagers(), trustManagers, null);
                _sslContext = sslContext;
            }
            catch (NoSuchAlgorithmException e)
            {
                throw new IllegalStateException("Unexpected exception creating SSLContext", e);
            }
            catch (KeyStoreException e)
            {
                throw new IllegalStateException("Unexpected exception creating SSLContext", e);
            }
            catch (UnrecoverableKeyException e)
            {
                throw new IllegalStateException("Unexpected exception creating SSLContext", e);
            }
            catch (KeyManagementException e)
            {
                throw new IllegalStateException("Unexpected exception creating SSLContext", e);
            }
        }
        return _sslContext;
    }

    private KeyStore createKeyStoreFrom(SslDomain sslDomain, char[] dummyPassword)
    {
        try
        {
            KeyStore keystore = KeyStore.getInstance(KeyStore.getDefaultType());
            keystore.load(null, null);

            if (sslDomain.getTrustedCaDb() != null)
            {
                String caCertAlias = "cacert";

                if(_logger.isLoggable(Level.FINE))
                {
                    _logger.log(Level.FINE, "_sslParams.getTrustedCaDb() : " + sslDomain.getTrustedCaDb());
                }
                Certificate trustedCaCert = (Certificate) readPemObject(sslDomain.getTrustedCaDb(), null, Certificate.class);
                keystore.setCertificateEntry(caCertAlias, trustedCaCert);
            }

            if (sslDomain.getCertificateFile() != null
                    && sslDomain.getPrivateKeyFile() != null)
            {
                String clientPrivateKeyAlias = "clientPrivateKey";
                Certificate clientCertificate = (Certificate) readPemObject(sslDomain.getCertificateFile(), null, Certificate.class);
                Object keyOrKeyPair = readPemObject(
                        sslDomain.getPrivateKeyFile(),
                        sslDomain.getPrivateKeyPassword(), PrivateKey.class, KeyPair.class);

                final PrivateKey clientPrivateKey;
                if (keyOrKeyPair instanceof PrivateKey)
                {
                    clientPrivateKey = (PrivateKey)keyOrKeyPair;
                }
                else if (keyOrKeyPair instanceof KeyPair)
                {
                    clientPrivateKey = ((KeyPair)keyOrKeyPair).getPrivate();
                }
                else
                {
                    // Should not happen - readPemObject will have already verified key type
                    throw new IllegalStateException("Unexpected key type " + keyOrKeyPair);
                }

                keystore.setKeyEntry(clientPrivateKeyAlias, clientPrivateKey,
                        dummyPassword, new Certificate[] { clientCertificate });
            }
            return keystore;
        }
        catch (KeyStoreException e)
        {
           throw new IllegalStateException("Unexpected exception creating keystore", e);
        }
        catch (NoSuchAlgorithmException e)
        {
            throw new IllegalStateException("Unexpected exception creating keystore", e);
        }
        catch (CertificateException e)
        {
            throw new IllegalStateException("Unexpected exception creating keystore", e);
        }
        catch (IOException e)
        {
            throw new IllegalStateException("Unexpected exception creating keystore", e);
        }
    }

    private void addAnonymousCipherSuites(SSLEngine sslEngine)
    {
        List<String> supportedSuites = Arrays.asList(sslEngine.getSupportedCipherSuites());
        List<String> currentEnabledSuites = Arrays.asList(sslEngine.getEnabledCipherSuites());

        List<String> enabledSuites = buildEnabledSuitesIncludingAnonymous(ANONYMOUS_CIPHER_SUITES, supportedSuites, currentEnabledSuites);
        sslEngine.setEnabledCipherSuites(enabledSuites.toArray(new String[0]));
    }

    private List<String> buildEnabledSuitesIncludingAnonymous(
            List<String> anonymousCipherSuites, List<String> supportedSuites, List<String> currentEnabled)
    {
        List<String> newEnabled = new ArrayList<String>(currentEnabled);

        int addedAnonymousCipherSuites = 0;
        for (String anonymousCipherSuiteName : anonymousCipherSuites)
        {
            if (supportedSuites.contains(anonymousCipherSuiteName))
            {
                newEnabled.add(anonymousCipherSuiteName);
                addedAnonymousCipherSuites++;
            }
        }

        if (addedAnonymousCipherSuites == 0)
        {
            throw new IllegalStateException("None of " + anonymousCipherSuites
                    + " anonymous cipher suites are within the supported list "
                    + supportedSuites);
        }

        if(_logger.isLoggable(Level.FINE))
        {
            _logger.fine("There are now " + newEnabled.size()
                    + " cipher suites enabled (previously " + currentEnabled.size()
                    + "), including " + addedAnonymousCipherSuites + " out of the "
                    + anonymousCipherSuites.size() + " requested anonymous ones." );
        }

        return newEnabled;
    }

    private String engineToString(SSLEngine engine)
    {
        return new StringBuilder("[ " )
            .append(engine)
            .append(", needClientAuth=").append(engine.getNeedClientAuth())
            .append(", useClientMode=").append(engine.getUseClientMode())
            .append(", peerHost=").append(engine.getPeerHost())
            .append(", peerPort=").append(engine.getPeerPort())
            .append(" ]").toString();
    }

    private Object readPemObject(String pemFile, String keyPassword, @SuppressWarnings("rawtypes") Class... expectedInterfaces)
    {
        final PasswordFinder passwordFinder;
        if (keyPassword != null)
        {
            passwordFinder = getPasswordFinderFor(keyPassword);
        }
        else
        {
            passwordFinder = null;
        }

        Reader reader = null;
        try
        {
            reader = new FileReader(pemFile);
            PEMReader pemReader = new PEMReader(reader, passwordFinder);
            Object pemObject = pemReader.readObject();
            if (!checkPemObjectIsOfAllowedTypes(pemObject, expectedInterfaces))
            {
                throw new IllegalStateException("File " + pemFile + " does not provide a object of the required type."
                        + " Read an object of class " + pemObject.getClass().getName()
                        + " whilst expecting an implementation of one of the following  : " + Arrays.asList(expectedInterfaces));
            }
            return pemObject;
        }
        catch(PEMException e)
        {
            _logger.log(Level.SEVERE, "Unable to read PEM object. Perhaps you need the unlimited strength libraries in <java-home>/jre/lib/security/ ?", e);
            throw new IllegalStateException("Unable to read PEM object from file " + pemFile, e);
        }
        catch (IOException e)
        {
            throw new RuntimeException("Unable to read PEM object from file " + pemFile, e);
        }
        finally
        {
            if (reader != null)
            {
                try
                {
                    reader.close();
                }
                catch (IOException e)
                {
                    // Ignore
                }
            }
        }
    }

    @SuppressWarnings("rawtypes")
    private boolean checkPemObjectIsOfAllowedTypes(Object pemObject,  Class... expectedInterfaces)
    {
        if (expectedInterfaces.length == 0)
        {
            throw new IllegalArgumentException("Must be at least one expectedKeyTypes");
        }

        for (Class keyInterface : expectedInterfaces)
        {
            if (keyInterface.isInstance(pemObject))
            {
                return true;
            }
        }
        return false;
    }

    private PasswordFinder getPasswordFinderFor(final String keyPassword)
    {
        PasswordFinder passwordFinder = new PasswordFinder()
        {
            @Override
            public char[] getPassword()
            {
                return keyPassword.toCharArray();
            }
        };
        return passwordFinder;
    }

    private final class AlwaysTrustingTrustManager implements X509TrustManager
    {
        @Override
        public X509Certificate[] getAcceptedIssuers()
        {
            return null;
        }

        @Override
        public void checkServerTrusted(X509Certificate[] arg0, String arg1)
                throws CertificateException
        {
            // Do not check certificate
        }

        @Override
        public void checkClientTrusted(X509Certificate[] arg0, String arg1)
                throws CertificateException
        {
            // Do not check certificate
        }
    }
}
