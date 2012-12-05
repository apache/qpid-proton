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
import java.security.Key;
import java.security.KeyManagementException;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchAlgorithmException;
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

import org.apache.qpid.proton.engine.Ssl;
import org.apache.qpid.proton.engine.Ssl.Mode;
import org.apache.qpid.proton.engine.Ssl.VerifyMode;
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

    public SslEngineFacade createSslEngineFacade(Ssl sslConfiguration)
    {
        SSLEngine engine = createSslEngine(sslConfiguration);
        return new DefaultSslEngineFacade(engine);
    }

    private SSLEngine createSslEngine(Ssl sslConfiguration)
    {
        SSLEngine sslEngine;

        SSLContext sslContext = createSslContext(sslConfiguration);
        sslEngine = sslContext.createSSLEngine();
        if (sslConfiguration.getPeerAuthentication() == VerifyMode.ANONYMOUS_PEER)
        {
            addAnonymousCipherSuites(sslEngine);
        }
        else
        {
            if (sslConfiguration.getMode() == Mode.SERVER)
            {
                sslEngine.setNeedClientAuth(true);
            }
        }

        if(_logger.isLoggable(Level.FINE))
        {
            _logger.log(Level.FINE, sslConfiguration.getMode() + " Enabled cipher suites " + Arrays.asList(sslEngine.getEnabledCipherSuites()));
        }

        boolean useClientMode = sslConfiguration.getMode() == Mode.CLIENT ? true : false;
        sslEngine.setUseClientMode(useClientMode);

        return sslEngine;
    }

    private SSLContext createSslContext(Ssl sslConfiguration)
    {
        final char[] dummyPassword = "unused-passphrase".toCharArray(); // Dummy password required by KeyStore and KeyManagerFactory, but never referred to again

        try
        {
            SSLContext sslContext = SSLContext.getInstance(TLS_PROTOCOL);
            KeyStore ksKeys = createKeyStoreFrom(sslConfiguration, dummyPassword);

            KeyManagerFactory kmf = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
            kmf.init(ksKeys, dummyPassword);

            final TrustManager[] trustManagers;
            if (sslConfiguration.getTrustedCaDb() == null && sslConfiguration.getPeerAuthentication() == VerifyMode.ANONYMOUS_PEER)
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
            return sslContext;
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

    private KeyStore createKeyStoreFrom(Ssl sslConfiguration, char[] dummyPassword)
    {
        try
        {
            KeyStore keystore = KeyStore.getInstance(KeyStore.getDefaultType());
            keystore.load(null, null);

            if (sslConfiguration.getTrustedCaDb() != null)
            {
                String caCertAlias = "cacert";

                if(_logger.isLoggable(Level.FINE))
                {
                    _logger.log(Level.FINE, "_sslParams.getTrustedCaDb() : " + sslConfiguration.getTrustedCaDb());
                }
                Certificate trustedCaCert = (Certificate) readPemObject(sslConfiguration.getTrustedCaDb());
                keystore.setCertificateEntry(caCertAlias, trustedCaCert);
            }

            if (sslConfiguration.getCertificateFile() != null
                    && sslConfiguration.getPrivateKeyFile() != null)
            {
                String clientPrivateKeyAlias = "clientPrivateKey";
                Certificate clientCertificate = (Certificate) readPemObject(sslConfiguration.getCertificateFile());
                Key clientPrivateKey = (Key) readPemObject(
                        sslConfiguration.getPrivateKeyFile(),
                        sslConfiguration.getPrivateKeyPassword());

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

        boolean addedAnonymousCipherSuite = false;
        for (String anonymousCipherSuiteName : anonymousCipherSuites)
        {
            if (supportedSuites.contains(anonymousCipherSuiteName))
            {
                newEnabled.add(anonymousCipherSuiteName);
                addedAnonymousCipherSuite = true;
            }
        }

        if (!addedAnonymousCipherSuite)
        {
            throw new IllegalStateException("None of " + anonymousCipherSuites
                    + " anonymous cipher suites are within the supported list "
                    + supportedSuites);
        }
        return newEnabled;
    }

    private Object readPemObject(String pemFile, final String privateKeyPassword)
    {
        PasswordFinder passwordFinder = new PasswordFinder()
        {
            @Override
            public char[] getPassword()
            {
                return privateKeyPassword.toCharArray();
            }
        };

        Reader reader = null;
        try
        {
            reader = new FileReader(pemFile);
            return new PEMReader(reader, privateKeyPassword == null ? null : passwordFinder).readObject();
        }
        catch(PEMException e)
        {
            _logger.log(Level.SEVERE, "Unable to read PEM object. Perhaps you need the unlimited strength libraries in <java-home>/jre/lib/security/ ?", e);
            throw new RuntimeException(e);
        }
        catch (IOException e)
        {
            throw new RuntimeException(e);
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

    private Object readPemObject(String pemFile)
    {
        return readPemObject(pemFile, null);
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
