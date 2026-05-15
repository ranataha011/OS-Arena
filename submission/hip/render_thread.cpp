#include "render_thread.h"

#include <unistd.h>

// Draw loop sem_trywait fail ho to purana snapshot redraw.
struct RenderCtx {
    SharedState* state;
    sf::RenderWindow* window;
    sf::Font* font;
    std::vector<Button>* buttons;
    int* hover_idx;
    ActionType* selected_action;
    int* selected_target;
};

void* render_thread_main(void* arg) {
    auto* ctx = static_cast<RenderCtx*>(arg);
    ctx->window->setActive(true);
    Snapshot snap{};
    while (ctx->window->isOpen()) {
        
        if (sem_trywait(&ctx->state->state_lock) == 0) {
            snap.state = *ctx->state;
            sem_post(&ctx->state->state_lock);
        }
        const GameStatus gs = snap.state.game_status;
        if (gs == GAME_QUIT) break;

        const int tab = snap.state.ui_active_tab;
        const int hover = snap.state.ui_hover_idx;
        const ActionType sel = snap.state.ui_selected_action;
        const int st = snap.state.ui_selected_target;
        (void)ctx->hover_idx;
        (void)ctx->selected_action;
        (void)ctx->selected_target;
        draw_hud(*ctx->window, *ctx->font, snap, *ctx->buttons, hover, sel, st, tab);
        ctx->window->display();
        usleep(16000);
    }
    return nullptr;
}
