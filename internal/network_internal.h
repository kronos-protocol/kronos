#ifndef NETWORK_INTERNAL_H
#define NETWORK_INTERNAL_H
#include "kronos_network.h"
#include "kronos_server.h"
#include "kronos_client.h"

struct Endpoint {
    Address_t address;
    UDPSocketRef_t udp_socket_ref;
    Channel_t channel;
};

void krs_wsa_init(void);
void krs_wsa_cleanup(void);

#endif //NETWORK_INTERNAL_H
