#pragma once
#include "../include/shared_state.h"

// Artifact table functions declare.
void init_artifact_table(SharedState* state);


void sync_artifact_ownership_from_inventory(SharedState* state);


void grant_pending_resources(SharedState* state);

int artifact_resource_index(WeaponID w);


bool try_place_weapon(SharedState* state, int entity_id, WeaponID weapon);


WeaponID swap_in_entity(SharedState* state, int entity_id, int lts_index, int primary_evict_hint = -1);


void strip_artifact_weapons_from_entity(SharedState* state, int entity_id);
