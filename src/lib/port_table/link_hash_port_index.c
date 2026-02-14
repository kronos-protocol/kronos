#import <port_table_internal.h>


int hash_port_index(uint32_t port, uint32_t size) {
    return HASH_PORT_INDEX(port, size);
}