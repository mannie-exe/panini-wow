#pragma once
#include <cstdint>

constexpr int TRAMPOLINE_POOL_SIZE = 512;

extern "C" uint8_t* g_trampolinePool;

bool TrampolinePool_Init();
void TrampolinePool_Shutdown();
int TrampolineAlloc(int size);
