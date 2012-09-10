package org.apache.qpid.proton.type.transport;

import org.apache.qpid.proton.type.Binary;

public interface FrameBody
{
    interface FrameBodyHandler<E>
    {
        void handleOpen(Open open, Binary payload, E context);
        void handleBegin(Begin begin, Binary payload, E context);
        void handleAttach(Attach attach, Binary payload, E context);
        void handleFlow(Flow flow, Binary payload, E context);
        void handleTransfer(Transfer transfer, Binary payload, E context);
        void handleDisposition(Disposition disposition, Binary payload, E context);
        void handleDetach(Detach detach, Binary payload, E context);
        void handleEnd(End end, Binary payload, E context);
        void handleClose(Close close, Binary payload, E context);

    }

    <E> void invoke(FrameBodyHandler<E> handler, Binary payload, E context);
}
