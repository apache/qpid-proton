package org.apache.qpid.proton.engine.impl.ssl;

import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.pour;
import static org.junit.Assert.assertEquals;

import java.nio.ByteBuffer;

import org.junit.Test;

public class SimpleSslTransportWrapperBufferSizeTest
{
    @Test
    public void testUnderlyingInputUsingSmallBuffer_receivesAllDecodedInput() throws Exception
    {
    	// set the underlyingInput.Buffer to very small value
    	RememberingTransportInput underlyingInput = new RememberingTransportInput();
    	underlyingInput.setInputBufferSize(1);
        
    	// set the SslInputBuffer and intermediateDecodeBuffer to large values
    	CapitalisingDummySslEngine dummySslEngine = new CapitalisingDummySslEngine();
        dummySslEngine.setApplicationBufferSize(100);
        dummySslEngine.setPacketBufferSize(100);
        
        SimpleSslTransportWrapper sslWrapper = new SimpleSslTransportWrapper(dummySslEngine, underlyingInput, null);
        
    	ByteBuffer byteBuffer = ByteBuffer.wrap("<-A-><-B-><-C->".getBytes());
    	pour(byteBuffer, sslWrapper.tail());
    	sslWrapper.process();
    	
        assertEquals("a_b_c_", underlyingInput.getAcceptedInput());
    }
}
