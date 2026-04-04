#include "trampoline_pool.h"
#include <windows.h>
#include <cstring>

static uint8_t s_poolBacking[TRAMPOLINE_POOL_SIZE] __attribute__((aligned(16)));
static int s_poolOffset = 0;

uint8_t* g_trampolinePool = nullptr;

bool TrampolinePool_Init() {
    DWORD oldProtect;
    if (!VirtualProtect(s_poolBacking, TRAMPOLINE_POOL_SIZE,
                        PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;
    memset(s_poolBacking, 0xCC, TRAMPOLINE_POOL_SIZE);
    g_trampolinePool = s_poolBacking;
    return true;
}

void TrampolinePool_Shutdown() {
    if (g_trampolinePool) {
        DWORD oldProtect;
        VirtualProtect(s_poolBacking, TRAMPOLINE_POOL_SIZE,
                       PAGE_READONLY, &oldProtect);
    }
    g_trampolinePool = nullptr;
    s_poolOffset = 0;
}

int TrampolineAlloc(int size) {
    if (!g_trampolinePool) return -1;
    if (s_poolOffset + size > TRAMPOLINE_POOL_SIZE) return -1;
    int offset = s_poolOffset;
    s_poolOffset += size;
    return offset;
}
