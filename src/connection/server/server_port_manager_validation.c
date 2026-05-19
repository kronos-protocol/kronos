#include "kronos_server.h"
#include "server_internal.h"


bool krs_server_port_manager_validate(ServerPortManager_t* server_port_manager) {
    if (!server_port_manager) return false;
    if (!server_port_manager->port_table) return false;
    return true;
}
