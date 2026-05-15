#pragma once
#include "../include/shared_state.h"

// Player pthread entry.
struct PlayerCtx {
    SharedState* state;
    int id;
};

void* player_thread(void* arg);
