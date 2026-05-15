#include "hud.h"

#include "../include/constants.h"
#include <algorithm>
#include <cstdio>

// Snapshot se bars log tabs draw koi combat write nahi.
static void draw_bar(sf::RenderWindow& w, float x, float y, float width, float h, float value, float maxv, const sf::Color& fg) {
    sf::RectangleShape bg(sf::Vector2f(width, h));
    bg.setPosition(x, y);
    bg.setFillColor(sf::Color(13, 13, 31));
    bg.setOutlineThickness(1.f);
    bg.setOutlineColor(sf::Color(26, 26, 46));
    w.draw(bg);
    float pct = (maxv > 0.f) ? std::max(0.f, std::min(1.f, value / maxv)) : 0.f;
    sf::RectangleShape fill(sf::Vector2f(width * pct, h));
    fill.setPosition(x, y);
    fill.setFillColor(fg);
    w.draw(fill);
}

void draw_hud(sf::RenderWindow& w, sf::Font& font, const Snapshot& snap, const std::vector<Button>& buttons, int hover_idx,
              ActionType selected_action, int selected_target, int active_tab) {
    (void)selected_action;
    w.clear(sf::Color(6, 6, 16));

    const SharedState& s = snap.state;
    
    int npcs_active = 0;
    for (int _n = 0; _n < s.num_npcs; ++_n)
        if (s.entities[s.num_players + _n].status != DEAD) ++npcs_active;

    sf::Text title("CHRONO RIFT", font, 24);
    title.setPosition(18.f, 10.f);
    title.setFillColor(sf::Color(250, 204, 21));
    w.draw(title);

    char hdr[128];
    if (s.pvp_mode) {
        std::snprintf(hdr, sizeof(hdr), "TEAM A vs TEAM B  [PvP LOCAL]   ACTIVE: E%d",
                      s.active_entity_id);
    } else {
        std::snprintf(hdr, sizeof(hdr), "KILLED: %d/10   ACTIVE: E%d   ENEMIES: %d/%d alive",
                      s.enemies_killed, s.active_entity_id, npcs_active, s.num_npcs);
    }
    sf::Text meta(hdr, font, 12);
    meta.setPosition(820.f, 16.f);
    meta.setFillColor(s.pvp_mode ? sf::Color(239, 68, 68) : sf::Color(140, 140, 150));
    w.draw(meta);
    if (s.eclipse_env_offer && !s.resource_table.entries[2].present) {
        sf::RectangleShape noticeBg(sf::Vector2f(460.f, 26.f));
        noticeBg.setPosition(792.f, 760.f);
        noticeBg.setFillColor(sf::Color(36, 14, 60, 220));
        noticeBg.setOutlineThickness(1.f);
        noticeBg.setOutlineColor(sf::Color(230, 190, 255));
        w.draw(noticeBg);

        sf::Text rel("ECLIPSE RELIC AVAILABLE: press P on your turn", font, 14);
        rel.setPosition(804.f, 765.f);
        rel.setFillColor(sf::Color(245, 225, 255));
        w.draw(rel);
    }

    if (!s.pvp_mode) {
        sf::RectangleShape progressBg(sf::Vector2f(1244.f, 6.f));
        progressBg.setPosition(18.f, 48.f);
        progressBg.setFillColor(sf::Color(13, 13, 13));
        w.draw(progressBg);
        sf::RectangleShape progress(sf::Vector2f(1244.f * std::min(1.f, s.enemies_killed / 10.f), 6.f));
        progress.setPosition(18.f, 48.f);
        progress.setFillColor(sf::Color(250, 204, 21));
        w.draw(progress);
    } else {
        
        sf::RectangleShape divider(sf::Vector2f(1244.f, 3.f));
        divider.setPosition(18.f, 48.f);
        divider.setFillColor(sf::Color(239, 68, 68, 120));
        w.draw(divider);
    }

    const char* tabs[4] = {"battle", "inventory", "artifacts", "system"};
    for (int i = 0; i < 4; ++i) {
        sf::RectangleShape tab(sf::Vector2f(120.f, 26.f));
        tab.setPosition(18.f + i * 124.f, 64.f);
        tab.setFillColor(i == active_tab ? sf::Color(20, 20, 36) : sf::Color(10, 10, 16));
        tab.setOutlineThickness(1.f);
        tab.setOutlineColor(i == active_tab ? sf::Color(96, 165, 250) : sf::Color(30, 30, 30));
        w.draw(tab);
        sf::Text tx(tabs[i], font, 12);
        tx.setPosition(48.f + i * 124.f, 70.f);
        tx.setFillColor(i == active_tab ? sf::Color(96, 165, 250) : sf::Color(80, 80, 80));
        w.draw(tx);
    }

    if (active_tab == 1) {
        sf::Text t("◈ INVENTORY", font, 14);
        t.setPosition(26.f, 108.f);
        t.setFillColor(sf::Color(96, 165, 250));
        w.draw(t);
        constexpr float row0 = 128.f;
        constexpr float row_step = 200.f;
        constexpr float lts_x0 = 26.f;
        constexpr float lts_y_off = 78.f;
        constexpr float chip_w = 130.f;
        constexpr float chip_h = 22.f;
        constexpr float chip_gap = 136.f;
        for (int r = 0; r < s.num_players; ++r) {
            float y = row0 + r * row_step;
            sf::Text p(std::string("PLAYER E") + std::to_string(r), font, 12);
            p.setPosition(26.f, y);
            p.setFillColor(sf::Color(180, 180, 210));
            w.draw(p);
            const int evslot = (r < MAX_PLAYERS) ? s.ui_primary_evict_slot[r] : -1;
            for (int i = 0; i < 20; ++i) {
                WeaponID wid = s.entities[r].inventory.slots[i];
                sf::RectangleShape cell(sf::Vector2f(22.f, 22.f));
                cell.setPosition(26.f + i * 24.f, y + 24.f);
                cell.setFillColor(wid == WPN_NONE ? sf::Color(15, 15, 31) : sf::Color(30, 50, 80));
                const bool is_evict_target = (evslot == i && wid != WPN_NONE && !WEAPON_TABLE[wid].is_artifact);
                cell.setOutlineThickness(is_evict_target ? 2.f : 1.f);
                cell.setOutlineColor(is_evict_target ? sf::Color(251, 146, 60) : sf::Color(40, 40, 55));
                w.draw(cell);
                if (wid != WPN_NONE) {
                    char abbr[4];
                    std::snprintf(abbr, sizeof(abbr), "%.3s", WEAPON_TABLE[wid].name);
                    sf::Text mark(abbr, font, 9);
                    mark.setPosition(28.f + i * 24.f, y + 28.f);
                    mark.setFillColor(WEAPON_TABLE[wid].is_artifact
                                      ? sf::Color(250, 204, 21)
                                      : sf::Color(232, 228, 208));
                    w.draw(mark);
                }
            }
            sf::Text lts_hdr("LONG-TERM STORAGE — click a chip to choose Swap In weapon for this player", font, 10);
            lts_hdr.setPosition(26.f, y + 52.f);
            lts_hdr.setFillColor(sf::Color(120, 160, 200));
            w.draw(lts_hdr);
            const Inventory& inv = s.entities[r].inventory;
            if (inv.lts_count == 0) {
                sf::Text empty("(empty)", font, 10);
                empty.setPosition(lts_x0, y + lts_y_off);
                empty.setFillColor(sf::Color(90, 90, 110));
                w.draw(empty);
            } else {
                for (int k = 0; k < inv.lts_count; ++k) {
                    float lx = lts_x0 + k * chip_gap;
                    float ly = y + lts_y_off;
                    sf::RectangleShape chip(sf::Vector2f(chip_w, chip_h));
                    chip.setPosition(lx, ly);
                    const bool sel = (k == s.ui_lts_pick[r]);
                    chip.setFillColor(sel ? sf::Color(40, 70, 50) : sf::Color(22, 22, 40));
                    chip.setOutlineThickness(sel ? 2.f : 1.f);
                    chip.setOutlineColor(sel ? sf::Color(74, 222, 128) : sf::Color(55, 55, 70));
                    w.draw(chip);
                    WeaponID lw = inv.lts[k];
                    char buf[48];
                    std::snprintf(buf, sizeof(buf), "[%d] %s", k, WEAPON_TABLE[lw].name);
                    sf::Text tx(buf, font, 9);
                    tx.setPosition(lx + 4.f, ly + 5.f);
                    tx.setFillColor(sf::Color(220, 218, 200));
                    w.draw(tx);
                }
            }
        }
        return;
    }
    if (active_tab == 2) {
        sf::Text t("◈ GLOBAL ARTIFACT TABLE", font, 14);
        t.setPosition(26.f, 108.f);
        t.setFillColor(sf::Color(250, 204, 21));
        w.draw(t);
        const char* art[3] = {"Solar Core", "Lunar Blade", "Eclipse Relic"};
        for (int i = 0; i < 3; ++i) {
            sf::RectangleShape row(sf::Vector2f(620.f, 66.f));
            row.setPosition(26.f, 142.f + i * 78.f);
            row.setFillColor(sf::Color(9, 9, 15));
            row.setOutlineThickness(1.f);
            row.setOutlineColor(sf::Color(40, 40, 55));
            w.draw(row);
            sf::Text n(art[i], font, 13);
            n.setPosition(36.f, 158.f + i * 78.f);
            n.setFillColor(i == 0 ? sf::Color(250, 204, 21) : (i == 1 ? sf::Color(129, 140, 248) : sf::Color(192, 132, 252)));
            w.draw(n);
            char st[96];
            const auto& re = s.resource_table.entries[i];
            if (i == 2 && s.eclipse_env_offer && !re.present)
                std::snprintf(st, sizeof(st), "State: offered in arena (P to pick up) — not in pool until claimed");
            else if (!re.present)
                std::snprintf(st, sizeof(st), "State: not in pool");
            else if (re.held_by < 0)
                std::snprintf(st, sizeof(st), "State: in pool, unheld (free)");
            else
                std::snprintf(st, sizeof(st), "State: held by entity E%d", re.held_by);
            sf::Text rs(st, font, 10);
            rs.setPosition(36.f, 178.f + i * 78.f);
            rs.setFillColor(sf::Color(130, 130, 145));
            w.draw(rs);
        }
        return;
    }
    if (active_tab == 3) {
        sf::Text t("◈ SYSTEM  [Temporal Scheduling — Arrival Time Logic]", font, 13);
        t.setPosition(26.f, 108.f);
        t.setFillColor(sf::Color(96, 165, 250));
        w.draw(t);

        char l1[128], l2[128], l3[128], l4[128];
        std::snprintf(l1, sizeof(l1), "Tick interval: %d ms  |  Players: %d  NPCs: %d", TICK_INTERVAL_MS, s.num_players, s.num_npcs);
        std::snprintf(l2, sizeof(l2), "Active Entity: E%d  |  Tick: %d  |  Kills: %d/10", s.active_entity_id, s.tick_count, s.enemies_killed);
        std::snprintf(l3, sizeof(l3), "Arrival formula: ticks_to_full = (MaxST - ST) / (Speed x %dms/1000)", TICK_INTERVAL_MS);
        const char* gs_str = (s.game_status == GAME_RUNNING)      ? "RUNNING"
                        : (s.game_status == GAME_WON)          ? "WON"
                        : (s.game_status == GAME_LOST)         ? "LOST"
                        : (s.game_status == GAME_TEAM_A_WINS)  ? "TEAM A WINS"
                        : (s.game_status == GAME_TEAM_B_WINS)  ? "TEAM B WINS"
                        :                                        "QUIT";
        std::snprintf(l4, sizeof(l4), "GameStatus: %s", gs_str);
        const char* hdr[4] = {l1, l2, l3, l4};
        for (int i = 0; i < 4; ++i) {
            sf::Text ln(hdr[i], font, 11);
            ln.setPosition(26.f, 132.f + i * 18.f);
            ln.setFillColor(i == 2 ? sf::Color(250, 204, 21) : sf::Color(150, 150, 165));
            w.draw(ln);
        }

        
        sf::Text hd("Entity        Type     Spd   ST / MaxST       ETA(ticks)  Status", font, 10);
        hd.setPosition(26.f, 212.f);
        hd.setFillColor(sf::Color(96, 165, 250));
        w.draw(hd);

        int total = s.num_players + s.num_npcs;
        for (int i = 0; i < total && i < 13; ++i) {
            const Entity& e = s.entities[i];
            float eta = 0.f;
            if (e.status == ALIVE && e.speed > 0.f && e.stamina < e.max_stamina)
                eta = (e.max_stamina - e.stamina) / (e.speed * (TICK_INTERVAL_MS / 1000.0f));
            char row[128];
            const char* st_str = e.status == ALIVE ? "ALIVE" : e.status == STUNNED ? "STUNNED" : "DEAD";
            std::snprintf(row, sizeof(row), "E%-2d %-9s  P%-3s  %4.0f  %5.0f/%-5.0f  %8.0f  %s",
                i, (e.type == TYPE_PLAYER ? "PLAYER" : "NPC"),
                (s.active_entity_id == i ? "*" : " "),
                e.speed, e.stamina, e.max_stamina,
                (e.status == ALIVE && e.stamina >= e.max_stamina) ? 0.f : eta,
                st_str);
            sf::Text row_txt(row, font, 10);
            row_txt.setPosition(26.f, 228.f + i * 16.f);
            sf::Color c = (e.status == DEAD) ? sf::Color(60,60,70) :
                          (e.status == STUNNED) ? sf::Color(192,132,252) :
                          (s.active_entity_id == i) ? sf::Color(250,204,21) :
                          (e.type == TYPE_PLAYER) ? sf::Color(96,165,250) : sf::Color(248,113,113);
            row_txt.setFillColor(c);
            w.draw(row_txt);
        }
        return;
    }

    sf::RectangleShape leftPanel(sf::Vector2f(410.f, 460.f));
    leftPanel.setPosition(18.f, 102.f);
    leftPanel.setFillColor(sf::Color(9, 9, 15));
    leftPanel.setOutlineThickness(1.f);
    leftPanel.setOutlineColor(sf::Color(25, 25, 35));
    w.draw(leftPanel);

    sf::RectangleShape midPanel(sf::Vector2f(410.f, 460.f));
    midPanel.setPosition(435.f, 102.f);
    midPanel.setFillColor(sf::Color(9, 9, 15));
    midPanel.setOutlineThickness(1.f);
    midPanel.setOutlineColor(sf::Color(25, 25, 35));
    w.draw(midPanel);

    sf::RectangleShape rightPanel(sf::Vector2f(417.f, 460.f));
    rightPanel.setPosition(852.f, 102.f);
    rightPanel.setFillColor(sf::Color(9, 9, 15));
    rightPanel.setOutlineThickness(1.f);
    rightPanel.setOutlineColor(sf::Color(25, 25, 35));
    w.draw(rightPanel);

    sf::Text pTitle("◈ PLAYER PARTY", font, 11);
    pTitle.setPosition(28.f, 110.f);
    pTitle.setFillColor(sf::Color(96, 165, 250));
    w.draw(pTitle);
    
    char eTitleBuf[64];
    if (s.pvp_mode && s.active_entity_id >= 0) {
        const bool actor_is_A = (s.active_entity_id < s.player2_start_idx);
        std::snprintf(eTitleBuf, sizeof(eTitleBuf), "\u25c8 %s  (opponents)",
                      actor_is_A ? "TEAM B" : "TEAM A");
    } else {
        std::snprintf(eTitleBuf, sizeof(eTitleBuf), "\u25c8 ENEMIES  (%d active)", npcs_active);
    }
    sf::Text eTitle(eTitleBuf, font, 11);
    eTitle.setPosition(445.f, 110.f);
    eTitle.setFillColor(sf::Color(248, 113, 113));
    w.draw(eTitle);
    sf::Text logTitle("◈ ACTION LOG", font, 11);
    logTitle.setPosition(862.f, 110.f);
    logTitle.setFillColor(sf::Color(74, 222, 128));
    w.draw(logTitle);

    for (int i = 0; i < s.num_players; ++i) {
        const Entity& e = s.entities[i];
        float y = 132.f + i * 90.f;
        sf::RectangleShape card(sf::Vector2f(390.f, 84.f));
        card.setPosition(28.f, y);
        card.setFillColor(s.active_entity_id == i ? sf::Color(13, 20, 37) : sf::Color(9, 9, 15));
        card.setOutlineThickness(1.f);
        card.setOutlineColor(s.active_entity_id == i ? sf::Color(96, 165, 250) : sf::Color(30, 30, 30));
        w.draw(card);

        sf::Text name(std::string("P") + std::to_string(e.ui_label) + " PLAYER", font, 12);
        name.setPosition(36.f, y + 8.f);
        name.setFillColor(sf::Color(96, 165, 250));
        w.draw(name);
        
        if (s.pvp_mode) {
            const bool is_A = (i < s.player2_start_idx);
            sf::Text tag(is_A ? "TEAM A" : "TEAM B", font, 9);
            tag.setPosition(36.f, y + 72.f);
            tag.setFillColor(is_A ? sf::Color(96, 165, 250) : sf::Color(239, 68, 68));
            w.draw(tag);
        }

        draw_bar(w, 95.f, y + 32.f, 220.f, 9.f, (float)e.hp, (float)e.max_hp, sf::Color(74, 222, 128));
        draw_bar(w, 95.f, y + 50.f, 220.f, 8.f, e.stamina, e.max_stamina, sf::Color(96, 165, 250));

        char hpTxt[64], stTxt[64];
        std::snprintf(hpTxt, sizeof(hpTxt), "HP %d/%d", e.hp, e.max_hp);
        std::snprintf(stTxt, sizeof(stTxt), "ST %.0f/%.0f", e.stamina, e.max_stamina);
        sf::Text hpLabel(hpTxt, font, 10);
        hpLabel.setPosition(322.f, y + 27.f);
        hpLabel.setFillColor(sf::Color(74, 222, 128));
        w.draw(hpLabel);
        sf::Text stLabel(stTxt, font, 10);
        stLabel.setPosition(322.f, y + 45.f);
        stLabel.setFillColor(sf::Color(96, 165, 250));
        w.draw(stLabel);

        
        {
            bool shown[WPN_COUNT] {};
            char wpn_buf[96] {};
            char* ptr = wpn_buf;
            ptr += std::snprintf(ptr, 6, "WPN: ");
            bool any = false;
            for (int s = 0; s < INVENTORY_SIZE; ++s) {
                WeaponID wslot = e.inventory.slots[s];
                if (wslot == WPN_NONE || (int)wslot >= WPN_COUNT || shown[(int)wslot]) continue;
                shown[(int)wslot] = true;
                int rem = (int)(wpn_buf + sizeof(wpn_buf) - ptr) - 4;
                if (rem <= 0) { std::snprintf(ptr, 4, "..."); ptr += 3; break; }
                ptr += std::snprintf(ptr, rem, "%s%s", any ? ", " : "", WEAPON_TABLE[wslot].name);
                any = true;
            }
            if (!any) std::snprintf(wpn_buf, sizeof(wpn_buf), "WPN: (none)");
            sf::Text wpnTxt(wpn_buf, font, 9);
            wpnTxt.setPosition(36.f, y + 66.f);
            wpnTxt.setFillColor(any ? sf::Color(110, 200, 130) : sf::Color(80, 80, 90));
            w.draw(wpnTxt);
        }
        
        {
            bool has_sc = false, has_lb = false;
            for (int s = 0; s < INVENTORY_SIZE; ++s) {
                if (e.inventory.slots[s] == WPN_SOLAR_CORE)  has_sc = true;
                if (e.inventory.slots[s] == WPN_LUNAR_BLADE) has_lb = true;
            }
            if (has_sc && has_lb && e.status != DEAD) {
                sf::RectangleShape badge(sf::Vector2f(120.f, 16.f));
                badge.setPosition(260.f, y + 64.f);
                badge.setFillColor(sf::Color(80, 60, 0, 200));
                badge.setOutlineThickness(1.f);
                badge.setOutlineColor(sf::Color(250, 204, 21));
                w.draw(badge);
                sf::Text ultTxt("ULTIMATE READY", font, 9);
                ultTxt.setPosition(264.f, y + 67.f);
                ultTxt.setFillColor(sf::Color(250, 204, 21));
                w.draw(ultTxt);
            }
        }

        if (e.status == STUNNED) {
            sf::RectangleShape ov(sf::Vector2f(390.f, 84.f));
            ov.setPosition(28.f, y);
            ov.setFillColor(sf::Color(80, 0, 140, 120));
            w.draw(ov);
            sf::Text st("STUNNED", font, 18);
            st.setPosition(160.f, y + 30.f);
            st.setFillColor(sf::Color(216, 180, 254));
            w.draw(st);
        } else if (e.status == DEAD) {
            sf::RectangleShape ov(sf::Vector2f(390.f, 84.f));
            ov.setPosition(28.f, y);
            ov.setFillColor(sf::Color(20, 20, 20, 160));
            w.draw(ov);
            sf::Text dt("DEAD", font, 18);
            dt.setPosition(185.f, y + 30.f);
            dt.setFillColor(sf::Color(100, 100, 110));
            w.draw(dt);
        }
    }

    
    int opp_start = 0, opp_end = 0;
    if (s.pvp_mode && s.active_entity_id >= 0) {
        const bool actor_is_A = (s.active_entity_id < s.player2_start_idx);
        opp_start = actor_is_A ? s.player2_start_idx : 0;
        opp_end   = actor_is_A ? s.num_players : s.player2_start_idx;
    } else {
        opp_start = s.num_players;
        opp_end   = s.num_players + s.num_npcs;
    }
    int opp_alive_count = 0;
    for (int _i = opp_start; _i < opp_end; ++_i)
        if (s.entities[_i].status != DEAD) ++opp_alive_count;

    
    const float npc_avail_h = 430.f;
    const float card_step = (opp_alive_count > 0) ? std::min(90.f, npc_avail_h / (float)opp_alive_count) : 90.f;
    const float card_h    = card_step - 4.f;
    const float sc        = card_h / 84.f;

    int vis_npc = 0;
    for (int i = opp_start; i < opp_end; ++i) {
        const Entity& e = s.entities[i];
        if (e.status == DEAD) continue;
        float y = 132.f + vis_npc * card_step;
        ++vis_npc;

        sf::RectangleShape card(sf::Vector2f(390.f, card_h));
        card.setPosition(445.f, y);
        card.setFillColor(s.active_entity_id == i ? sf::Color(26, 13, 13) : sf::Color(9, 9, 15));
        card.setOutlineThickness(1.f);
        if (selected_target == i) card.setOutlineColor(sf::Color(250, 204, 21));
        else card.setOutlineColor(s.active_entity_id == i ? sf::Color(248, 113, 113) : sf::Color(30, 30, 30));
        w.draw(card);

        
        char opp_label_buf[32];
        if (s.pvp_mode)
            std::snprintf(opp_label_buf, sizeof(opp_label_buf), "P%d PLAYER", e.ui_label);
        else
            std::snprintf(opp_label_buf, sizeof(opp_label_buf), "ENEMY %d", e.ui_label);
        sf::Text name(opp_label_buf, font, std::max(8, (int)(12.f * sc)));
        name.setPosition(453.f, y + std::max(2.f, 8.f * sc));
        name.setFillColor(s.pvp_mode ? sf::Color(239, 68, 68) : sf::Color(248, 113, 113));
        w.draw(name);
        if (selected_target == i) {
            sf::Text tgt("\u25b6 TARGET", font, std::max(8, (int)(10.f * sc)));
            tgt.setPosition(620.f, y + std::max(2.f, 8.f * sc));
            tgt.setFillColor(sf::Color(250, 204, 21));
            w.draw(tgt);
        }

        const float bar_h1 = std::max(3.f, 9.f * sc);
        const float bar_h2 = std::max(3.f, 8.f * sc);
        const float bar_y1 = y + card_h * 0.36f;
        const float bar_y2 = y + card_h * 0.58f;
        draw_bar(w, 512.f, bar_y1, 220.f, bar_h1, (float)e.hp, (float)e.max_hp, sf::Color(248, 113, 113));
        draw_bar(w, 512.f, bar_y2, 220.f, bar_h2, e.stamina, e.max_stamina, sf::Color(192, 132, 252));

        char hpTxt[64], stTxt[64];
        std::snprintf(hpTxt, sizeof(hpTxt), "HP %d/%d", e.hp, e.max_hp);
        std::snprintf(stTxt, sizeof(stTxt), "ST %.0f/%.0f", e.stamina, e.max_stamina);
        sf::Text hpLabel(hpTxt, font, 10);
        hpLabel.setPosition(739.f, bar_y1 - 5.f);
        hpLabel.setFillColor(sf::Color(248, 113, 113));
        w.draw(hpLabel);
        sf::Text stLabel(stTxt, font, 10);
        stLabel.setPosition(739.f, bar_y2 - 4.f);
        stLabel.setFillColor(sf::Color(192, 132, 252));
        w.draw(stLabel);

        
        if (card_h > 44.f) {
            bool shown[WPN_COUNT] {};
            char wpn_buf[96] {};
            char* ptr = wpn_buf;
            ptr += std::snprintf(ptr, 6, "WPN: ");
            bool any = false;
            for (int s = 0; s < INVENTORY_SIZE; ++s) {
                WeaponID wslot = e.inventory.slots[s];
                if (wslot == WPN_NONE || (int)wslot >= WPN_COUNT || shown[(int)wslot]) continue;
                shown[(int)wslot] = true;
                int rem = (int)(wpn_buf + sizeof(wpn_buf) - ptr) - 4;
                if (rem <= 0) { std::snprintf(ptr, 4, "..."); ptr += 3; break; }
                ptr += std::snprintf(ptr, rem, "%s%s", any ? ", " : "", WEAPON_TABLE[wslot].name);
                any = true;
            }
            if (!any) std::snprintf(wpn_buf, sizeof(wpn_buf), "WPN: (none)");
            sf::Text wpnTxt(wpn_buf, font, 9);
            wpnTxt.setPosition(453.f, y + card_h * 0.80f);
            wpnTxt.setFillColor(any ? sf::Color(220, 120, 100) : sf::Color(80, 80, 90));
            w.draw(wpnTxt);
        }

        if (e.status == STUNNED) {
            sf::RectangleShape ov(sf::Vector2f(390.f, card_h));
            ov.setPosition(445.f, y);
            ov.setFillColor(sf::Color(80, 0, 140, 120));
            w.draw(ov);
            sf::Text st("STUNNED", font, std::max(9, (int)(18.f * sc)));
            st.setPosition(577.f, y + card_h * 0.35f);
            st.setFillColor(sf::Color(216, 180, 254));
            w.draw(st);
        }
    }

    sf::RectangleShape logBox(sf::Vector2f(397.f, 240.f));
    logBox.setPosition(862.f, 132.f);
    logBox.setFillColor(sf::Color(7, 7, 12));
    logBox.setOutlineThickness(1.f);
    logBox.setOutlineColor(sf::Color(25, 25, 35));
    w.draw(logBox);
    for (int i = 0; i < 14; ++i) {
        int idx = (s.log.head - 1 - i + LOG_LINES) % LOG_LINES;
        char prefixed[LOG_LINE_LEN + 8];  
        std::snprintf(prefixed, sizeof(prefixed), "[%03d] %s", i, s.log.lines[idx]);
        sf::Text l(prefixed, font, 10);
        l.setPosition(870.f, 140.f + i * 16.f);
        l.setFillColor(sf::Color(135, 135, 150));
        w.draw(l);
    }

    sf::RectangleShape procBox(sf::Vector2f(397.f, 150.f));
    procBox.setPosition(862.f, 382.f);
    procBox.setFillColor(sf::Color(7, 7, 12));
    procBox.setOutlineThickness(1.f);
    procBox.setOutlineColor(sf::Color(25, 25, 35));
    w.draw(procBox);
    sf::Text procTitle("PROCESS TABLE", font, 11);
    procTitle.setPosition(872.f, 390.f);
    procTitle.setFillColor(sf::Color(130, 130, 140));
    w.draw(procTitle);

    char p1[96], p2[96], p3[96], p4[96], p5[96], p6[96];
    std::snprintf(p1, sizeof(p1), "Arbiter        PID:%d   RUNNING", s.arbiter_pid);
    if (s.pvp_mode) {
        if (s.human_pid2 > 0) {
            std::snprintf(p2, sizeof(p2), "HI Process 1   PID:%d   TEAM A: E0-E%d", s.human_pid, s.player2_start_idx - 1);
            std::snprintf(p3, sizeof(p3), "HI Process 2   PID:%d   TEAM B: E%d-E%d", s.human_pid2, s.player2_start_idx, s.num_players - 1);
        } else {
            std::snprintf(p2, sizeof(p2), "HIP+UI         PID:%d   TEAM A: E0-E%d", s.human_pid, s.player2_start_idx - 1);
            std::snprintf(p3, sizeof(p3), "HIP+UI         PID:%d   TEAM B: E%d-E%d", s.human_pid, s.player2_start_idx, s.num_players - 1);
        }
        std::snprintf(p4, sizeof(p4), "NPC Process    (not launched in PvP mode)");
    } else {
        std::snprintf(p2, sizeof(p2), "HI Process     PID:%d   RUNNING", s.human_pid);
        std::snprintf(p3, sizeof(p3), "NPC Process    PID:%d   RUNNING", s.npc_pid);
        std::snprintf(p4, sizeof(p4), "Render Thread  PID:T01  RUNNING");
    }
    std::snprintf(p5, sizeof(p5), "Render Thread  PID:T01  RUNNING");
    std::snprintf(p6, sizeof(p6), "Deadlock Mon.  PID:T02  RUNNING");
    const char* proc_lines[6] = {p1, p2, p3, p4, p5, p6};
    const int proc_count = s.pvp_mode ? 6 : 5;
    for (int i = 0; i < proc_count; ++i) {
        sf::Text ln(proc_lines[i], font, 10);
        ln.setPosition(872.f, 408.f + i * 18.f);
        ln.setFillColor(i == 2 && s.pvp_mode ? sf::Color(239, 68, 68)
                       : i == 1 && s.pvp_mode ? sf::Color(96, 165, 250)
                       :                        sf::Color(95, 95, 110));
        w.draw(ln);
    }

    sf::RectangleShape shmBox(sf::Vector2f(397.f, 44.f));
    shmBox.setPosition(862.f, 534.f);
    shmBox.setFillColor(sf::Color(7, 7, 12));
    shmBox.setOutlineThickness(1.f);
    shmBox.setOutlineColor(sf::Color(25, 25, 35));
    w.draw(shmBox);
    sf::Text shmTitle("SHARED MEMORY", font, 11);
    shmTitle.setPosition(872.f, 538.f);
    shmTitle.setFillColor(sf::Color(130, 130, 140));
    w.draw(shmTitle);

    char sm1[80], sm2[80], sm3[80], sm4[80];
    std::snprintf(sm1, sizeof(sm1), "Mutex: %s", (s.active_entity_id >= 0 ? "LOCKED" : "FREE"));
    std::snprintf(sm2, sizeof(sm2), "Semaphore: 1");
    std::snprintf(sm3, sizeof(sm3), "Active Turn: E%d", s.active_entity_id);
    std::snprintf(sm4, sizeof(sm4), "Kill Count: %d", s.enemies_killed);
    const char* sm[4] = {sm1, sm2, sm3, sm4};
    for (int i = 0; i < 4; ++i) {
        sf::Text ln(sm[i], font, 10);
        ln.setPosition(968.f, 538.f + i * 10.f);
        ln.setFillColor(i == 3 ? sf::Color(96, 165, 250) : sf::Color(105, 105, 120));
        w.draw(ln);
    }

    
    if (active_tab == 0 && s.active_entity_id >= 0 && s.active_entity_id < s.num_players) {
        const int pid = s.active_entity_id;
        const Inventory& pinv = s.entities[pid].inventory;
        if (pinv.lts_count > 0) {
            int sel = (pid < MAX_PLAYERS) ? s.ui_lts_pick[pid] : 0;
            if (sel < 0 || sel >= pinv.lts_count) sel = 0;
            const int evslot2 = (pid < MAX_PLAYERS) ? s.ui_primary_evict_slot[pid] : -1;
            WeaponID evwpn = (evslot2 >= 0 && evslot2 < INVENTORY_SIZE) ? pinv.slots[evslot2] : WPN_NONE;
            
            if (evwpn != WPN_NONE && WEAPON_TABLE[evwpn].is_artifact) evwpn = WPN_NONE;
            char ltsbuf[192];
            if (evwpn != WPN_NONE) {
                std::snprintf(ltsbuf, sizeof(ltsbuf),
                              "SWAP IN  [%d/%d] %-14s  will displace: %-14s   (Inv tab: click slot to change | click again to clear)",
                              sel + 1, pinv.lts_count, WEAPON_TABLE[pinv.lts[sel]].name, WEAPON_TABLE[evwpn].name);
            } else {
                std::snprintf(ltsbuf, sizeof(ltsbuf),
                              "SWAP IN  [%d/%d] %-14s  <- / -> cycle LTS  |  Inv tab: click a primary slot (orange) to pick what gets evicted",
                              sel + 1, pinv.lts_count, WEAPON_TABLE[pinv.lts[sel]].name);
            }
            sf::Text ltsHint(ltsbuf, font, 10);
            ltsHint.setPosition(240.f, 568.f);
            ltsHint.setFillColor(evwpn != WPN_NONE ? sf::Color(251, 146, 60) : sf::Color(100, 200, 140));
            w.draw(ltsHint);
        }
    }

    sf::RectangleShape actionBar(sf::Vector2f(700.f, 56.f));
    actionBar.setPosition(240.f, 580.f);
    actionBar.setFillColor(sf::Color(10, 10, 25, 230));
    actionBar.setOutlineThickness(1.f);
    actionBar.setOutlineColor(sf::Color(40, 40, 70));
    w.draw(actionBar);
    
    bool ultimate_unlocked = false;
    {
        const int pid = s.active_entity_id;
        if (pid >= 0 && pid < s.num_players) {
            bool has_solar = false, has_lunar = false;
            for (int sl = 0; sl < INVENTORY_SIZE; ++sl) {
                if (s.entities[pid].inventory.slots[sl] == WPN_SOLAR_CORE) has_solar = true;
                if (s.entities[pid].inventory.slots[sl] == WPN_LUNAR_BLADE) has_lunar = true;
            }
            ultimate_unlocked = has_solar && has_lunar;
        }
    }
    for (size_t i = 0; i < buttons.size(); ++i) {
        const bool is_ult = (buttons[i].action == ACT_ULTIMATE);
        sf::Color fill_col  = ((int)i == hover_idx) ? sf::Color(48, 48, 88) : sf::Color(35, 35, 60);
        sf::Color out_col   = sf::Color(70, 70, 100);
        sf::Color text_col  = sf::Color(232, 228, 208);
        if (is_ult) {
            if (s.ultimate_active) {
                fill_col = sf::Color(60, 40, 0);         
                out_col  = sf::Color(250, 204, 21);
                text_col = sf::Color(250, 204, 21);
            } else if (ultimate_unlocked) {
                fill_col = sf::Color(40, 32, 5);         
                out_col  = sf::Color(200, 160, 20);
                text_col = sf::Color(220, 175, 30);
            } else {
                fill_col = sf::Color(20, 20, 22);        
                out_col  = sf::Color(50, 50, 55);
                text_col = sf::Color(70, 70, 75);
            }
        }
        sf::RectangleShape b(sf::Vector2f(buttons[i].rect.width, buttons[i].rect.height));
        b.setPosition(buttons[i].rect.left, buttons[i].rect.top);
        b.setFillColor(fill_col);
        b.setOutlineThickness(is_ult ? 2.f : 1.f);
        b.setOutlineColor(out_col);
        w.draw(b);
        sf::Text tx(buttons[i].label, font, 14);
        tx.setPosition(buttons[i].rect.left + 16.f, buttons[i].rect.top + 10.f);
        tx.setFillColor(text_col);
        w.draw(tx);
    }

    sf::Text footer("Mouse: click tabs to switch • battle tab supports actions", font, 12);
    footer.setPosition(18.f, 760.f);
    footer.setFillColor(sf::Color(90, 90, 100));
    w.draw(footer);

    
    if (s.ultimate_active) {
        sf::RectangleShape banner(sf::Vector2f((float)WINDOW_W, 30.f));
        banner.setPosition(0.f, 0.f);
        banner.setFillColor(sf::Color(70, 45, 0, 240));
        banner.setOutlineThickness(0.f);
        w.draw(banner);
        sf::Text bt("*** ULTIMATE ACTIVE  --  ASP (NPC process) suspended via SIGSTOP for 10 seconds  --  press U again? ***", font, 13);
        bt.setPosition(60.f, 6.f);
        bt.setFillColor(sf::Color(250, 204, 21));
        w.draw(bt);
    }

    if (s.drop_pending) {
        sf::RectangleShape overlay(sf::Vector2f((float)WINDOW_W, (float)WINDOW_H));
        overlay.setFillColor(sf::Color(0, 0, 0, 190));
        w.draw(overlay);
        sf::Text title("WEAPON DROPPED", font, 30);
        title.setPosition(470.f, 300.f);
        title.setFillColor(sf::Color(250, 204, 21));
        w.draw(title);
        char subbuf[192];
        std::snprintf(subbuf, sizeof(subbuf), "Choose in 8s: %s", WEAPON_TABLE[s.drop_weapon].name);
        sf::Text sub(subbuf, font, 18);
        sub.setPosition(450.f, 350.f);
        sub.setFillColor(sf::Color(180, 180, 200));
        w.draw(sub);
        sf::RectangleShape pbtn(sf::Vector2f(160.f, 44.f));
        pbtn.setPosition(470.f, 430.f);
        pbtn.setFillColor(sf::Color(20, 56, 37));
        pbtn.setOutlineThickness(2.f);
        pbtn.setOutlineColor(sf::Color(74, 222, 128));
        w.draw(pbtn);
        sf::Text ptxt("PICK UP", font, 16);
        ptxt.setPosition(510.f, 441.f);
        ptxt.setFillColor(sf::Color(74, 222, 128));
        w.draw(ptxt);
        sf::RectangleShape dbtn(sf::Vector2f(160.f, 44.f));
        dbtn.setPosition(650.f, 430.f);
        dbtn.setFillColor(sf::Color(70, 20, 20));
        dbtn.setOutlineThickness(2.f);
        dbtn.setOutlineColor(sf::Color(248, 113, 113));
        w.draw(dbtn);
        sf::Text dtxt("DECLINE", font, 16);
        dtxt.setPosition(688.f, 441.f);
        dtxt.setFillColor(sf::Color(248, 113, 113));
        w.draw(dtxt);
    }
    if (s.game_status != GAME_RUNNING) {
        sf::RectangleShape overlay(sf::Vector2f((float)WINDOW_W, (float)WINDOW_H));
        overlay.setFillColor(sf::Color(0, 0, 0, 180));
        w.draw(overlay);
        const char* title_str;
        sf::Color    title_col;
        const char* sub_str;
        if      (s.game_status == GAME_TEAM_A_WINS) { title_str = "TEAM A WINS!";          title_col = sf::Color(96,  165, 250); sub_str = "Team B eliminated.  GG"; }
        else if (s.game_status == GAME_TEAM_B_WINS) { title_str = "TEAM B WINS!";          title_col = sf::Color(239, 68,  68);  sub_str = "Team A eliminated.  GG"; }
        else if (s.game_status == GAME_WON)         { title_str = "VICTORY";               title_col = sf::Color(74,  222, 128); sub_str = "10 ENEMIES ELIMINATED"; }
        else                                         { title_str = "DEFEATED";              title_col = sf::Color(248, 113, 113); sub_str = "ALL PLAYERS DEAD"; }
        sf::Text title(title_str, font, 48);
        title.setPosition(470.f, 300.f);
        title.setFillColor(title_col);
        w.draw(title);
        sf::Text sub(sub_str, font, 18);
        sub.setPosition(470.f, 360.f);
        sub.setFillColor(sf::Color(180, 180, 200));
        w.draw(sub);
        sf::RectangleShape pbtn(sf::Vector2f(160.f, 48.f));
        pbtn.setPosition(470.f, 430.f);
        pbtn.setFillColor(sf::Color(20, 56, 37));
        pbtn.setOutlineThickness(2.f);
        pbtn.setOutlineColor(sf::Color(74, 222, 128));
        w.draw(pbtn);
        sf::Text ptxt("PLAY AGAIN", font, 18);
        ptxt.setPosition(492.f, 442.f);
        ptxt.setFillColor(sf::Color(74, 222, 128));
        w.draw(ptxt);
        sf::RectangleShape ebtn(sf::Vector2f(160.f, 48.f));
        ebtn.setPosition(650.f, 430.f);
        ebtn.setFillColor(sf::Color(70, 20, 20));
        ebtn.setOutlineThickness(2.f);
        ebtn.setOutlineColor(sf::Color(248, 113, 113));
        w.draw(ebtn);
        sf::Text etxt("EXIT", font, 18);
        etxt.setPosition(706.f, 442.f);
        etxt.setFillColor(sf::Color(248, 113, 113));
        w.draw(etxt);
    }
}
