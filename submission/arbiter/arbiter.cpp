#include "../include/constants.h"
#include "../include/roll_seed.h"
#include "../include/shared_state.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <pthread.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

#include "arbiter_signals.h"
#include "artifact_table.h"

// Arbiter SHM banata hai sem init fork exec hips aur asps phir scheduler chalata hai.
extern void init_inventory(Inventory& inv);
extern void init_artifact_table(SharedState* state);
extern void* deadlock_monitor(void* arg);
extern void scheduling_loop(SharedState* state);
extern void log_action(SharedState* state, const char* text);

static void shutdown_children(SharedState* state) {
    if (state->npc_pid > 0) {
        kill(state->npc_pid, SIGTERM);
        waitpid(state->npc_pid, nullptr, 0);
    }
    if (state->human_pid == state->ui_pid) {
        kill(state->ui_pid, SIGTERM);
        waitpid(state->ui_pid, nullptr, 0);
    } else {
        if (state->human_pid > 0) {
            kill(state->human_pid, SIGTERM);
            waitpid(state->human_pid, nullptr, 0);
        }
        if (state->human_pid2 > 0) {
            kill(state->human_pid2, SIGTERM);
            waitpid(state->human_pid2, nullptr, 0);
        }
        kill(state->ui_pid, SIGTERM);
        waitpid(state->ui_pid, nullptr, 0);
    }
}

static void init_entity(Entity& e, int id, int ui_label, EntityType t, int players) {
    e = {};
    e.id = id;
    e.ui_label = ui_label;
    e.type = t;
    e.status = ALIVE;
    e.max_hp = (t == TYPE_PLAYER) ? (int)(ROLL_CORE + (rand() % 901 + 100)) : (int)(ROLL_LAST_2 + (rand() % 151 + 50));
    e.hp = e.max_hp;
    e.damage = (t == TYPE_PLAYER) ? (int)(ROLL_LAST_1 + 10) : (int)(ROLL_SECOND_LAST + 10);
    
    e.speed = (t == TYPE_PLAYER) ? (100.0f / (float)players) : (10.0f + (float)(rand() % 21));
    
    e.max_stamina = (t == TYPE_PLAYER) ? 100.0f : 150.0f;
    init_inventory(e.inventory);
}

int main() {
    srand(ROLL_CORE);
    int shmid = shmget(SHM_KEY, sizeof(SharedState), IPC_CREAT | 0666);
    if (shmid < 0) return 1;
    auto* state = (SharedState*)shmat(shmid, nullptr, 0);
    if (state == (void*)-1) return 1;
    std::memset(state, 0, sizeof(*state));
    sem_init(&state->state_lock, 1, 1);
    sem_init(&state->action_submitted, 1, 0);
    sem_init(&state->render_ready, 1, 0);
    sem_init(&state->input_lock, 1, 1);
    sem_init(&state->resource_table.table_lock, 1, 1);
    sem_init(&state->log.log_lock, 1, 1);
    sem_init(&state->ui_mailbox.lock, 1, 1);
    state->ui_mailbox.menu_party_size = 2;
    state->ui_mailbox.menu_demo_mode = false;
    state->ui_mailbox.menu_pvp = false;
    state->human_pid2 = 0;
    state->player2_start_idx = 0;
    state->pvp_mode = 0;
    state->pvp_last_team = -1;
    state->ui_mailbox.drop_user_pick = -1;
    state->arbiter_pid = getpid();
    state->game_status = GAME_RUNNING;
    state->requested_players = 2;
    state->last_player_actor_id = -1;
    state->ui_gameover_ack = false;
    state->play_again_requested = false;
    state->drop_pending = false;
    state->drop_owner_id = -1;
    state->drop_source_enemy_label = -1;
    state->drop_weapon = WPN_NONE;
    state->drop_expire_ms = 0;
    state->drop_choice = -1;
    state->ultimate_active = false;
    state->ui_active_tab = 0;
    state->ui_hover_idx = -1;
    state->ui_selected_action = ACT_NONE;
    state->ui_selected_target = -1;
    for (int i = 0; i < MAX_PLAYERS; ++i) { state->ui_lts_pick[i] = 0; state->ui_primary_evict_slot[i] = -1; }
    for (int i = 0; i < MAX_ENTITIES; ++i) {
        state->waits_for_resource[i] = -1;
        state->pending_artifact_grant[i] = WPN_NONE;
    }
    state->eclipse_env_offer = false;
    setup_arbiter_signals(state);
    init_artifact_table(state);

    char key[32];
    std::snprintf(key, sizeof(key), "%d", SHM_KEY);
    pid_t ui_pid = fork();
    if (ui_pid == 0) {
        execl("./hips", "hips", key, nullptr);
        _exit(1);
    }
    state->ui_pid = ui_pid;
    bool demo_mode = false;
    bool pvp_mode  = false;
    while (!state->ui_setup_done) {
        bool submitted = false;
        int party = 2;
        sem_wait(&state->ui_mailbox.lock);
        if (state->ui_mailbox.menu_begin_submitted) {
            submitted = true;
            party     = state->ui_mailbox.menu_party_size;
            demo_mode = state->ui_mailbox.menu_demo_mode;
            pvp_mode  = state->ui_mailbox.menu_pvp;
            state->ui_mailbox.menu_begin_submitted = false;
        }
        sem_post(&state->ui_mailbox.lock);
        
        if (state->game_status == GAME_QUIT) break;
        if (submitted) {
            sem_wait(&state->state_lock);
            if (party < 1) party = 1;
            if (pvp_mode) {
                
                if (party * 2 > MAX_PLAYERS) party = MAX_PLAYERS / 2;
            } else {
                if (party > MAX_PLAYERS) party = MAX_PLAYERS;
            }
            state->requested_players = party;
            state->ui_setup_done = true;
            sem_post(&state->state_lock);
        }
        usleep(10000);
    }

    
    if (state->game_status == GAME_QUIT) {
        kill(state->ui_pid, SIGTERM);
        waitpid(state->ui_pid, nullptr, 0);
        shmdt(state);
        shmctl(shmid, IPC_RMID, nullptr);
        return 0;
    }

    
    state->num_players = pvp_mode ? state->requested_players * 2 : state->requested_players;
    if (pvp_mode) {
        state->num_npcs          = 0;
        state->pvp_mode          = 1;
        state->player2_start_idx = state->requested_players; 
    } else {
        state->num_npcs          = rand() % 8 + 2;
        state->pvp_mode          = 0;
        state->player2_start_idx = 0;
    }
    for (int i = 0; i < state->num_players; ++i) init_entity(state->entities[i], i, i, TYPE_PLAYER, state->num_players);
    for (int i = 0; i < state->num_npcs; ++i)
        init_entity(state->entities[state->num_players + i], state->num_players + i, i + 1, TYPE_NPC, state->num_players);
    state->total_enemy_spawned = state->num_npcs;
    if (demo_mode) {
        try_place_weapon(state, 0, WPN_SOLAR_CORE);
        try_place_weapon(state, 0, WPN_LUNAR_BLADE);
        log_action(state, "DEMO MODE: Player E0 starts with Solar Core + Lunar Blade (Ultimate ability unlocked).");
    }
    log_action(state, "Chrono Rift started.");

    state->human_pid  = state->ui_pid;
    state->human_pid2 = 0;
    sem_wait(&state->state_lock);
    for (int i = 0; i < state->num_players; ++i) state->entities[i].pid = state->ui_pid;
    sem_post(&state->state_lock);

    if (pvp_mode) {
        state->npc_pid = 0;
        log_action(state, "PvP MODE: HIP+UI process owns all player pthreads. No asps launched.");
    } else {
        pid_t npc_pid = fork();
        if (npc_pid == 0) {
            execl("./asps", "asps", key, nullptr);
            _exit(1);
        }
        state->npc_pid = npc_pid;
        sem_wait(&state->state_lock);
        for (int i = 0; i < state->num_npcs; ++i) state->entities[state->num_players + i].pid = npc_pid;
        sem_post(&state->state_lock);
    }

    pthread_t dm {};
    pthread_create(&dm, nullptr, deadlock_monitor, state);
    scheduling_loop(state);
    pthread_join(dm, nullptr);

    long long wait_ms = 0;
    while (!state->ui_gameover_ack && wait_ms < 30000) {
        bool clicked = false;
        bool play_again = false;
        sem_wait(&state->ui_mailbox.lock);
        if (state->ui_mailbox.gameover_clicked) {
            clicked = true;
            play_again = state->ui_mailbox.gameover_play_again;
            state->ui_mailbox.gameover_clicked = false;
        }
        sem_post(&state->ui_mailbox.lock);
        if (clicked) {
            sem_wait(&state->state_lock);
            state->ui_gameover_ack = true;
            state->play_again_requested = play_again;
            sem_post(&state->state_lock);
        }
        usleep(100000);
        wait_ms += 100;
    }

    if (state->play_again_requested) {
        shutdown_children(state);
        shmdt(state);
        shmctl(shmid, IPC_RMID, nullptr);
        execl("./arbiters", "arbiters", nullptr);
        return 0;
    }

    shutdown_children(state);
    shmdt(state);
    shmctl(shmid, IPC_RMID, nullptr);
    return 0;
}
