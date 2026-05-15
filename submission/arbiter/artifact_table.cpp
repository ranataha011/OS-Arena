#include "artifact_table.h"

// Solar Lunar Eclipse table lock inventory ke sath sync grant pending.
extern bool has_weapon(const Inventory& inv, WeaponID weapon);
extern void remove_weapon_from_inventory(Inventory& inv, WeaponID weapon);
extern bool allocate_weapon(Inventory& inv, WeaponID weapon);

void init_artifact_table(SharedState* state) {
    state->resource_table.entries[0] = {WPN_SOLAR_CORE, true, -1};
    state->resource_table.entries[1] = {WPN_LUNAR_BLADE, true, -1};
    state->resource_table.entries[2] = {WPN_ECLIPSE_RELIC, false, -1};
}

int artifact_resource_index(WeaponID w) {
    if (w == WPN_SOLAR_CORE) return 0;
    if (w == WPN_LUNAR_BLADE) return 1;
    if (w == WPN_ECLIPSE_RELIC) return 2;
    return -1;
}

void sync_artifact_ownership_from_inventory(SharedState* state) {
    sem_wait(&state->resource_table.table_lock);
    int solar = -1;
    int lunar = -1;
    int eclipse = -1;
    const int n = state->num_players + state->num_npcs;
    for (int i = 0; i < n; ++i) {
        if (state->entities[i].status == DEAD) continue;
        const Inventory& inv = state->entities[i].inventory;
        if (has_weapon(inv, WPN_SOLAR_CORE)) solar = i;
        if (has_weapon(inv, WPN_LUNAR_BLADE)) lunar = i;
        if (has_weapon(inv, WPN_ECLIPSE_RELIC)) eclipse = i;
    }
    if (state->resource_table.entries[0].present)
        state->resource_table.entries[0].held_by = solar;
    else
        state->resource_table.entries[0].held_by = -1;
    if (state->resource_table.entries[1].present)
        state->resource_table.entries[1].held_by = lunar;
    else
        state->resource_table.entries[1].held_by = -1;
    if (state->resource_table.entries[2].present)
        state->resource_table.entries[2].held_by = eclipse;
    else
        state->resource_table.entries[2].held_by = -1;

    for (int i = 0; i < n; ++i) {
        Entity& e = state->entities[i];
        e.holds_solar_core = has_weapon(e.inventory, WPN_SOLAR_CORE);
        e.holds_lunar_blade = has_weapon(e.inventory, WPN_LUNAR_BLADE);
        e.holds_eclipse_relic = has_weapon(e.inventory, WPN_ECLIPSE_RELIC);
        int w = state->waits_for_resource[i];
        if (w >= 0 && w <= 2) {
            int hb = state->resource_table.entries[w].held_by;
            if (hb == i) {
                state->waits_for_resource[i] = -1;
                state->pending_artifact_grant[i] = WPN_NONE;
            }
        }
    }
    sem_post(&state->resource_table.table_lock);
}

void grant_pending_resources(SharedState* state) {
    sem_wait(&state->resource_table.table_lock);
    const int n = state->num_players + state->num_npcs;
    for (int e = 0; e < n; ++e) {
        if (state->entities[e].status == DEAD) {
            state->waits_for_resource[e] = -1;
            state->pending_artifact_grant[e] = WPN_NONE;
            continue;
        }
        WeaponID pw = state->pending_artifact_grant[e];
        if (pw == WPN_NONE) {
            int r = state->waits_for_resource[e];
            if (r >= 0 && r <= 2 && !state->resource_table.entries[r].present) state->waits_for_resource[e] = -1;
            continue;
        }
        int r = state->waits_for_resource[e];
        if (r < 0 || r > 2 || !state->resource_table.entries[r].present) {
            state->waits_for_resource[e] = -1;
            state->pending_artifact_grant[e] = WPN_NONE;
            continue;
        }
        int h = state->resource_table.entries[r].held_by;
        if (h >= 0 && h != e) continue;

        sem_post(&state->resource_table.table_lock);
        state->waits_for_resource[e] = -1;
        state->pending_artifact_grant[e] = WPN_NONE;
        (void)allocate_weapon(state->entities[e].inventory, pw);
        sem_wait(&state->resource_table.table_lock);
    }
    sem_post(&state->resource_table.table_lock);
}

bool try_place_weapon(SharedState* state, int entity_id, WeaponID weapon) {
    if (weapon == WPN_NONE || entity_id < 0 || entity_id >= MAX_ENTITIES) return false;
    Inventory& inv = state->entities[entity_id].inventory;
    if (has_weapon(inv, weapon)) return true;

    if (weapon == WPN_ECLIPSE_RELIC) {
        sem_wait(&state->resource_table.table_lock);
        const bool env_first = state->eclipse_env_offer && !state->resource_table.entries[2].present;
        sem_post(&state->resource_table.table_lock);

        if (env_first) {
            bool ok = allocate_weapon(inv, weapon);
            if (ok) {
                sem_wait(&state->resource_table.table_lock);
                state->eclipse_env_offer = false;
                state->resource_table.entries[2].present = true;
                sem_post(&state->resource_table.table_lock);
            }
            return ok;
        }

        sem_wait(&state->resource_table.table_lock);
        if (!state->resource_table.entries[2].present) {
            sem_post(&state->resource_table.table_lock);
            return false;
        }
        int h = state->resource_table.entries[2].held_by;
        if (h >= 0 && h != entity_id) {
            state->waits_for_resource[entity_id] = 2;
            state->pending_artifact_grant[entity_id] = weapon;
            sem_post(&state->resource_table.table_lock);
            return false;
        }
        sem_post(&state->resource_table.table_lock);
        return allocate_weapon(inv, weapon);
    }

    if (!WEAPON_TABLE[weapon].is_artifact) return allocate_weapon(inv, weapon);

    sem_wait(&state->resource_table.table_lock);
    int r = artifact_resource_index(weapon);
    if (r < 0 || !state->resource_table.entries[r].present) {
        sem_post(&state->resource_table.table_lock);
        return allocate_weapon(inv, weapon);
    }
    int h = state->resource_table.entries[r].held_by;
    if (h >= 0 && h != entity_id) {
        state->waits_for_resource[entity_id] = r;
        state->pending_artifact_grant[entity_id] = weapon;
        sem_post(&state->resource_table.table_lock);
        return false;
    }
    sem_post(&state->resource_table.table_lock);
    return allocate_weapon(inv, weapon);
}

WeaponID swap_in_entity(SharedState* state, int entity_id, int lts_index, int primary_evict_hint) {
    Inventory& inv = state->entities[entity_id].inventory;
    if (inv.lts_count == 0) return WPN_NONE;
    if (lts_index < 0 || lts_index >= inv.lts_count) lts_index = 0;  

    
    if (primary_evict_hint >= 0 && primary_evict_hint < INVENTORY_SIZE) {
        WeaponID ew = inv.slots[primary_evict_hint];
        if (ew != WPN_NONE && !WEAPON_TABLE[ew].is_artifact) {
            
            int left = primary_evict_hint;
            while (left > 0 && inv.slots[left - 1] == ew) --left;
            for (int i = left; i < INVENTORY_SIZE && inv.slots[i] == ew; ++i) inv.slots[i] = WPN_NONE;
            if (inv.lts_count < MAX_WEAPONS_LTS) inv.lts[inv.lts_count++] = ew;
        }
    }

    WeaponID w = inv.lts[lts_index];
    if (!try_place_weapon(state, entity_id, w)) return WPN_NONE;
    for (int i = lts_index; i < inv.lts_count - 1; ++i) inv.lts[i] = inv.lts[i + 1];
    --inv.lts_count;
    return w;
}

void strip_artifact_weapons_from_entity(SharedState* state, int entity_id) {
    if (entity_id < 0 || entity_id >= MAX_ENTITIES) return;
    Inventory& inv = state->entities[entity_id].inventory;
    remove_weapon_from_inventory(inv, WPN_SOLAR_CORE);
    remove_weapon_from_inventory(inv, WPN_LUNAR_BLADE);
    remove_weapon_from_inventory(inv, WPN_ECLIPSE_RELIC);
    state->entities[entity_id].holds_solar_core = false;
    state->entities[entity_id].holds_lunar_blade = false;
    state->entities[entity_id].holds_eclipse_relic = false;
    state->waits_for_resource[entity_id] = -1;
    state->pending_artifact_grant[entity_id] = WPN_NONE;
}
