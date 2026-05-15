#include "../include/shared_state.h"

// HIP standalone weapon table.
const WeaponDef WEAPON_TABLE[WPN_COUNT] = {
    {"None", 0, 0, false},
    
    {"Solar Core", 10, 95, true},
    {"Lunar Blade", 10, 90, true},
    {"Iron Halberd", 7, 55, false},
    {"Venom Dagger", 4, 30, false},
    {"Thunderstaff", 6, 50, false},
    {"Obsidian Axe", 5, 45, false},
    {"Frostbow", 6, 48, false},
    {"Splinter Stick", 2, 12, false},
    
    {"Eclipse Relic", 8, 70, true},
};
