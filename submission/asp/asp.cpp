#include "../include/roll_seed.h"
#include "../include/shared_state.h"
#include "npc_thread.h"
#include <cstdlib>
#include <ctime>
#include <pthread.h>
#include <signal.h>
#include <sys/shm.h>
#include <unistd.h>
#include <vector>

// NPC process har enemy ka pthread spawn.
int main(int argc, char** argv) {
    if (argc < 2) return 1;
    srand((unsigned)time(nullptr) ^ ((unsigned)getpid() << 16));
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &blocked, nullptr);

    int key = std::atoi(argv[1]);
    int shmid = shmget(key, sizeof(SharedState), 0666);
    if (shmid < 0) return 1;
    auto* state = (SharedState*)shmat(shmid, nullptr, 0);
    if (state == (void*)-1) return 1;

    std::vector<pthread_t> tids(state->num_npcs);
    std::vector<NpcCtx> ctx(state->num_npcs);
    for (int i = 0; i < state->num_npcs; ++i) {
        ctx[i] = {state, state->num_players + i};
        pthread_create(&tids[i], nullptr, npc_thread, &ctx[i]);
    }
    for (auto& t : tids) pthread_join(t, nullptr);
    shmdt(state);
    return 0;
}
