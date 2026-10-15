// ============================================================
// ESP32 DGB Miner - Mining Task (FreeRTOS, Core 1)
// ============================================================
#ifndef MINING_TASK_H
#define MINING_TASK_H

#include "config.h"

// Start the mining task pinned to Core 1
void startMiningTask();

// Stop the mining task
void stopMiningTask();

// Check if mining task is running
bool isMiningTaskRunning();

#endif // MINING_TASK_H
