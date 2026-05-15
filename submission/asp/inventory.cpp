#include "../include/shared_state.h"
#include <climits>
#include <cstring>
#include <set>

// ASP copy inventory helpers.
void init_inventory(Inventory& inv) {
    std::memset(&inv, 0, sizeof(Inventory));
    for (int i = 0; i < INVENTORY_SIZE; ++i) inv.slots[i] = WPN_NONE;
}

bool has_weapon(const Inventory& inv, WeaponID weapon) {
    for (int i = 0; i < INVENTORY_SIZE; ++i) if (inv.slots[i] == weapon) return true;
    return false;
}

void remove_weapon_from_inventory(Inventory& inv, WeaponID weapon) {
    for (int i = 0; i < INVENTORY_SIZE; ++i) if (inv.slots[i] == weapon) inv.slots[i] = WPN_NONE;
}

static int weapon_left_edge(const Inventory& inv, int idx) {
    if (idx < 0 || idx >= INVENTORY_SIZE) return -1;
    WeaponID w = inv.slots[idx];
    if (w == WPN_NONE) return -1;
    int left = idx;
    while (left > 0 && inv.slots[left - 1] == w) --left;
    return left;
}


static void evict_weapon_instance_at(Inventory& inv, int left) {
    if (left < 0 || left >= INVENTORY_SIZE) return;
    WeaponID w = inv.slots[left];
    if (w == WPN_NONE) return;
    for (int i = left; i < INVENTORY_SIZE && inv.slots[i] == w; ++i) inv.slots[i] = WPN_NONE;
    if (inv.lts_count < MAX_WEAPONS_LTS) inv.lts[inv.lts_count++] = w;
}

bool allocate_weapon(Inventory& inv, WeaponID weapon) {
    if (weapon == WPN_NONE) return false;
    const int need = WEAPON_TABLE[weapon].slot_size;
    if (need <= 0 || need > INVENTORY_SIZE) return false;

    for (int p = 0; p <= INVENTORY_SIZE - need; ++p) {
        bool ok = true;
        for (int i = p; i < p + need; ++i) ok &= (inv.slots[i] == WPN_NONE);
        if (ok) {
            for (int i = p; i < p + need; ++i) inv.slots[i] = weapon;
            return true;
        }
    }

    int best_p = -1;
    int best_cost = INT_MAX;
    for (int p = 0; p <= INVENTORY_SIZE - need; ++p) {
        std::set<int> roots;
        for (int i = p; i < p + need; ++i) {
            if (inv.slots[i] != WPN_NONE) {
                int le = weapon_left_edge(inv, i);
                if (le >= 0) roots.insert(le);
            }
        }
        const int cost = (int)roots.size();
        if (cost < best_cost) {
            best_cost = cost;
            best_p = p;
        }
    }
    if (best_p < 0 || best_cost == INT_MAX) return false;

    
    
    std::set<int> roots;
    for (int i = best_p; i < best_p + need; ++i) {
        if (inv.slots[i] != WPN_NONE) {
            int le = weapon_left_edge(inv, i);
            if (le >= 0) roots.insert(le);
        }
    }
    if (inv.lts_count + (int)roots.size() > MAX_WEAPONS_LTS) return false;

    for (int left : roots) evict_weapon_instance_at(inv, left);

    
    for (int i = best_p; i < best_p + need; ++i) {
        if (inv.slots[i] != WPN_NONE) return false;
    }
    for (int i = best_p; i < best_p + need; ++i) inv.slots[i] = weapon;
    return true;
}

