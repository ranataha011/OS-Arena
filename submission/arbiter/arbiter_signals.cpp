#include "arbiter_signals.h"
#include "../include/shared_state.h"
#include <csignal>
#include <signal.h>
#include <unistd.h>

// SIGTERM quit ultimate ke liye SIGSTOP SIGCONT aur alarm 10 sec.
static SharedState* g_state = nullptr;

static int g_ultimate_asp_sig_stopped = 0;

static void on_sigterm(int) {
    if (!g_state) return;
    g_state->game_status = GAME_QUIT;
}

static void on_sigalrm(int) {
    if (g_state) {
        const int base = g_state->num_players;
        const int cnt  = g_state->num_npcs;
        
        
        
        float buf[MAX_ENTITIES];
        for (int i = 0; i < cnt; ++i) buf[i] = g_state->entities[base + i].stamina;
        for (int i = 0; i < cnt; ++i) g_state->entities[base + i].stamina = buf[i];
    }
    if (g_state && g_state->npc_pid > 0) kill(g_state->npc_pid, SIGCONT);   
    g_ultimate_asp_sig_stopped = 0;                                          
    if (g_state) g_state->ultimate_active = false;                           
}

void setup_arbiter_signals(SharedState* state) {
    g_state = state;
    g_ultimate_asp_sig_stopped = 0;
    struct sigaction sa {};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = on_sigterm;
    sigaction(SIGTERM, &sa, nullptr);
    sa.sa_handler = on_sigalrm;
    sigaction(SIGALRM, &sa, nullptr);
}

void arbiter_ultimate_freeze_asp(SharedState* state) {
    g_ultimate_asp_sig_stopped = 1;
    state->ultimate_active = true;                    
    if (state->npc_pid > 0) kill(state->npc_pid, SIGSTOP);
    alarm(10);
}

bool arbiter_ultimate_asp_is_frozen() { return g_ultimate_asp_sig_stopped != 0; }
