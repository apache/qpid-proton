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
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.Reader;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
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
import java.security.cert.CertificateFactory;
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
import org.apache.qpid.proton.engine.TransportException;

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

    // BouncyCastle Reflection Helpers
    private static final Constructor<?> pemParserCons;
    private static final Method         pemReadMethod;

    private static final Constructor<?> JcaPEMKeyConverterCons;
    private static final Class<?>       PEMKeyPairClass;
    private static final Method         getKeyPairMethod;
    private static final Method         getPrivateKeyMethod;

    private static final Class<?>       PEMEncryptedKeyPairClass;
    private static final Method         decryptKeyPairMethod;

    private static final Constructor<?> JcePEMDecryptorProviderBuilderCons;
    private static final Method         builderMethod;

    private static final Class<?>       PrivateKeyInfoClass;
    private static final Exception      bouncyCastleSetupException;

    static
    {
        // Setup BouncyCastle Reflection artifacts
        Constructor<?> pemParserConsResult = null;
        Method         pemReadMethodResult = null;
        Constructor<?> JcaPEMKeyConverterConsResult = null;
        Class<?>       PEMKeyPairClassResult = null;
        Method         getKeyPairMethodResult = null;
        Method         getPrivateKeyMethodResult = null;
        Class<?>       PEMEncryptedKeyPairClassResult = null;
        Method         decryptKeyPairMethodResult = null;
        Constructor<?> JcePEMDecryptorProviderBuilderConsResult = null;
        Method         builderMethodResult = null;
        Class<?>       PrivateKeyInfoClassResult = null;
        Exception      bouncyCastleSetupExceptionResult = null;

        try
        {
            final Class<?> pemParserClass = Class.forName("org.bouncycastle.openssl.PEMParser");
            pemParserConsResult = pemParserClass.getConstructor(Reader.class);
            pemReadMethodResult = pemParserClass.getMethod("readObject");

            final Class<?> jcaPEMKeyConverterClass = Class.forName("org.bouncycastle.openssl.jcajce.JcaPEMKeyConverter");
            JcaPEMKeyConverterConsResult = jcaPEMKeyConverterClass.getConstructor();
            PEMKeyPairClassResult = Class.forName("org.bouncycastle.openssl.PEMKeyPair");
            getKeyPairMethodResult = jcaPEMKeyConverterClass.getMethod("getKeyPair", PEMKeyPairClassResult);

            final Class<?> PEMDecrypterProvider = Class.forName("org.bouncycastle.openssl.PEMDecryptorProvider");

            PEMEncryptedKeyPairClassResult = Class.forName("org.bouncycastle.openssl.PEMEncryptedKeyPair");
            decryptKeyPairMethodResult = PEMEncryptedKeyPairClassResult.getMethod("decryptKeyPair", PEMDecrypterProvider);

            final Class<?> jcePEMDecryptorProviderBuilderClass = Class.forName(
                    "org.bouncycastle.openssl.jcajce.JcePEMDecryptorProviderBuilder");
            JcePEMDecryptorProviderBuilderConsResult = jcePEMDecryptorProviderBuilderClass.getConstructor();
            builderMethodResult = jcePEMDecryptorProviderBuilderClass.getMethod("build", char[].class);

            PrivateKeyInfoClassResult = Class.forName("org.bouncycastle.asn1.pkcs.PrivateKeyInfo");
            getPrivateKeyMethodResult = jcaPEMKeyConverterClass.getMethod("getPrivateKey", PrivateKeyInfoClassResult);

            // Try loading BC as a provider
            Class<?> klass = Class.forName("org.bouncycastle.jce.provider.BouncyCastleProvider");
            Provider provider = (Provider) klass.getConstructor().newInstance();
            Security.addProvider(provider);
        }
        catch (Exception e)
        {
            bouncyCastleSetupExceptionResult = e;
        }
        finally {
            pemParserCons = pemParserConsResult;
            pemReadMethod = pemReadMethodResult;
            JcaPEMKeyConverterCons = JcaPEMKeyConverterConsResult;
            PEMKeyPairClass = PEMKeyPairClassResult;
            getKeyPairMethod = getKeyPairMethodResult;
            getPrivateKeyMethod = getPrivateKeyMethodResult;
            PEMEncryptedKeyPairClass = PEMEncryptedKeyPairClassResult;
            decryptKeyPairMethod = decryptKeyPairMethodResult;
            JcePEMDecryptorProviderBuilderCons = JcePEMDecryptorProviderBuilderConsResult;
            builderMethod = builderMethodResult;
            PrivateKeyInfoClass = PrivateKeyInfoClassResult;
            bouncyCastleSetupException = bouncyCastleSetupExceptionResult;
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

    Certificate readCertificate(String pemFile)
    {
        InputStream is = null;

        try
        {
            CertificateFactory cFactory = CertificateFactory.getInstance("X.509");
            is = new FileInputStream(pemFile);
            return cFactory.generateCertificate(is);
        }
        catch (CertificateException ce)
        {
            String msg = "Failed to load certificate [" + pemFile + "]";
            _logger.log(Level.SEVERE, msg, ce);
            throw new TransportException(msg, ce);
        }
        catch (FileNotFoundException e)
        {
            String msg = "Certificate file not found [" + pemFile + "]";
            _logger.log(Level.SEVERE, msg);
            throw new TransportException(msg, e);
        }
        finally
        {
            closeSafely(is);
        }
    }

    PrivateKey readPrivateKey(String pemFile, String password)
    {
        if (bouncyCastleSetupException != null)
        {
            throw new TransportException("BouncyCastle failed to load", bouncyCastleSetupException);
        }

        final Object pemObject = readPemObject(pemFile);
        PrivateKey privateKey = null;

        try
        {
            Object keyConverter = JcaPEMKeyConverterCons.newInstance();
            setProvider(keyConverter, "BC");

            if (PEMEncryptedKeyPairClass.isInstance(pemObject))
            {
                Object decryptorBuilder = JcePEMDecryptorProviderBuilderCons.newInstance();

                // Build a PEMDecryptProvider
                Object decryptProvider = builderMethod.invoke(decryptorBuilder, password.toCharArray());

                Object decryptedKeyPair = decryptKeyPairMethod.invoke(pemObject, decryptProvider);
                KeyPair keyPair = (KeyPair) getKeyPairMethod.invoke(keyConverter, decryptedKeyPair);

                privateKey = keyPair.getPrivate();
            }
            else if (PEMKeyPairClass.isInstance(pemObject))
            {
                // It's a KeyPair but not encrypted.
                KeyPair keyPair = (KeyPair) getKeyPairMethod.invoke(keyConverter, pemObject);
                privateKey = keyPair.getPrivate();
            }
            else if (PrivateKeyInfoClass.isInstance(pemObject))
            {
                // It's an unencrypted private key
                privateKey = (PrivateKey) getPrivateKeyMethod.invoke(keyConverter, pemObject);
            }
            else
            {
                final String msg = "Unable to load PrivateKey, Unpexected Object [" + pemObject.getClass().getName()
                        + "]";
                _logger.log(Level.SEVERE, msg);
                throw new TransportException(msg);
            }
        }
        catch (InstantiationException | IllegalAccessException | IllegalArgumentException
                | InvocationTargetException | NoSuchMethodException | SecurityException e)
        {
            final String msg = "Failed to process key file [" + pemFile + "] - " + e.getMessage();
            throw new TransportException(msg, e);
        }

        return privateKey;
    }

    private Object readPemObject(String pemFile)
    {
        Reader reader = null;
        Object pemParser = null;
        Object pemObject = null;

        try
        {
            reader = new FileReader(pemFile);
            pemParser = pemParserCons.newInstance(reader); // = new PEMParser(reader);
            pemObject = pemReadMethod.invoke(pemParser); // = pemParser.readObject();
        }
        catch (IOException | IllegalAccessException | IllegalArgumentException | InvocationTargetException | InstantiationException e)
        {
            _logger.log(Level.SEVERE, "Unable to read PEM object. Perhaps you need the unlimited strength libraries in <java-home>/jre/lib/security/ ?", e);
            throw new TransportException("Unable to read PEM object from file " + pemFile, e);
        }
        finally
        {
            closeSafely(reader);
        }

        return pemObject;
    }

    private void setProvider(Object obj, String provider) throws IllegalAccessException, IllegalArgumentException, InvocationTargetException, NoSuchMethodException, SecurityException
    {
        final Class<?> aClz = obj.getClass();
        final Method setProvider = aClz.getMethod("setProvider", String.class);
        setProvider.invoke(obj, provider);
    }

    private void closeSafely(Closeable c)
    {
        if (c != null)
        {
            try
            {
                c.close();
            }
            catch (IOException e)
            {
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
