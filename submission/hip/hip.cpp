#include "../include/constants.h"
#include "../include/roll_seed.h"
#include "../include/shared_state.h"
#include "input_handler.h"
#include "hud.h"
#include "player_thread.h"
#include "render_thread.h"
#include <X11/Xlib.h>
#include <SFML/Graphics.hpp>
#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <pthread.h>
#include <signal.h>
#include <sys/shm.h>
#include <unistd.h>
#include <array>
#include <string>
#include <vector>


// SFML window render thread har player ka pthread mailbox se intent.
static bool load_game_font(sf::Font& font) {
    static const char* kPaths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "assets/fonts/arial.ttf",
    };
    for (const char* path : kPaths) {
        if (font.loadFromFile(path)) return true;
    }
    return false;
}

struct RenderCtx {
    SharedState* state;
    sf::RenderWindow* window;
    sf::Font* font;
    std::vector<Button>* buttons;
    int* hover_idx;
    ActionType* selected_action;
    int* selected_target;
};

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    
    
    
    
    XInitThreads();
    {
        sigset_t blocked;
        sigemptyset(&blocked);
        sigaddset(&blocked, SIGUSR1);
        pthread_sigmask(SIG_BLOCK, &blocked, nullptr);
    }
    srand(ROLL_CORE);
    int key = std::atoi(argv[1]);
    int shmid = shmget(key, sizeof(SharedState), 0666);
    if (shmid < 0) return 1;
    auto* state = (SharedState*)shmat(shmid, nullptr, 0);
    if (state == (void*)-1) return 1;

    sf::RenderWindow window(sf::VideoMode(WINDOW_W, WINDOW_H), "Chrono Rift");
    window.setFramerateLimit(60);
    sf::Font font;
    if (!load_game_font(font)) {
        std::fprintf(stderr,
                   "Chrono Rift UI: could not load any font.\n"
                   "  Install one of: fonts-dejavu-core (Debian/Ubuntu) or liberation-fonts\n"
                   "  OR add optional file: assets/fonts/arial.ttf\n");
        shmdt(state);
        return 1;
    }

    enum Mode { SPLASH, MENU, GAME };
    Mode mode = SPLASH;
    int chosen_players = 2;
    std::vector<sf::FloatRect> menu_buttons = {{380, 230, 520, 60}, {380, 300, 520, 60}, {380, 370, 520, 60}, {380, 440, 520, 60}};
    sf::FloatRect demo_btn(380, 512, 520, 50);   
    sf::FloatRect pvp_btn(380, 566, 520, 46);    
    sf::FloatRect start_btn(380, 622, 520, 46);  
    bool demo_selected = false;
    bool pvp_selected  = false;

    std::vector<Button> buttons = {
        {{210, 588, 92, 40}, ACT_STRIKE, "STRIKE"},
        {{310, 588, 92, 40}, ACT_EXHAUST, "EXHAUST"},
        {{410, 588, 92, 40}, ACT_USE_WEAPON, "WPN"},
        {{510, 588, 92, 40}, ACT_SWAP_IN, "SWAPIN"},
        {{610, 588, 92, 40}, ACT_HEAL, "HEAL"},
        {{710, 588, 92, 40}, ACT_SKIP, "SKIP"},
        {{810, 588, 120, 40}, ACT_ULTIMATE, "ULTIMATE"},
    };
    int hover_idx = -1;
    ActionType selected_action = ACT_NONE;
    int selected_target = -1;
    int active_tab = 0; 
    std::vector<sf::FloatRect> tab_rects = {{18, 64, 120, 26}, {142, 64, 120, 26}, {266, 64, 120, 26}, {390, 64, 120, 26}};
    
    std::array<int, MAX_PLAYERS> lts_pick{};
    std::array<int, MAX_PLAYERS> primary_evict_slot{};
    primary_evict_slot.fill(-1);  

    RenderCtx rctx {state, &window, &font, &buttons, &hover_idx, &selected_action, &selected_target};
    pthread_t render_tid{};
    bool render_started = false;
    bool players_spawned = false;
    std::vector<pthread_t> player_tids;
    std::vector<PlayerCtx> player_ctxs;

    auto pick_target_from_click = [&](float mx, float my) -> int {
        sem_wait(&state->state_lock);
        int result = -1;
        if (state->pvp_mode && state->active_entity_id >= 0) {
            
            const int split = state->player2_start_idx;
            const bool actor_is_A = (state->active_entity_id < split);
            const int opp_start = actor_is_A ? split : 0;
            const int opp_end   = actor_is_A ? state->num_players : split;
            int opp_alive = 0;
            for (int i = opp_start; i < opp_end; ++i)
                if (state->entities[i].status != DEAD) ++opp_alive;
            const float card_step = (opp_alive > 0) ? std::min(90.f, 430.f / (float)opp_alive) : 90.f;
            const float card_h    = card_step - 4.f;
            int vis = 0;
            for (int i = opp_start; i < opp_end; ++i) {
                if (state->entities[i].status == DEAD) continue;
                float y = 132.f + vis * card_step;
                ++vis;
                if (mx >= 445.f && mx <= 835.f && my >= y && my <= y + card_h) { result = i; break; }
            }
        } else {
            
            int npcs_active = 0;
            for (int i = 0; i < state->num_npcs; ++i)
                if (state->entities[state->num_players + i].status != DEAD) ++npcs_active;
            const float card_step = (npcs_active > 0) ? std::min(90.f, 430.f / (float)npcs_active) : 90.f;
            const float card_h    = card_step - 4.f;
            int vis = 0;
            for (int i = 0; i < state->num_npcs; ++i) {
                int id = state->num_players + i;
                if (state->entities[id].status == DEAD) continue;
                float y = 132.f + vis * card_step;
                ++vis;
                if (mx >= 445.f && mx <= 835.f && my >= y && my <= y + card_h) { result = id; break; }
            }
        }
        sem_post(&state->state_lock);
        return result;
    };
    auto first_alive_npc_target = [&]() -> int {
        if (state->pvp_mode) {
            const int active = state->active_entity_id;
            const int split  = state->player2_start_idx;
            if (active >= 0 && active < split) {
                for (int i = split; i < state->num_players; ++i)
                    if (state->entities[i].status != DEAD) return i;
            } else if (active >= split) {
                for (int i = 0; i < split; ++i)
                    if (state->entities[i].status != DEAD) return i;
            }
            return -1;
        }
        for (int i = 0; i < state->num_npcs; ++i) {
            int id = state->num_players + i;
            if (state->entities[id].status != DEAD) return id;
        }
        return -1;
    };
    auto is_opponent = [&](int entity_id) -> bool {
        if (!state->pvp_mode)
            return entity_id >= state->num_players;
        const int active = state->active_entity_id;
        const int split  = state->player2_start_idx;
        if (active < 0) return false;
        return (active < split) != (entity_id < split);
    };
    auto first_inventory_weapon = [&](int player_id) -> WeaponID {
        if (player_id < 0 || player_id >= state->num_players) return WPN_NONE;
        for (int i = 0; i < INVENTORY_SIZE; ++i) {
            WeaponID w = state->entities[player_id].inventory.slots[i];
            if (w != WPN_NONE) return w;
        }
        return WPN_NONE;
    };

    sf::FloatRect play_again_btn(470, 430, 160, 48);
    sf::FloatRect exit_btn(650, 430, 160, 48);
    sf::FloatRect pickup_btn(470, 430, 160, 44);
    sf::FloatRect decline_btn(650, 430, 160, 44);

    auto sync_ui_to_shm = [&]() {
        sem_wait(&state->state_lock);
        for (int r = 0; r < state->num_players; ++r) {
            int c = state->entities[r].inventory.lts_count;
            if (c <= 0) lts_pick[r] = 0;
            else if (lts_pick[r] >= c) lts_pick[r] = 0;  
            else if (lts_pick[r] < 0) lts_pick[r] = 0;
        }
        state->ui_active_tab = active_tab;
        state->ui_hover_idx = hover_idx;
        state->ui_selected_action = selected_action;
        state->ui_selected_target = selected_target;
        for (int r = 0; r < MAX_PLAYERS; ++r) state->ui_lts_pick[r] = lts_pick[r];
        for (int r = 0; r < MAX_PLAYERS; ++r) state->ui_primary_evict_slot[r] = primary_evict_slot[r];
        sem_post(&state->state_lock);
    };

    while (window.isOpen()) {
        if (mode == GAME && render_started && !players_spawned) {
            sem_wait(&state->state_lock);
            const pid_t me = getpid();
            const bool ready =
                state->num_players > 0 && state->entities[0].pid == me && state->entities[0].max_hp > 0;
            sem_post(&state->state_lock);
            if (ready) {
                players_spawned = true;
                const int n = state->num_players;
                player_tids.resize((size_t)n);
                player_ctxs.resize((size_t)n);
                for (int i = 0; i < n; ++i) {
                    player_ctxs[(size_t)i] = {state, i};
                    pthread_create(&player_tids[(size_t)i], nullptr, player_thread, &player_ctxs[(size_t)i]);
                }
            }
        }
        sf::Event e {};
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed) {
                
                
                
                sem_wait(&state->state_lock);
                state->game_status     = GAME_QUIT;
                state->ui_gameover_ack = true;   
                if (!state->ui_setup_done)
                    state->ui_setup_done = true; 
                sem_post(&state->state_lock);
                sem_post(&state->action_submitted); 
                window.close();
            }
            if (mode == SPLASH && e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
                mode = MENU;
            } else if (mode == MENU && e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
                sf::Vector2f p((float)e.mouseButton.x, (float)e.mouseButton.y);
                for (int i = 0; i < 4; ++i) {
                    if (menu_buttons[i].contains(p)) {
                        chosen_players = i + 1;
                        demo_selected  = false;
                        if (chosen_players == 1) pvp_selected = false;
                    }
                }
                if (demo_btn.contains(p)) { demo_selected = true; pvp_selected = false; chosen_players = 1; }
                if (pvp_btn.contains(p) && chosen_players > 1 && !demo_selected) pvp_selected = !pvp_selected;
                if (start_btn.contains(p)) {
                    sem_wait(&state->ui_mailbox.lock);
                    state->ui_mailbox.menu_party_size   = demo_selected ? 1 : chosen_players;
                    state->ui_mailbox.menu_demo_mode    = demo_selected;
                    state->ui_mailbox.menu_pvp          = pvp_selected && chosen_players > 1 && !demo_selected;
                    state->ui_mailbox.menu_begin_submitted = true;
                    sem_post(&state->ui_mailbox.lock);
                    mode = GAME;
                    window.setActive(false);
                    pthread_create(&render_tid, nullptr, render_thread_main, &rctx);
                    render_started = true;
                }
            } else if (mode == GAME) {
                
                int snap_active, snap_num_p, snap_num_n, snap_drop_owner;
                bool snap_drop; GameStatus snap_gs;
                sem_wait(&state->state_lock);
                snap_active     = state->active_entity_id;
                snap_num_p      = state->num_players;
                snap_num_n      = state->num_npcs;
                snap_drop       = state->drop_pending;
                snap_drop_owner = state->drop_owner_id;
                snap_gs         = state->game_status;
                sem_post(&state->state_lock);

                
                if (snap_drop && snap_drop_owner >= 0 && snap_drop_owner < snap_num_p &&
                    e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
                    sf::Vector2f p((float)e.mouseButton.x, (float)e.mouseButton.y);
                    sem_wait(&state->ui_mailbox.lock);
                    if (pickup_btn.contains(p)) state->ui_mailbox.drop_user_pick = 1;
                    else if (decline_btn.contains(p)) state->ui_mailbox.drop_user_pick = 0;
                    sem_post(&state->ui_mailbox.lock);
                    continue;
                }
                if (snap_gs != GAME_RUNNING && e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
                    sf::Vector2f p((float)e.mouseButton.x, (float)e.mouseButton.y);
                    sem_wait(&state->ui_mailbox.lock);
                    state->ui_mailbox.gameover_clicked = true;
                    state->ui_mailbox.gameover_play_again = play_again_btn.contains(p);
                    if (exit_btn.contains(p)) state->ui_mailbox.gameover_play_again = false;
                    sem_post(&state->ui_mailbox.lock);
                    continue;
                }
                if (e.type == sf::Event::MouseMoved) {
                    hover_idx = -1;
                    if (active_tab == 0) {
                        sf::Vector2f p((float)e.mouseMove.x, (float)e.mouseMove.y);
                        for (int i = 0; i < (int)buttons.size(); ++i) if (buttons[i].rect.contains(p)) hover_idx = i;
                    }
                } else if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
                    sf::Vector2f p((float)e.mouseButton.x, (float)e.mouseButton.y);
                    for (int i = 0; i < 4; ++i) {
                        if (tab_rects[i].contains(p)) {
                            active_tab = i;
                            break;
                        }
                    }
                    if (active_tab == 1) {
                        constexpr float row0 = 128.f;
                        constexpr float row_step = 200.f;
                        constexpr float lts_x0 = 26.f;
                        constexpr float lts_y_off = 78.f;
                        constexpr float chip_w = 130.f;
                        constexpr float chip_h = 22.f;
                        constexpr float chip_gap = 136.f;
                        for (int r = 0; r < state->num_players; ++r) {
                            float y = row0 + r * row_step;
                            
                            bool hit_primary = false;
                            for (int k = 0; k < INVENTORY_SIZE; ++k) {
                                float sx = 26.f + k * 24.f, sy = y + 24.f;
                                if (p.x >= sx && p.x <= sx + 22.f && p.y >= sy && p.y <= sy + 22.f) {
                                    primary_evict_slot[r] = (primary_evict_slot[r] == k) ? -1 : k;
                                    hit_primary = true; break;
                                }
                            }
                            if (hit_primary) continue;
                            
                            for (int k = 0; k < state->entities[r].inventory.lts_count; ++k) {
                                float lx = lts_x0 + k * chip_gap;
                                float ly = y + lts_y_off;
                                if (p.x >= lx && p.x <= lx + chip_w && p.y >= ly && p.y <= ly + chip_h) {
                                    lts_pick[r] = k;
                                    break;
                                }
                            }
                        }
                        continue;
                    }
                    if (active_tab != 0) continue;
                    if (snap_active >= 0 && snap_active < snap_num_p) {
                        bool clicked_action = false;
                        for (const auto& b : buttons) {
                            if (!b.rect.contains(p)) continue;
                            selected_action = b.action;
                            clicked_action = true;
                            if (selected_action == ACT_HEAL || selected_action == ACT_SKIP || selected_action == ACT_ULTIMATE) {
                                Action a {selected_action, snap_active, -1, WPN_NONE, 0, true};
                                submit_ui_player_intent(state, snap_active, a);
                                selected_action = ACT_NONE;
                            } else if (selected_action == ACT_SWAP_IN) {
                                int pid = snap_active;
                                int cnt = state->entities[pid].inventory.lts_count;
                                if (cnt > 0) {
                                    int idx = lts_pick[pid];
                                    if (idx < 0 || idx >= cnt) idx = 0;  
                                    Action a {ACT_SWAP_IN, pid, primary_evict_slot[pid], WPN_NONE, idx, true};
                                    submit_ui_player_intent(state, pid, a);
                                    lts_pick[pid] = 0;           
                                    primary_evict_slot[pid] = -1; 
                                }
                                selected_action = ACT_NONE;
                            } else if (selected_action == ACT_STRIKE || selected_action == ACT_EXHAUST || selected_action == ACT_USE_WEAPON) {
                                int target = selected_target;
                                if (target < 0 || target >= snap_num_p + snap_num_n ||
                                    !is_opponent(target) || state->entities[target].status == DEAD) {
                                    target = first_alive_npc_target();
                                }
                                if (target >= 0) {
                                    WeaponID wpn =
                                        (selected_action == ACT_USE_WEAPON) ? first_inventory_weapon(snap_active) : WPN_NONE;
                                    Action a {selected_action, snap_active, target, wpn, 0, true};
                                    submit_ui_player_intent(state, snap_active, a);
                                    selected_action = ACT_NONE;
                                    selected_target = target;
                                }
                            }
                            break;
                        }
                        if (!clicked_action) {
                            
                            
                            int t = pick_target_from_click(p.x, p.y);
                            if (t >= 0) selected_target = t;
                        }
                    }
                } else if (e.type == sf::Event::KeyPressed && snap_active >= 0 && snap_active < snap_num_p) {
                    ActionType a = ACT_NONE;
                    if (e.key.code == sf::Keyboard::Num1) a = ACT_STRIKE;
                    if (e.key.code == sf::Keyboard::Num2) a = ACT_EXHAUST;
                    if (e.key.code == sf::Keyboard::Num3) a = ACT_USE_WEAPON;
                    if (e.key.code == sf::Keyboard::Num4) a = ACT_SWAP_IN;
                    if (e.key.code == sf::Keyboard::Num5) a = ACT_HEAL;
                    if (e.key.code == sf::Keyboard::Num6) a = ACT_SKIP;
                    if (e.key.code == sf::Keyboard::P) a = ACT_PICKUP;
                    
                    if (e.key.code == sf::Keyboard::Q) a = ACT_DECLINE;
                    if (e.key.code == sf::Keyboard::U) a = ACT_ULTIMATE;
                    
                    
                    if (e.key.code == sf::Keyboard::Left || e.key.code == sf::Keyboard::Right) {
                        sem_wait(&state->state_lock);
                        const int cnt = state->entities[snap_active].inventory.lts_count;
                        sem_post(&state->state_lock);
                        if (cnt > 0) {
                            if (lts_pick[snap_active] < 0 || lts_pick[snap_active] >= cnt)
                                lts_pick[snap_active] = 0;
                            lts_pick[snap_active] = (e.key.code == sf::Keyboard::Left)
                                ? (lts_pick[snap_active] - 1 + cnt) % cnt
                                : (lts_pick[snap_active] + 1) % cnt;
                        }
                    }
                    if (a != ACT_NONE) {
                        int pid = snap_active;
                        
                        int target = selected_target;
                        if (target < 0 || target >= snap_num_p + snap_num_n ||
                            !is_opponent(target) || state->entities[target].status == DEAD)
                            target = -1;
                        WeaponID wpn = (a == ACT_USE_WEAPON) ? first_inventory_weapon(pid) : WPN_NONE;
                        int aux = 0;
                        bool ok = true;
                        if (a == ACT_SWAP_IN) {
                            target = primary_evict_slot[pid];  
                            int cnt = state->entities[pid].inventory.lts_count;
                            if (cnt <= 0) ok = false;
                            else {
                                if (lts_pick[pid] < 0 || lts_pick[pid] >= cnt) lts_pick[pid] = 0;
                                aux = lts_pick[pid];
                            }
                        }
                        if (ok) {
                            Action act {a, pid, target, wpn, aux, true};
                            submit_ui_player_intent(state, pid, act);
                            if (a == ACT_SWAP_IN) { lts_pick[pid] = 0; primary_evict_slot[pid] = -1; }
                        }
                    }
                }
            }
        }
        if (mode == GAME) sync_ui_to_shm();

        if (mode == SPLASH) {
            window.clear(sf::Color(6, 6, 16));
            sf::Text t("CHRONO RIFT", font, 56);
            t.setPosition(410, 280);
            t.setFillColor(sf::Color(250, 204, 21));
            window.draw(t);
            sf::Text t2("CS2006 OPERATING SYSTEMS - SPRING 2026", font, 14);
            t2.setPosition(380, 350);
            t2.setFillColor(sf::Color(90, 90, 110));
            window.draw(t2);
            sf::Text t3("CLICK ANYWHERE TO CONTINUE", font, 18);
            t3.setPosition(430, 390);
            t3.setFillColor(sf::Color(130, 130, 150));
            window.draw(t3);
            window.display();
            continue;
        }
        if (mode == MENU) {
            window.clear(sf::Color(6, 6, 16));
            sf::Text h("PARTY CONFIGURATION", font, 34);
            h.setPosition(395, 160);
            h.setFillColor(sf::Color(96, 165, 250));
            window.draw(h);
            sf::Text q("Select party size (1-4 human-controlled characters)", font, 16);
            q.setPosition(390, 205);
            q.setFillColor(sf::Color(95, 95, 110));
            window.draw(q);
            for (int i = 0; i < 4; ++i) {
                bool sel = !demo_selected && chosen_players == i + 1;
                sf::RectangleShape b({menu_buttons[i].width, menu_buttons[i].height});
                b.setPosition(menu_buttons[i].left, menu_buttons[i].top);
                b.setFillColor(sel ? sf::Color(28, 40, 64) : sf::Color(15, 15, 31));
                b.setOutlineThickness(2.f);
                b.setOutlineColor(sel ? sf::Color(96, 165, 250) : sf::Color(45, 45, 55));
                window.draw(b);
                sf::Text tx(std::to_string(i + 1) + " Player" + (i == 0 ? "" : "s"), font, 24);
                tx.setPosition(menu_buttons[i].left + 190, menu_buttons[i].top + 14);
                tx.setFillColor(sel ? sf::Color(96, 165, 250) : sf::Color(120, 120, 130));
                window.draw(tx);
            }
            
            if (pvp_selected && chosen_players > 1) {
                char hint[80];
                std::snprintf(hint, sizeof(hint), "PvP: %d player(s) per team  \u2192  %d total entities",
                              chosen_players, chosen_players * 2);
                sf::Text ht(hint, font, 13);
                ht.setPosition(388.f, 502.f);
                ht.setFillColor(sf::Color(239, 68, 68));
                window.draw(ht);
            }
            
            {
                sf::RectangleShape db({demo_btn.width, demo_btn.height});
                db.setPosition(demo_btn.left, demo_btn.top);
                db.setFillColor(demo_selected ? sf::Color(56, 40, 10) : sf::Color(15, 15, 31));
                db.setOutlineThickness(2.f);
                db.setOutlineColor(demo_selected ? sf::Color(250, 204, 21) : sf::Color(90, 80, 40));
                window.draw(db);
                sf::Text dl("DEMO MODE  (1 player, Ultimate pre-equipped)", font, 18);
                dl.setPosition(demo_btn.left + 60, demo_btn.top + 13);
                dl.setFillColor(demo_selected ? sf::Color(250, 204, 21) : sf::Color(140, 120, 60));
                window.draw(dl);
            }
            
            {
                const bool pvp_available = (chosen_players > 1) && !demo_selected;
                sf::RectangleShape pb({pvp_btn.width, pvp_btn.height});
                pb.setPosition(pvp_btn.left, pvp_btn.top);
                pb.setFillColor(pvp_selected ? sf::Color(50, 10, 10) : sf::Color(15, 15, 31));
                pb.setOutlineThickness(2.f);
                pb.setOutlineColor(pvp_selected ? sf::Color(239, 68, 68)
                                  : pvp_available ? sf::Color(90, 40, 40)
                                  :                 sf::Color(35, 25, 25));
                window.draw(pb);
                sf::Text pl("PvP MODE  (1 HIP+UI process, pthreads / team)", font, 18);
                pl.setPosition(pvp_btn.left + 60, pvp_btn.top + 11);
                pl.setFillColor(pvp_selected ? sf::Color(239, 68, 68)
                               : pvp_available ? sf::Color(160, 80, 80)
                               :                 sf::Color(60, 40, 40));
                window.draw(pl);
            }
            sf::RectangleShape sb({start_btn.width, start_btn.height});
            sb.setPosition(start_btn.left, start_btn.top);
            sb.setFillColor(sf::Color(20, 56, 37));
            sb.setOutlineThickness(2.f);
            sb.setOutlineColor(sf::Color(74, 222, 128));
            window.draw(sb);
            sf::Text st("BEGIN GAME", font, 22);
            st.setPosition(start_btn.left + 175, start_btn.top + 13);
            st.setFillColor(sf::Color(74, 222, 128));
            window.draw(st);
            window.display();
            continue;
        }
        if (mode == GAME) {
            
            sf::sleep(sf::milliseconds(10));
        } else {
            sf::sleep(sf::milliseconds(10));
        }
    }

    if (render_started) pthread_join(render_tid, nullptr);
    for (size_t i = 0; i < player_tids.size(); ++i) {
        if (player_tids[i]) pthread_join(player_tids[i], nullptr);
    }
    shmdt(state);
    return 0;
}
