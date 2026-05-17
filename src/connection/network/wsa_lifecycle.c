#include "kronos_network.h"
#include "network_internal.h"

#include <winsock2.h>
#include <windows.h>


static volatile LONG s_wsa_ref_count = 0;
static CRITICAL_SECTION s_wsa_lock;
static INIT_ONCE s_wsa_lock_init_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK s_init_wsa_lock(PINIT_ONCE init_once, PVOID param, PVOID* context) {
    (void)init_once;
    (void)param;
    (void)context;
    InitializeCriticalSection(&s_wsa_lock);
    return TRUE;
}

static void s_ensure_lock(void) {
    InitOnceExecuteOnce(&s_wsa_lock_init_once, s_init_wsa_lock, NULL, NULL);
}

void krs_wsa_init(void) {
    s_ensure_lock();
    EnterCriticalSection(&s_wsa_lock);
    if (InterlockedIncrement(&s_wsa_ref_count) == 1) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }
    LeaveCriticalSection(&s_wsa_lock);
}

void krs_wsa_cleanup(void) {
    s_ensure_lock();
    EnterCriticalSection(&s_wsa_lock);
    if (InterlockedDecrement(&s_wsa_ref_count) == 0) {
        WSACleanup();
    }
    LeaveCriticalSection(&s_wsa_lock);
}
