#include "input_handler.h"

// UI thread mailbox mein player intent likhta hai.
void submit_ui_player_intent(SharedState* state, int player_id, Action action) {
    if (player_id < 0 || player_id >= MAX_PLAYERS) return;
    sem_wait(&state->ui_mailbox.lock);
    UiPlayerIntent& slot = state->ui_mailbox.player_intent[player_id];
    slot.type = action.type;
    slot.target_id = action.target_id;
    slot.weapon_id = action.weapon_id;
    slot.aux_index = action.aux_index;
    slot.valid = true;
    sem_post(&state->ui_mailbox.lock);
}
