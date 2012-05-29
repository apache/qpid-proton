package org.apache.qpid.proton.engine.impl;

import java.util.EnumSet;
import org.apache.qpid.proton.engine.EndpointState;

class EndpointImplQuery<T extends EndpointImpl> implements LinkNode.Query<T>
{
    private final EnumSet<EndpointState> _local;
    private final EnumSet<EndpointState> _remote;

    public EndpointImplQuery(EnumSet<EndpointState> local, EnumSet<EndpointState> remote)
    {
        _local = local;
        _remote = remote;
    }

    public boolean matches(LinkNode<T> node)
    {
        return (_local == null || _local.contains(node.getValue().getLocalState()))
                && (_remote == null || _remote.contains(node.getValue().getRemoteState()));
    }
}
