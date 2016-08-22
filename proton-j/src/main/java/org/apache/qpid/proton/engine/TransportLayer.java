/*
 * Copyright (c) Microsoft. All rights reserved.
 * Licensed under the MIT license. See LICENSE file in the project root for full license information.
 */
package org.apache.qpid.proton.engine;

import org.apache.qpid.proton.engine.impl.TransportInput;
import org.apache.qpid.proton.engine.impl.TransportOutput;
import org.apache.qpid.proton.engine.impl.TransportWrapper;

public interface TransportLayer {

    public TransportWrapper wrap(final TransportInput input, final TransportOutput output);

}
