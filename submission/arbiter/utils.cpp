#include "../include/shared_state.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <signal.h>
#include <time.h>

// Clock stun wait sigtimedwait log ring buffer.
long long now_ms() {
    timespec ts {};
    clock_gettime(CLOCK_REALTIME, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

long long now_mono_ms() {
    timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}


void wait_while_stunned(SharedState* state, int id, const sigset_t* usr1_set) {
    while (state->game_status == GAME_RUNNING) {
        long long until = 0;
        sem_wait(&state->state_lock);
        const bool stunned = (id >= 0 && id < MAX_ENTITIES && state->entities[id].status == STUNNED);
        if (stunned) until = state->entities[id].stun_until_ms;
        sem_post(&state->state_lock);
        if (!stunned) return;

        for (;;) {
            long long rem = until - now_ms();
            if (rem <= 0) break;
            struct timespec ts {};
            ts.tv_sec = rem / 1000;
            ts.tv_nsec = (rem % 1000) * 1000000L;
            if (ts.tv_nsec >= 1000000000L) {
                ++ts.tv_sec;
                ts.tv_nsec -= 1000000000L;
            }
            int w = sigtimedwait(usr1_set, nullptr, &ts);
            if (w < 0 && errno == EINTR) continue;
            (void)w;

            sem_wait(&state->state_lock);
            const bool still = (id >= 0 && id < MAX_ENTITIES && state->entities[id].status == STUNNED);
            sem_post(&state->state_lock);
            if (!still) return;
            if (now_ms() >= until) break;
        }
    }
}

void log_action(SharedState* state, const char* text) {
    sem_wait(&state->log.log_lock);
    std::snprintf(state->log.lines[state->log.head], LOG_LINE_LEN, "%s", text);
    state->log.head = (state->log.head + 1) % LOG_LINES;
    sem_post(&state->log.log_lock);
}

const char* entity_name(const SharedState* state, int id) {
    static thread_local char name_bufs[8][32];
    static thread_local int idx = 0;
    idx = (idx + 1) % 8;
    char* name_buf = name_bufs[idx];
    if (id < 0 || id >= state->num_players + state->num_npcs) return "Unknown";
    std::snprintf(name_buf, sizeof(name_bufs[0]), "%s_%d", state->entities[id].type == TYPE_PLAYER ? "Player" : "NPC", id);
    return name_buf;
}
