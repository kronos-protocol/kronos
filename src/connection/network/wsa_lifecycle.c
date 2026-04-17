#include "kronos_network.h"
#include "network_internal.h"

#include <winsock2.h>
#include <windows.h>


static volatile LONG s_wsa_ref_count = 0;
static CRITICAL_SECTION s_wsa_lock;
static volatile LONG s_lock_initialized = 0;

static void s_ensure_lock(void) {
    if (InterlockedCompareExchange(&s_lock_initialized, 1, 0) == 0) {
        InitializeCriticalSection(&s_wsa_lock);
    }
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
