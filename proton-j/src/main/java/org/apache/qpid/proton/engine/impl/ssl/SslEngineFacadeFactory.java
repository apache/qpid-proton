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

import java.io.Closeable;
import java.io.FileReader;
import java.io.IOException;
import java.io.Reader;
import java.security.KeyManagementException;
import java.security.KeyPair;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.Provider;
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
import org.apache.qpid.proton.ProtonUnsupportedOperationException;
import org.apache.qpid.proton.engine.SslDomain;
import org.apache.qpid.proton.engine.SslPeerDetails;
import org.apache.qpid.proton.engine.TransportException;
import org.bouncycastle.asn1.pkcs.PrivateKeyInfo;
import org.bouncycastle.cert.X509CertificateHolder;
import org.bouncycastle.cert.jcajce.JcaX509CertificateConverter;
import org.bouncycastle.openssl.PEMDecryptorProvider;
import org.bouncycastle.openssl.PEMEncryptedKeyPair;
import org.bouncycastle.openssl.PEMKeyPair;
import org.bouncycastle.openssl.PEMParser;
import org.bouncycastle.openssl.jcajce.JcaPEMKeyConverter;
import org.bouncycastle.openssl.jcajce.JcePEMDecryptorProviderBuilder;

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

    private static boolean bouncyCastlePresent;
     
    static
    {
        try {
            // Try loading BC as a provider
            Class klass = Class.forName("org.bouncycastle.jce.provider.BouncyCastleProvider");
            Security.addProvider((Provider) klass.newInstance());
            
            // Ensure we're running a newer version of BC
            klass = Class.forName("org.bouncycastle.openssl.PEMParser");
            bouncyCastlePresent = true;
        } catch (ClassNotFoundException e) {
            _logger.warning("unable to load bouncycastle provider. " + e.getMessage());
            bouncyCastlePresent = false;
        } catch (InstantiationException e) {
            _logger.warning("unable to instantiate bouncycastle provider");
            bouncyCastlePresent = false;
        } catch (IllegalAccessException e) {
            _logger.warning("unable to access bouncycastle provider");
            bouncyCastlePresent = false;
        }
    }

    SslEngineFacadeFactory()
    {
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

        removeSSLv3Support(sslEngine);

        return sslEngine;
    }

    private static final String SSLV3_PROTOCOL = "SSLv3";

    private static void removeSSLv3Support(final SSLEngine engine)
    {
        List<String> enabledProtocols = Arrays.asList(engine.getEnabledProtocols());
        if(enabledProtocols.contains(SSLV3_PROTOCOL))
        {
            List<String> allowedProtocols = new ArrayList<String>(enabledProtocols);
            allowedProtocols.remove(SSLV3_PROTOCOL);
            engine.setEnabledProtocols(allowedProtocols.toArray(new String[allowedProtocols.size()]));
        }
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
                throw new TransportException("Unexpected exception creating SSLContext", e);
            }
            catch (KeyStoreException e)
            {
                throw new TransportException("Unexpected exception creating SSLContext", e);
            }
            catch (UnrecoverableKeyException e)
            {
                throw new TransportException("Unexpected exception creating SSLContext", e);
            }
            catch (KeyManagementException e)
            {
                throw new TransportException("Unexpected exception creating SSLContext", e);
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
                
                Certificate trustedCaCert = readCertificate(sslDomain.getTrustedCaDb());
                keystore.setCertificateEntry(caCertAlias, trustedCaCert);
            }

            if (sslDomain.getCertificateFile() != null
                    && sslDomain.getPrivateKeyFile() != null)
            {
                String clientPrivateKeyAlias = "clientPrivateKey";
                Certificate clientCertificate = (Certificate) readCertificate(sslDomain.getCertificateFile());
              
                PrivateKey clientPrivateKey = readPrivateKey(sslDomain.getPrivateKeyFile(), sslDomain.getPrivateKeyPassword());
          
                keystore.setKeyEntry(clientPrivateKeyAlias, clientPrivateKey,
                        dummyPassword, new Certificate[] { clientCertificate });
            }
            return keystore;
        }
        catch (KeyStoreException e)
        {
           throw new TransportException("Unexpected exception creating keystore", e);
        }
        catch (NoSuchAlgorithmException e)
        {
            throw new TransportException("Unexpected exception creating keystore", e);
        }
        catch (CertificateException e)
        {
            throw new TransportException("Unexpected exception creating keystore", e);
        }
        catch (IOException e)
        {
            throw new TransportException("Unexpected exception creating keystore", e);
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
            throw new TransportException
                ("None of " + anonymousCipherSuites
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
    
    private Certificate readCertificate(String pemFile) {

        // Sanity
        if (!bouncyCastlePresent) {
            throw new ProtonUnsupportedOperationException("BouncyCastle Not Loaded");
        }

        final Object pemObject = readPemObject(pemFile);

        try {
            if (pemObject instanceof Certificate) {
                return (Certificate) pemObject;
            }
            else if (pemObject instanceof X509CertificateHolder) {
                X509CertificateHolder certHolder = (X509CertificateHolder) pemObject;
                JcaX509CertificateConverter converter = new JcaX509CertificateConverter().setProvider("BC");
                return converter.getCertificate(certHolder);

            }
            else {
                final String msg = "Unable to load Cert, Unpexected Object [" + pemObject.getClass().getName() + "]";

                _logger.log(Level.SEVERE, msg);
                throw new TransportException(msg);
            }
        }
        catch (CertificateException e) {
            _logger.log(Level.SEVERE, "Unable to load certificate: " + pemFile, e);
            throw new TransportException("Failed to load certificate", e);

        }

    }

    private PrivateKey readPrivateKey(String pemFile, String password) {

        // Sanity
        if (!bouncyCastlePresent) {
            throw new ProtonUnsupportedOperationException("BouncyCastle Not Loaded");
        }

        PrivateKey privateKey = null;

        final Object pemObject = readPemObject(pemFile);
        final JcaPEMKeyConverter keyConverter = new JcaPEMKeyConverter().setProvider("BC");
       

        try {

            if (pemObject instanceof PEMEncryptedKeyPair) {
                final PEMDecryptorProvider decryptProvider = new JcePEMDecryptorProviderBuilder().build(password.toCharArray());
                KeyPair keyPair = keyConverter
                        .getKeyPair(((PEMEncryptedKeyPair) pemObject).decryptKeyPair(decryptProvider));

                privateKey = keyPair.getPrivate();

            }
            else if (pemObject instanceof PEMKeyPair) {
                KeyPair keyPair = keyConverter.getKeyPair((PEMKeyPair) pemObject);
                privateKey = keyPair.getPrivate();
            }
            else if (pemObject instanceof PrivateKeyInfo) {
                privateKey = keyConverter.getPrivateKey((PrivateKeyInfo) pemObject);
            }
            else {
                final String msg = "Unable to load PrivateKey, Unpexected Object [" + pemObject.getClass().getName()
                        + "]";

                _logger.log(Level.SEVERE, msg);
                throw new TransportException(msg);
            }

        }
        catch (IOException e) {
            _logger.log(Level.SEVERE, "Unable to load PrivateKey", e);
            throw new TransportException("Unable to load Private Key from file " + pemFile, e);
        }

        return privateKey;
    }

    private Object readPemObject(String pemFile) {

        Reader reader = null;
        PEMParser pemParser = null;
        Object pemObject = null;

        try {
            reader = new FileReader(pemFile);
            pemParser = new PEMParser(reader);
            pemObject = pemParser.readObject();
        }
        catch (IOException e) {
            _logger.log(Level.SEVERE, "Unable to read PEM object. Perhaps you need the unlimited strength libraries in <java-home>/jre/lib/security/ ?", e);
            throw new TransportException("Unable to read PEM object from file " + pemFile, e);
        }
        finally {
            closeSafely(pemParser);
            closeSafely(reader);
        }

        return pemObject;
    }

    
    private void closeSafely(Closeable c){
        
        if (c != null) {
            try {
                c.close();
            }
            catch (IOException e) {
               // Swallow
            }
        }
    }
    
    private void closeSafely(PEMParser pp) {
        
        if (pp != null) {
            try {
                pp.close();
            }
            catch (IOException e) {
              // Swallow
            }
        }
    }

    private static final class AlwaysTrustingTrustManager implements X509TrustManager
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
