#pragma once

// Signal setup aur ultimate freeze helpers declare.
struct SharedState;

void setup_arbiter_signals(SharedState* state);


void arbiter_ultimate_freeze_asp(SharedState* state);


bool arbiter_ultimate_asp_is_frozen();
