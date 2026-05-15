#include "npc_ai.h"
#include <cstdlib>

// Simple AI target pick skip pickup rules.
extern bool has_weapon(const Inventory& inv, WeaponID weapon);

[[maybe_unused]] static WeaponID best_weapon_for_npc(const Inventory& inv) {
    WeaponID best = WPN_NONE;
    int best_damage = -1;
    for (int i = 0; i < INVENTORY_SIZE; ++i) {
        WeaponID w = inv.slots[i];
        if (w == WPN_NONE) continue;
        int dmg = WEAPON_TABLE[w].damage;
        if (dmg > best_damage) {
            best_damage = dmg;
            best = w;
        }
    }
    return best;
}

Action npc_decide(SharedState* state, int npc_id) {
    
    if (state->eclipse_env_offer && !has_weapon(state->entities[npc_id].inventory, WPN_ECLIPSE_RELIC)) {
        return {ACT_PICKUP, npc_id, -1, WPN_ECLIPSE_RELIC, -1, true};
    }

    if (rand() % 10 < 3) return {ACT_SKIP, npc_id, -1, WPN_NONE, -1, true};

    Action a{ACT_SKIP, npc_id, -1, WPN_NONE, -1, true};
    int target = -1;
    if (state->last_player_actor_id >= 0 &&
        state->last_player_actor_id < state->num_players &&
        state->entities[state->last_player_actor_id].status == ALIVE) {
        target = state->last_player_actor_id;
    } else {
        int hp = 1 << 30;
        for (int i = 0; i < state->num_players; ++i) {
            if (state->entities[i].status == ALIVE && state->entities[i].hp < hp) {
                hp = state->entities[i].hp;
                target = i;
            }
        }
    }
    if (target >= 0) {
        
        WeaponID art = WPN_NONE;
        const Inventory& inv = state->entities[npc_id].inventory;
        for (int s = 0; s < INVENTORY_SIZE; ++s) {
            WeaponID w = inv.slots[s];
            if (w != WPN_NONE && WEAPON_TABLE[w].is_artifact) { art = w; break; }
        }
        a = (art != WPN_NONE) ? Action{ACT_USE_WEAPON, npc_id, target, art, -1, true}
                               : Action{ACT_STRIKE,    npc_id, target, WPN_NONE, -1, true};
    }
    return a;
}
