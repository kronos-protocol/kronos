#import <port_table_internal.h>


int hash_port_index(int port, int size) {
    return HASH_PORT_INDEX(port, size);
}