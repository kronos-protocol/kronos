#include "kronos_server.h"
#include "server_internal.h"

#include <stdlib.h>


static UDPSocketDescriptor_t* s_get_descriptor(ServerPortManager_t* spm, Port_t port) {
    PortTableLookup_r lookup = krs_lib_port_table_lookup(spm->port_table, port);
    if (!lookup.exists) return NULL;
    return lookup.socket_handler;
}

Void_r krs_server_set_port_callback(ServerPortManager_t* spm, Port_t port,
                                    ChannelMessageCallback_f callback, void* user_data) {
    Void_r result = {0};
    if (!spm) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "spm is NULL");
        return result;
    }

    UDPSocketDescriptor_t* desc = s_get_descriptor(spm, port);
    if (!desc) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NOT_INITIALIZED, "no descriptor for port");
        return result;
    }

    AcquireSRWLockExclusive(&desc->state_lock);
    desc->port_callback = callback;
    desc->port_callback_user_data = user_data;
    ReleaseSRWLockExclusive(&desc->state_lock);

    result.base = krs_lib_error_result_base_suc();
    return result;
}

Void_r krs_server_set_channel_callback(ServerPortManager_t* spm, Port_t port, Channel_t channel,
                                       ChannelMessageCallback_f callback, void* user_data) {
    Void_r result = {0};
    if (!spm) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "spm is NULL");
        return result;
    }
    if (channel < 10) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_INVALID_PARAMETER, "channel < 10 is reserved");
        return result;
    }

    UDPSocketDescriptor_t* desc = s_get_descriptor(spm, port);
    if (!desc) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NOT_INITIALIZED, "no descriptor for port");
        return result;
    }

    AcquireSRWLockExclusive(&desc->state_lock);
    desc->channel_callbacks[channel] = callback;
    desc->channel_callback_user_data[channel] = user_data;
    ReleaseSRWLockExclusive(&desc->state_lock);

    result.base = krs_lib_error_result_base_suc();
    return result;
}

Void_r krs_server_set_connect_callback(ServerPortManager_t* spm, Port_t port,
                                       ConnectionLifecycleCallback_f callback, void* user_data) {
    Void_r result = {0};
    if (!spm) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "spm is NULL");
        return result;
    }

    UDPSocketDescriptor_t* desc = s_get_descriptor(spm, port);
    if (!desc) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NOT_INITIALIZED, "no descriptor for port");
        return result;
    }

    desc->connect_callback = callback;
    desc->connect_callback_user_data = user_data;
    result.base = krs_lib_error_result_base_suc();
    return result;
}

Void_r krs_server_set_disconnect_callback(ServerPortManager_t* spm, Port_t port,
                                          ConnectionLifecycleCallback_f callback, void* user_data) {
    Void_r result = {0};
    if (!spm) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "spm is NULL");
        return result;
    }

    UDPSocketDescriptor_t* desc = s_get_descriptor(spm, port);
    if (!desc) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NOT_INITIALIZED, "no descriptor for port");
        return result;
    }

    desc->disconnect_callback = callback;
    desc->disconnect_callback_user_data = user_data;
    result.base = krs_lib_error_result_base_suc();
    return result;
}

Void_r krs_server_set_channel_range_callback(ServerPortManager_t* spm, Port_t port,
                                             Channel_t from_channel, Channel_t to_channel,
                                             ChannelMessageCallback_f callback, void* user_data) {
    Void_r result = {0};
    if (!spm) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "spm is NULL");
        return result;
    }
    if (from_channel < 10) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_INVALID_PARAMETER, "from_channel < 10");
        return result;
    }
    if (to_channel < from_channel) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_INVALID_PARAMETER, "to_channel < from_channel");
        return result;
    }

    for (uint32_t ch = from_channel; ch <= to_channel; ch++) {
        Void_r r = krs_server_set_channel_callback(spm, port, (Channel_t)ch, callback, user_data);
        if (!r.base.valid) return r;
    }

    result.base = krs_lib_error_result_base_suc();
    return result;
}

Void_r krs_server_set_delivery_failure_callback(ServerPortManager_t* spm, Port_t port,
                                                DeliveryFailureCallback_f callback, void* user_data) {
    Void_r result = {0};
    if (!spm) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NULL_POINTER, "spm is NULL");
        return result;
    }

    UDPSocketDescriptor_t* desc = s_get_descriptor(spm, port);
    if (!desc) {
        result.base = krs_lib_error_result_base_w_msg(KRS_ERR_NOT_INITIALIZED, "no descriptor for port");
        return result;
    }

    desc->delivery_failure_callback = callback;
    desc->delivery_failure_callback_user_data = user_data;
    result.base = krs_lib_error_result_base_suc();
    return result;
}
