#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "../include/constants.h"
#include "../include/roll_seed.h"
#include "../include/shared_state.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <ctime>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "arbiter_signals.h"
#include "artifact_table.h"

// Main loop stamina tick turn pick action_submitted par wait combat apply.
extern long long now_ms();
extern long long now_mono_ms();
extern void log_action(SharedState* state, const char* text);
extern const char* entity_name(const SharedState* state, int id);
extern bool has_weapon(const Inventory& inv, WeaponID weapon);
extern bool allocate_weapon(Inventory& inv, WeaponID weapon);

static int pick_first_target(const SharedState* state, int actor, EntityType target_type) {
    for (int i = 0; i < state->num_players + state->num_npcs; ++i) {
        if (i == actor) continue;
        if (state->entities[i].status != DEAD && state->entities[i].type == target_type) return i;
    }
    return -1;
}

static int pick_pvp_target(const SharedState* state, int actor) {
    const int split = state->player2_start_idx;
    if (actor < split) {
        for (int i = split; i < state->num_players; ++i)
            if (state->entities[i].status != DEAD) return i;
    } else {
        for (int i = 0; i < split; ++i)
            if (state->entities[i].status != DEAD) return i;
    }
    return -1;
}

static void check_pvp_win(SharedState* state) {
    if (!state->pvp_mode) return;
    const int split = state->player2_start_idx;
    bool teamA_dead = true, teamB_dead = true;
    for (int i = 0; i < split; ++i)
        if (state->entities[i].status != DEAD) { teamA_dead = false; break; }
    for (int i = split; i < state->num_players; ++i)
        if (state->entities[i].status != DEAD) { teamB_dead = false; break; }
    if      (teamA_dead) state->game_status = GAME_TEAM_B_WINS;
    else if (teamB_dead) state->game_status = GAME_TEAM_A_WINS;
}

static int pick_preferred_player_target(const SharedState* state, int actor) {
    if (state->last_player_actor_id >= 0 &&
        state->last_player_actor_id < state->num_players &&
        state->entities[state->last_player_actor_id].status != DEAD &&
        state->last_player_actor_id != actor) {
        return state->last_player_actor_id;
    }
    return pick_first_target(state, actor, TYPE_PLAYER);
}

static WeaponID random_drop_weapon(const SharedState* state) {
    for (int t = 0; t < 64; ++t) {
        WeaponID w = static_cast<WeaponID>((rand() % (WPN_COUNT - 1)) + 1);
        if (!WEAPON_TABLE[w].is_artifact) return w;
        int r = artifact_resource_index(w);
        if (r >= 0 && state->resource_table.entries[r].present && state->resource_table.entries[r].held_by < 0) return w;
    }
    return WPN_SPLINTER_STICK;
}


static int random_alive_npc(const SharedState* state) {
    int pool[MAX_ENTITIES];
    int count = 0;
    for (int i = state->num_players; i < state->num_players + state->num_npcs; ++i) {
        if (state->entities[i].status != DEAD) pool[count++] = i;
    }
    if (count == 0) return -1;
    return pool[rand() % count];
}

extern void init_inventory(Inventory& inv);

static void respawn_npc(Entity& e, int id, int ui_label, int players) {
    e.id = id;
    e.ui_label = ui_label;
    e.type = TYPE_NPC;
    e.status = ALIVE;
    e.max_hp = (int)(ROLL_LAST_2 + (rand() % 151 + 50));
    e.hp = e.max_hp;
    e.damage = (int)(ROLL_SECOND_LAST + 10);
    e.speed = 10.0f + (float)(rand() % 21);
    e.max_stamina = 150.0f;
    e.stamina = 0.0f;
    e.turn_ready = false;
    
    
    e.stun_until_ms = 0;
    e.pending_action = {ACT_NONE, id, -1, WPN_NONE, -1, false};
    init_inventory(e.inventory);
    (void)players;
}

void scheduling_loop(SharedState* state) {
    int waiting_actor = -1;
    long long waiting_since_ms = 0;
    while (state->game_status == GAME_RUNNING) {
        sem_wait(&state->state_lock);
        sync_artifact_ownership_from_inventory(state);
        grant_pending_resources(state);
        sem_wait(&state->ui_mailbox.lock);
        if (state->drop_pending && state->drop_choice == -1) {
            int u = state->ui_mailbox.drop_user_pick;
            if (u == 0 || u == 1) {
                state->drop_choice = u;
                state->ui_mailbox.drop_user_pick = -1;
            }
        }
        sem_post(&state->ui_mailbox.lock);
        ++state->tick_count;
        long long now = now_ms();

        if (state->drop_pending && state->drop_choice == -1 && now >= state->drop_expire_ms) {
            state->drop_choice = 0;  
        }
        if (state->drop_pending && state->drop_choice != -1) {
            if (state->drop_choice == 1) {
                const int oid = state->drop_owner_id;
                bool picked = try_place_weapon(state, oid, state->drop_weapon);
                char msg[192];
                if (picked)
                    std::snprintf(msg, sizeof(msg), "DROP RESULT: Player_%d picked %s (placed/LTS).", oid,
                                  WEAPON_TABLE[state->drop_weapon].name);
                else if (state->pending_artifact_grant[oid] == state->drop_weapon)
                    std::snprintf(msg, sizeof(msg),
                                  "DROP RESULT: Player_%d waits on exclusive %s (held by another entity).", oid,
                                  WEAPON_TABLE[state->drop_weapon].name);
                else
                    std::snprintf(msg, sizeof(msg), "DROP RESULT: Player_%d could not fit %s (inventory/LTS).", oid,
                                  WEAPON_TABLE[state->drop_weapon].name);
                log_action(state, msg);
            } else {
                int npc = random_alive_npc(state);
                if (npc >= 0) {
                    bool picked = try_place_weapon(state, npc, state->drop_weapon);
                    char msg[192];
                    if (picked)
                        std::snprintf(msg, sizeof(msg), "DROP RESULT: Enemy_%d took %s (placed/LTS).",
                                      state->entities[npc].ui_label, WEAPON_TABLE[state->drop_weapon].name);
                    else if (state->pending_artifact_grant[npc] == state->drop_weapon)
                        std::snprintf(msg, sizeof(msg),
                                      "DROP RESULT: Enemy_%d waits on exclusive %s (another holder).",
                                      state->entities[npc].ui_label, WEAPON_TABLE[state->drop_weapon].name);
                    else
                        std::snprintf(msg, sizeof(msg), "DROP RESULT: Enemy_%d could not fit %s.",
                                      state->entities[npc].ui_label, WEAPON_TABLE[state->drop_weapon].name);
                    log_action(state, msg);
                } else {
                    log_action(state, "DROP RESULT: No alive enemy to claim declined weapon.");
                }
            }
            state->drop_pending = false;
            state->drop_owner_id = -1;
            state->drop_source_enemy_label = -1;
            state->drop_weapon = WPN_NONE;
            state->drop_expire_ms = 0;
            state->drop_choice = -1;
        }

        if (state->tick_count >= 100 && !state->resource_table.entries[2].present && !state->eclipse_env_offer) {
            state->eclipse_env_offer = true;
            log_action(state,
                       "ECLIPSE RELIC appears in the arena — on your turn press P (pickup) to claim it (uses full turn).");
        }

        for (int i = 0; i < state->num_players + state->num_npcs; ++i) {
            auto& e = state->entities[i];
            if (e.status == STUNNED && now >= e.stun_until_ms) e.status = ALIVE;
            if (e.status == DEAD) continue;
            
            if (e.status == ALIVE) {
                e.stamina = std::min(e.max_stamina, e.stamina + e.speed * (TICK_INTERVAL_MS / 1000.0f));
            }
        }
        
        
        if (waiting_actor == -1) {
            const int n = state->num_players + state->num_npcs;
            if (n > 0) {
                const int start = state->sched_rr_next % n;
                for (int k = 0; k < n; ++k) {
                    const int i = (start + k) % n;
                    if (state->drop_pending && i == state->drop_owner_id) continue;
                    
                    if (arbiter_ultimate_asp_is_frozen() && i >= state->num_players) continue;
                    
                    if (state->pvp_mode && state->pvp_last_team != -1) {
                        const int entity_team = (i < state->player2_start_idx) ? 0 : 1;
                        if (entity_team == state->pvp_last_team) continue;
                    }
                    if (state->entities[i].status == ALIVE && state->entities[i].stamina >= state->entities[i].max_stamina) {
                        waiting_actor = i;
                        waiting_since_ms = now;
                        state->active_entity_id = i;
                        state->entities[i].turn_ready = true;
                        state->sched_rr_next = (i + 1) % n;
                        char msg[128];
                        std::snprintf(msg, sizeof(msg), "TURN READY: %s", entity_name(state, i));
                        log_action(state, msg);
                        break;
                    }
                }
            }
        }

        if (waiting_actor != -1) {
            bool got_action = (waiting_actor < state->num_players) ? state->input_buffer[waiting_actor].ready
                                                                    : state->entities[waiting_actor].pending_action.ready;
            bool timed_out = (now - waiting_since_ms) >= 3000;
            
            bool npc_frozen = (waiting_actor >= state->num_players && arbiter_ultimate_asp_is_frozen());
            if (!got_action && !timed_out && !npc_frozen) {
                sem_post(&state->state_lock);
                sem_post(&state->render_ready);
                
                struct timespec ts {};
                clock_gettime(CLOCK_REALTIME, &ts);
                long add_ms = TICK_INTERVAL_MS;
                ts.tv_sec += add_ms / 1000;
                ts.tv_nsec += (add_ms % 1000) * 1000000L;
                if (ts.tv_nsec >= 1000000000L) {
                    ts.tv_sec++;
                    ts.tv_nsec -= 1000000000L;
                }
                int w = 0;
                do {
                    w = sem_timedwait(&state->action_submitted, &ts);
                } while (w == -1 && errno == EINTR);
                (void)w;
                continue;
            }

            Action act {};
            if (waiting_actor < state->num_players) act = state->input_buffer[waiting_actor];
            else act = state->entities[waiting_actor].pending_action;
            if (waiting_actor < state->num_players) state->last_player_actor_id = waiting_actor;
            if (!act.ready) {
                if (state->entities[waiting_actor].type == TYPE_NPC) {
                    act = {ACT_SKIP, waiting_actor, -1, WPN_NONE, -1, true};
                    char msg[128];
                    std::snprintf(msg, sizeof(msg), "TURN TIMEOUT: %s auto-SKIP (3s).", entity_name(state, waiting_actor));
                    log_action(state, msg);
                } else {
                    act = {ACT_SKIP, waiting_actor, -1, WPN_NONE, -1, true};
                    char msg[128];
                    std::snprintf(msg, sizeof(msg), "TURN TIMEOUT: %s forced SKIP.", entity_name(state, waiting_actor));
                    log_action(state, msg);
                }
            }

            
            
            const int swap_evict_hint = (act.type == ACT_SWAP_IN) ? act.target_id : -1;

            auto& actor = state->entities[waiting_actor];
            int target = (act.type == ACT_SWAP_IN) ? -1 : act.target_id;
            if (target < 0) {
                if (state->pvp_mode && actor.type == TYPE_PLAYER) {
                    target = pick_pvp_target(state, waiting_actor);
                } else {
                    target = (actor.type == TYPE_PLAYER)
                                 ? pick_first_target(state, waiting_actor, TYPE_NPC)
                                 : pick_preferred_player_target(state, waiting_actor);
                }
            }
            
            if (state->pvp_mode && target >= 0 && target < state->num_players) {
                const int split = state->player2_start_idx;
                const bool actor_is_A  = (waiting_actor < split);
                const bool target_is_A = (target < split);
                if (actor_is_A == target_is_A)
                    target = pick_pvp_target(state, waiting_actor);
            }
            
            if (target >= 0 && target < MAX_ENTITIES) {
                if (state->pvp_mode && actor.type == TYPE_PLAYER) {
                    
                    if (state->entities[target].status == DEAD)
                        target = pick_pvp_target(state, waiting_actor);
                } else {
                    const EntityType expected = (actor.type == TYPE_PLAYER) ? TYPE_NPC : TYPE_PLAYER;
                    if (state->entities[target].status == DEAD || state->entities[target].type != expected) {
                        target = (actor.type == TYPE_PLAYER)
                                     ? pick_first_target(state, waiting_actor, TYPE_NPC)
                                     : pick_preferred_player_target(state, waiting_actor);
                    }
                }
            }

            if (act.type == ACT_STRIKE && target >= 0) {
                
                int before_hp = state->entities[target].hp;
                state->entities[target].hp -= actor.damage;
                if (state->entities[target].hp < 0) state->entities[target].hp = 0;
                char msg[160];
                std::snprintf(msg, sizeof(msg), "%s strikes %s for %d (HP %d->%d).",
                    entity_name(state, waiting_actor), entity_name(state, target),
                    actor.damage, before_hp, state->entities[target].hp);
                log_action(state, msg);
            } else if (act.type == ACT_EXHAUST && target >= 0) {
                float before_st = state->entities[target].stamina;
                state->entities[target].stamina = std::max(0.0f, state->entities[target].stamina - (float)actor.damage);
                char msg[160];
                std::snprintf(msg, sizeof(msg), "%s exhausts %s (ST %.1f->%.1f).", entity_name(state, waiting_actor),
                              entity_name(state, target), before_st, state->entities[target].stamina);
                log_action(state, msg);
            } else if (act.type == ACT_USE_WEAPON && target >= 0 && has_weapon(actor.inventory, act.weapon_id)) {
                int before_hp = state->entities[target].hp;
                state->entities[target].hp -= WEAPON_TABLE[act.weapon_id].damage;
                if (state->entities[target].hp < 0) state->entities[target].hp = 0;
                
                const bool is_artifact_weapon = WEAPON_TABLE[act.weapon_id].is_artifact;
                if (is_artifact_weapon && state->entities[target].hp > 0) {
                    state->entities[target].status = STUNNED;
                    state->entities[target].stun_until_ms = now_ms() + 3000;
                    if (state->entities[target].pid > 0 && state->entities[target].linux_tid > 0)
                        (void)tgkill(state->entities[target].pid, state->entities[target].linux_tid, SIGUSR1);
                }
                char msg[192];
                std::snprintf(msg, sizeof(msg), "%s uses %s on %s (HP %d->%d%s).", entity_name(state, waiting_actor),
                              WEAPON_TABLE[act.weapon_id].name, entity_name(state, target), before_hp,
                              state->entities[target].hp,
                              is_artifact_weapon && state->entities[target].hp > 0 ? ", 3s stun" : "");
                log_action(state, msg);
            } else if (act.type == ACT_SWAP_IN) {
                const int lts_before = state->entities[waiting_actor].inventory.lts_count;
                
                WeaponID sw = swap_in_entity(state, waiting_actor, act.aux_index, swap_evict_hint);
                if (sw != WPN_NONE) {
                    char msg[192];
                    std::snprintf(msg, sizeof(msg),
                                  "SWAP IN: %s moved to primary inventory  (%d item(s) remain in LTS).",
                                  WEAPON_TABLE[sw].name,
                                  state->entities[waiting_actor].inventory.lts_count);
                    log_action(state, msg);
                } else if (state->pending_artifact_grant[waiting_actor] != WPN_NONE) {
                    log_action(state, "Swap In blocked: waiting on exclusive artifact held by another entity.");
                } else if (lts_before == 0) {
                    log_action(state, "Swap In failed: LTS is empty.");
                } else {
                    log_action(state, "Swap In failed: primary inventory full and no eviction path available.");
                }
            } else if (act.type == ACT_PICKUP) {
                if (try_place_weapon(state, waiting_actor, WPN_ECLIPSE_RELIC)) {
                    log_action(state, "Eclipse Relic picked up from the arena.");
                } else if (state->pending_artifact_grant[waiting_actor] == WPN_ECLIPSE_RELIC) {
                    log_action(state, "Eclipse Relic is held by another — queued until it is free.");
                } else if (state->eclipse_env_offer) {
                    log_action(state, "Could not pick up Eclipse (inventory full or no space). Offer remains.");
                } else {
                    log_action(state, "Nothing to pick up.");
                }
            } else if (act.type == ACT_HEAL) {
                int before = actor.hp;
                actor.hp = std::min(actor.max_hp, actor.hp + (int)(actor.max_hp * 0.1f));
                char msg[128]; std::snprintf(msg, sizeof(msg), "%s heals %d.", entity_name(state, waiting_actor), actor.hp - before); log_action(state, msg);
            } else if (act.type == ACT_ULTIMATE && actor.type == TYPE_PLAYER && has_weapon(actor.inventory, WPN_SOLAR_CORE) && has_weapon(actor.inventory, WPN_LUNAR_BLADE)) {
                arbiter_ultimate_freeze_asp(state);
                log_action(state, "ULTIMATE activated: ASP frozen via SIGSTOP; SIGALRM+SIGCONT in 10s (signals only).");
            } else {
                log_action(state, "Turn skipped.");
            }

            if (target >= 0 && state->entities[target].hp <= 0) {
                state->entities[target].status = DEAD;
                state->waits_for_resource[target] = -1;
                state->pending_artifact_grant[target] = WPN_NONE;
                state->entities[target].turn_ready = false;
                state->entities[target].pending_action = {ACT_NONE, target, -1, WPN_NONE, -1, false};
                if (target < state->num_players) {
                    state->input_buffer[target].ready = false;
                    sem_wait(&state->ui_mailbox.lock);
                    state->ui_mailbox.player_intent[target].valid = false;
                    sem_post(&state->ui_mailbox.lock);
                }
                if (state->entities[target].type == TYPE_NPC) {
                    ++state->enemies_killed;
                    if (actor.type == TYPE_PLAYER) {
                        
                        
                        bool npc_had_weapon = false;
                        for (int s = 0; s < INVENTORY_SIZE; ++s) {
                            if (state->entities[target].inventory.slots[s] != WPN_NONE) {
                                npc_had_weapon = true; break;
                            }
                        }
                        
                        
                        bool any_npc_alive = false;
                        for (int _i = state->num_players; _i < state->num_players + state->num_npcs; ++_i)
                            if (state->entities[_i].status != DEAD) { any_npc_alive = true; break; }
                        WeaponID drop = WPN_NONE;
                        if (!npc_had_weapon && any_npc_alive && (rand() % 100) < DROP_CHANCE_PERCENT) {
                            drop = random_drop_weapon(state);
                        }
                        if (drop != WPN_NONE) {
                            char dmsg[192];
                            std::snprintf(dmsg, sizeof(dmsg), "%s defeated %s. DROP: %s (choose pickup/decline in 8s).",
                                          entity_name(state, waiting_actor), entity_name(state, target), WEAPON_TABLE[drop].name);
                            log_action(state, dmsg);
                            state->drop_pending = true;
                            state->drop_owner_id = waiting_actor;
                            state->drop_source_enemy_label = state->entities[target].ui_label;
                            state->drop_weapon = drop;
                            state->drop_expire_ms = now_ms() + 8000;
                            state->drop_choice = -1;
                            sem_wait(&state->ui_mailbox.lock);
                            state->ui_mailbox.drop_user_pick = -1;
                            sem_post(&state->ui_mailbox.lock);
                        } else {
                            char nd[128];
                            std::snprintf(nd, sizeof(nd), "%s defeated %s. No weapon dropped (chance miss).",
                                          entity_name(state, waiting_actor), entity_name(state, target));
                            log_action(state, nd);
                        }
                    }

                    if (state->enemies_killed < 10 && state->total_enemy_spawned < 10) {
                        ++state->total_enemy_spawned;
                        respawn_npc(state->entities[target], target, state->total_enemy_spawned, state->num_players);
                        char rmsg[128];
                        std::snprintf(rmsg, sizeof(rmsg), "RESPAWN: Enemy_%d entered the arena.", state->total_enemy_spawned);
                        log_action(state, rmsg);
                    }
                }
            }
            actor.stamina = (act.type == ACT_SKIP) ? actor.max_stamina * 0.5f : 0.0f;
            actor.turn_ready = false;
            state->active_entity_id = -1;
            
            if (state->pvp_mode && waiting_actor >= 0)
                state->pvp_last_team = (waiting_actor < state->player2_start_idx) ? 0 : 1;
            if (waiting_actor < state->num_players) state->input_buffer[waiting_actor].ready = false;
            else state->entities[waiting_actor].pending_action.ready = false;
            waiting_actor = -1;
            waiting_since_ms = 0;

            bool players_alive = false;
            
            for (int i = 0; i < state->num_players; ++i) players_alive |= (state->entities[i].status != DEAD);
            if (!players_alive) state->game_status = GAME_LOST;
            if (state->enemies_killed >= 10) state->game_status = GAME_WON;
            check_pvp_win(state);  
        }

        sem_post(&state->state_lock);
        sem_post(&state->render_ready);
        usleep(TICK_INTERVAL_MS * 1000);
    }
}
