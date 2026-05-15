#include "player_thread.h"
#include <pthread.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

// Sirf active player apna move input_buffer mein daal ke action_submitted post.
extern void wait_while_stunned(SharedState* state, int entity_id, const sigset_t* usr1_set);

static pid_t linux_tid_self() { return static_cast<pid_t>(syscall(SYS_gettid)); }

static void sigusr1_handler(int) {}  

void* player_thread(void* arg) {
    auto* ctx = static_cast<PlayerCtx*>(arg);

    
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
        const bool stunned  = (ctx->state->entities[ctx->id].status == STUNNED);
        const bool my_turn  = (!stunned &&
                                ctx->state->active_entity_id == ctx->id &&
                                ctx->state->entities[ctx->id].turn_ready &&
                                ctx->state->entities[ctx->id].status == ALIVE);
        sem_post(&ctx->state->state_lock);

        if (stunned) {
            wait_while_stunned(ctx->state, ctx->id, &waitset);
            continue;
        }

        if (my_turn) {
            sem_wait(&ctx->state->ui_mailbox.lock);
            UiPlayerIntent& in = ctx->state->ui_mailbox.player_intent[ctx->id];
            if (in.valid) {
                
                if (in.type == ACT_DECLINE) {
                    in.valid = false;
                    sem_post(&ctx->state->ui_mailbox.lock);
                    if (ctx->state->arbiter_pid > 0) kill(ctx->state->arbiter_pid, SIGTERM);
                } else {
                    Action a{in.type, ctx->id, in.target_id, in.weapon_id, in.aux_index, true};
                    in.valid = false;
                    sem_post(&ctx->state->ui_mailbox.lock);
                    sem_wait(&ctx->state->input_lock);
                    ctx->state->input_buffer[ctx->id] = a;
                    ctx->state->input_buffer[ctx->id].ready = true;
                    sem_post(&ctx->state->input_lock);
                    sem_post(&ctx->state->action_submitted);
                }
            } else {
                sem_post(&ctx->state->ui_mailbox.lock);
            }
        }

        usleep(my_turn ? 500 : 3000);
    }
    return nullptr;
}
