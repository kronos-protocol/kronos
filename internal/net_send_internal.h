#ifndef NET_SEND_INTERNAL_H
#define NET_SEND_INTERNAL_H

#include "kronos_network.h"

void krs_net_send_frame(UDPSocketRef_t socket, const PortAddress_t* addr,
                        const uint8_t* frame_data, uint16_t frame_size);

#endif // NET_SEND_INTERNAL_H
