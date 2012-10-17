#ifndef PROTON_SASL_INTERNAL_H
#define PROTON_SASL_INTERNAL_H 1

#include <proton/sasl.h>

/** Decode input data bytes into SASL frames, and process them.
 *
 * This function is called by the driver layer to pass data received
 * from the remote peer into the SASL layer.
 *
 * @param[in] sasl the SASL layer.
 * @param[in] bytes buffer of frames to process
 * @param[in] available number of octets of data in 'bytes'
 * @return the number of bytes consumed, or error code if < 0
 */
ssize_t pn_sasl_input(pn_sasl_t *sasl, const char *bytes, size_t available);

/** Gather output frames from the layer.
 *
 * This function is used by the driver to poll the SASL layer for data
 * that will be sent to the remote peer.
 *
 * @param[in] sasl The SASL layer.
 * @param[out] bytes to be filled with encoded frames.
 * @param[in] size space available in bytes array.
 * @return the number of octets written to bytes, or error code if < 0
 */
ssize_t pn_sasl_output(pn_sasl_t *sasl, char *bytes, size_t size);

void pn_sasl_trace(pn_sasl_t *sasl, pn_trace_t trace);

/** Destructor for the given SASL layer.
 *
 * @param[in] sasl the SASL object to free. No longer valid on
 *                 return.
 */
void pn_sasl_free(pn_sasl_t *sasl);

#endif /* sasl-internal.h */
