package org.apache.qpid.proton.engine.impl.ssl;

import static org.junit.Assert.assertNotNull;

import java.net.URL;
import java.security.PrivateKey;
import java.security.cert.Certificate;

import org.junit.Test;

public class SslEngineFacadeFactoryTest {

    
    @Test
    public void testCertifcateLoad() {
        String ipFile = resolveFilename("cert.pem.txt");
        
        SslEngineFacadeFactory factory = new SslEngineFacadeFactory();
        
        Certificate cert = null;
        
       cert = factory.readCertificate(ipFile);
       
       assertNotNull("Certificate was NULL", cert);
    }
    
    @Test
    public void testLoadKey() {
        String keyFile = resolveFilename("key.pem.txt");
        SslEngineFacadeFactory factory = new SslEngineFacadeFactory();
        
        PrivateKey key = null;
        
        key = factory.readPrivateKey(keyFile, "unittest");
        
        assertNotNull("Key was NULL", key);
        
    }
    
    @Test
    public void testLoadUnencryptedPrivateKey(){
        
        String keyFile = resolveFilename("private-key-clear.pem.txt");
        SslEngineFacadeFactory factory = new SslEngineFacadeFactory();
        
        PrivateKey key = null;
        
        key = factory.readPrivateKey(keyFile, "unittest");
        
        assertNotNull("Key was NULL", key);
    }
    
    private String resolveFilename(String testFilename) {
        
        URL resourceUri = this.getClass().getResource(testFilename);
        
        assertNotNull("Failed to load file: " + testFilename, resourceUri);
        
        String fName = resourceUri.getPath();
        
        return fName;
        
    }

}
