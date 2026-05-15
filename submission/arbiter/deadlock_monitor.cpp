#include "../include/shared_state.h"
#include "artifact_table.h"
#include <algorithm>
#include <cstdio>
#include <unistd.h>
#include <vector>

// Background thread artifact wait graph pe cycle dhondh ke victim strip.
extern void log_action(SharedState* state, const char* text);

static bool dfs_cycle_util(int u, const std::vector<std::vector<int>>& adj, std::vector<int>& color, std::vector<int>& stack,
                           int& cycle_max) {
    color[u] = 1;
    stack.push_back(u);
    for (int v : adj[u]) {
        if (color[v] == 0) {
            if (dfs_cycle_util(v, adj, color, stack, cycle_max)) return true;
        } else if (color[v] == 1) {
            cycle_max = v;
            for (int i = (int)stack.size() - 1; i >= 0; --i) {
                cycle_max = std::max(cycle_max, stack[i]);
                if (stack[i] == v) break;
            }
            return true;
        }
    }
    stack.pop_back();
    color[u] = 2;
    return false;
}

static int find_waitfor_cycle_victim(SharedState* state, int n) {
    std::vector<std::vector<int>> adj(n);
    for (int e = 0; e < n; ++e) {
        int r = state->waits_for_resource[e];
        if (r < 0 || r > 2) continue;
        int h = state->resource_table.entries[r].held_by;
        if (h >= 0 && h < n && h != e) adj[e].push_back(h);
    }
    std::vector<int> color(n, 0);
    std::vector<int> stack;
    for (int i = 0; i < n; ++i) {
        if (color[i] != 0) continue;
        int cycle_max = -1;
        stack.clear();
        if (dfs_cycle_util(i, adj, color, stack, cycle_max) && cycle_max >= 0) return cycle_max;
    }
    return -1;
}

void* deadlock_monitor(void* arg) {
    auto* state = static_cast<SharedState*>(arg);
    while (state->game_status == GAME_RUNNING) {
        usleep(500000);
        if (state->game_status != GAME_RUNNING) break;
        sem_wait(&state->state_lock);
        sync_artifact_ownership_from_inventory(state);
        grant_pending_resources(state);

        const int n = state->num_players + state->num_npcs;
        sem_wait(&state->resource_table.table_lock);
        const int victim = find_waitfor_cycle_victim(state, n);
        sem_post(&state->resource_table.table_lock);

        if (victim >= 0) {
            char msg[160];
            std::snprintf(msg, sizeof(msg), "DEADLOCK RESOLVED (wait-for cycle): victim E%d yields artifacts.", victim);
            log_action(state, msg);
            strip_artifact_weapons_from_entity(state, victim);
        }

        sync_artifact_ownership_from_inventory(state);
        grant_pending_resources(state);
        sem_post(&state->state_lock);
    }
    return nullptr;
}
