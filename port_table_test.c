#include <kronos_port_table.h>
#include <stdio.h>

int main() {
    PortTable_t* port_table = krs_lib_port_table_create();
    krs_lib_port_table_insert(port_table, 5);
    krs_lib_port_table_insert(port_table, 5);
    krs_lib_port_table_insert(port_table, 7);
    krs_lib_port_table_insert(port_table, 100);
    krs_lib_port_table_insert(port_table, 5);
    krs_lib_port_table_insert(port_table, 5);
    krs_lib_port_table_insert(port_table, 5);
    krs_lib_port_table_insert(port_table, 5);
    krs_lib_port_table_insert(port_table, 5);
    printf("hello\n");
}