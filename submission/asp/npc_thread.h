#pragma once
#include "../include/shared_state.h"

// NPC pthread context.
struct NpcCtx {
    SharedState* state;
    int id;
};

void* npc_thread(void* arg);
