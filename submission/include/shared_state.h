#ifndef SHARED_STATE_H
#define SHARED_STATE_H
// Poora game state ek hi shared memory chunk mein. Semaphore yahi pe hain taake teen processes sync kar saken.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>

constexpr int MAX_PLAYERS = 8;   
constexpr int MAX_ENEMIES = 9;
constexpr int MAX_ENTITIES = MAX_PLAYERS + MAX_ENEMIES;
constexpr int INVENTORY_SIZE = 20;
constexpr int MAX_WEAPONS_LTS = 20;
constexpr int LOG_LINES = 30;
constexpr int LOG_LINE_LEN = 192;

enum WeaponID {
    WPN_NONE = 0,
    WPN_SOLAR_CORE,
    WPN_LUNAR_BLADE,
    WPN_IRON_HALBERD,
    WPN_VENOM_DAGGER,
    WPN_THUNDERSTAFF,
    WPN_OBSIDIAN_AXE,
    WPN_FROSTBOW,
    WPN_SPLINTER_STICK,
    WPN_ECLIPSE_RELIC,
    WPN_COUNT
};

struct WeaponDef {
    const char* name;
    int slot_size;
    int damage;
    bool is_artifact;
};

extern const WeaponDef WEAPON_TABLE[WPN_COUNT];

enum EntityStatus { ALIVE, STUNNED, DEAD };
enum EntityType { TYPE_PLAYER, TYPE_NPC };
enum ActionType {
    ACT_NONE = 0,
    ACT_STRIKE,
    ACT_EXHAUST,
    ACT_USE_WEAPON,
    ACT_SWAP_IN,
    ACT_HEAL,
    ACT_SKIP,
    ACT_ULTIMATE,
    ACT_PICKUP,
    ACT_DECLINE
};
enum GameStatus { GAME_RUNNING, GAME_WON, GAME_LOST, GAME_QUIT, GAME_TEAM_A_WINS, GAME_TEAM_B_WINS };

struct Action {
    ActionType type;
    int actor_id;
    int target_id;
    WeaponID weapon_id;
    int aux_index;
    bool ready;
};

struct Inventory {
    WeaponID slots[INVENTORY_SIZE];
    WeaponID lts[MAX_WEAPONS_LTS];
    int lts_count;
};

struct ArtifactEntry {
    WeaponID weapon;
    bool present;
    int held_by;
};

struct ResourceTable {
    sem_t table_lock;
    ArtifactEntry entries[3];
};

struct Entity {
    int id;
    int ui_label;
    EntityType type;
    EntityStatus status;
    pid_t pid;
    
    pid_t linux_tid;
    pthread_t tid;
    int hp;
    int max_hp;
    int damage;
    float speed;
    float stamina;
    float max_stamina;
    bool turn_ready;
    bool holds_solar_core;
    bool holds_lunar_blade;
    bool holds_eclipse_relic;
    long long stun_until_ms;
    Action pending_action;
    Inventory inventory;
};

struct ActionLog {
    char lines[LOG_LINES][LOG_LINE_LEN];
    int head;
    sem_t log_lock;
};


struct UiPlayerIntent {
    ActionType type;
    int target_id;
    WeaponID weapon_id;
    int aux_index;
    bool valid;
};

struct UiMailbox {
    sem_t lock;
    int menu_party_size;
    bool menu_begin_submitted;
    bool menu_demo_mode; 
    bool menu_pvp;        
    UiPlayerIntent player_intent[MAX_PLAYERS];
    
    int drop_user_pick;
    bool gameover_clicked;
    bool gameover_play_again;
};

struct SharedState {
    sem_t state_lock;
    sem_t action_submitted;
    sem_t render_ready;
    sem_t input_lock;

    GameStatus game_status;
    int enemies_killed;
    int num_players;
    int num_npcs;
    int active_entity_id;
    int last_player_actor_id;
    
    int sched_rr_next;
    int tick_count;

    bool ui_setup_done;
    int requested_players;
    bool multiplayer_enabled;
    pid_t human_pid2;       
    int   player2_start_idx; 
    int   pvp_mode;          
    int   pvp_last_team;     
    bool ui_gameover_ack;
    bool play_again_requested;
    int total_enemy_spawned;
    bool drop_pending;
    int drop_owner_id;
    int drop_source_enemy_label;
    WeaponID drop_weapon;
    long long drop_expire_ms;
    int drop_choice; 
    

    bool ultimate_active;

    
    int ui_active_tab;
    int ui_hover_idx;
    ActionType ui_selected_action;
    int ui_selected_target;
    
    int ui_lts_pick[MAX_PLAYERS];
    
    int ui_primary_evict_slot[MAX_PLAYERS];

    
    int waits_for_resource[MAX_ENTITIES];
    
    WeaponID pending_artifact_grant[MAX_ENTITIES];
    
    bool eclipse_env_offer;

    UiMailbox ui_mailbox;

    pid_t arbiter_pid;
    pid_t human_pid;
    pid_t npc_pid;
    pid_t ui_pid;

    ResourceTable resource_table;
    ActionLog log;
    Entity entities[MAX_ENTITIES];
    Action input_buffer[MAX_PLAYERS];
};

#endif 
