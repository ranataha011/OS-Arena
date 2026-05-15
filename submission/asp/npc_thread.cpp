#include "npc_thread.h"
#include "npc_ai.h"
#include <pthread.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

// Apni turn pe pending_action set karke action_submitted post.
extern void wait_while_stunned(SharedState* state, int entity_id, const sigset_t* usr1_set);

static pid_t linux_tid_self() { return static_cast<pid_t>(syscall(SYS_gettid)); }

static void sigusr1_handler(int) {}  

void* npc_thread(void* arg) {
    auto* ctx = static_cast<NpcCtx*>(arg);

    
    struct sigaction sa{};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;  
    sa.sa_handler = sigusr1_handler;
    sigaction(SIGUSR1, &sa, nullptr);

    
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &blocked, nullptr);

    sigset_t waitset;
    sigemptyset(&waitset);
    sigaddset(&waitset, SIGUSR1);

    sem_wait(&ctx->state->state_lock);
    ctx->state->entities[ctx->id].linux_tid = linux_tid_self();
    sem_post(&ctx->state->state_lock);

    while (ctx->state->game_status == GAME_RUNNING) {
        sem_wait(&ctx->state->state_lock);
        
        if (ctx->state->entities[ctx->id].linux_tid == 0)
            ctx->state->entities[ctx->id].linux_tid = linux_tid_self();
        const bool stunned  = (ctx->state->entities[ctx->id].status == STUNNED);
        const bool my_turn  = (!stunned &&
                                ctx->state->active_entity_id == ctx->id &&
                                ctx->state->entities[ctx->id].turn_ready &&
                                !ctx->state->entities[ctx->id].pending_action.ready);
        if (my_turn) {
            ctx->state->entities[ctx->id].pending_action = npc_decide(ctx->state, ctx->id);
            sem_post(&ctx->state->action_submitted);
        }
        sem_post(&ctx->state->state_lock);

        if (stunned) {
            wait_while_stunned(ctx->state, ctx->id, &waitset);
            continue;
        }
        
        if (!my_turn) usleep(5000);
    }
    return nullptr;
}
