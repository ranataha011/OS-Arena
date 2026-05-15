# ⚔️ OS-Arena

> A multi-process, multi-threaded turn-based RPG arena built in C++17 — demonstrating core Operating Systems concepts through an actual playable game.

Three separate Linux processes communicate exclusively through **POSIX shared memory** and **semaphores** to run a fully synchronized, turn-based combat game with a real SFML graphical interface.

---

## 📸 Overview

**OS Arena** is an OS-level project where the game engine itself _is_ the OS demonstration. Players (human or AI) battle enemies in a structured arena where every action, render frame, and AI decision crosses process boundaries via IPC primitives — no sockets, no pipes, just raw shared memory and semaphores.

---

## 🏗️ Architecture

The system is split into **three independent processes**, each compiled as its own binary:

```
┌─────────────────────────────────────────────────────────┐
│                   SHARED MEMORY (POSIX)                  │
│                    SharedState struct                    │
│          sem_t state_lock, action_submitted,             │
│          render_ready, input_lock, log_lock ...          │
└──────────────┬───────────────────┬──────────────────────┘
               │                   │                   │
       ┌───────▼──────┐   ┌────────▼───────┐   ┌──────▼──────┐
       │   arbiters   │   │     hips        │   │    asps     │
       │  (Arbiter)   │   │ (Human Interface│   │  (NPC/AI    │
       │              │   │   Process)      │   │  Process)   │
       │ • Creates SHM│   │ • SFML Window   │   │ • 1 pthread │
       │ • Turn sched │   │ • Input handler │   │   per enemy │
       │ • Deadlock   │   │ • 1 pthread per │   │ • AI logic  │
       │   monitor    │   │   human player  │   │ • Reads &   │
       │ • fork+exec  │   │ • Render thread │   │   writes    │
       │   hips & asps│   │ • HUD drawing   │   │   actions   │
       └──────────────┘   └─────────────────┘   └─────────────┘
```

| Binary | Source | Role |
|--------|--------|------|
| `arbiters` | `arbiter/` | Game master — owns SHM, runs scheduler, monitors deadlocks |
| `hips` | `hip/` | Human Interface Process — SFML GUI, player input, rendering |
| `asps` | `asp/` | Autonomous/NPC Process — AI-controlled enemies via pthreads |

---

## 🧠 OS Concepts Demonstrated

### 1. Inter-Process Communication (IPC) via Shared Memory
All game state lives in a single `SharedState` struct mapped into every process via `shmget` / `shmat`. No copying, no serialization — every process reads and writes the same physical memory.

```cpp
int shmid = shmget(SHM_KEY, sizeof(SharedState), IPC_CREAT | 0666);
SharedState* state = (SharedState*)shmat(shmid, nullptr, 0);
```

### 2. POSIX Semaphores for Synchronization
Every critical section is guarded by named semaphores embedded directly in shared memory:

| Semaphore | Purpose |
|-----------|---------|
| `state_lock` | Mutual exclusion on the full game state |
| `action_submitted` | Signals Arbiter that a player action is ready |
| `render_ready` | Signals HIP that a new frame can be drawn |
| `input_lock` | Serializes input buffer writes from player threads |
| `log_lock` | Protects the circular action log |
| `table_lock` | Guards the artifact resource table |

### 3. Multi-threading (pthreads)
- **HIP**: spawns one `pthread_t` per human player + a dedicated render thread
- **ASP**: spawns one `pthread_t` per NPC enemy
- **Arbiter**: runs a background `deadlock_monitor` thread alongside the main scheduler

### 4. Process Management
- Arbiter uses `fork()` + `exec()` to launch `hips` and `asps` as child processes
- Handles `SIGTERM`, `SIGCHLD` for clean shutdown
- Tracks `pid_t` of each child process in shared memory for cross-process signaling

### 5. Deadlock Detection & Recovery
A dedicated background thread in Arbiter runs a **wait-for graph** cycle detection algorithm (DFS-based) over the artifact resource table. If a deadlock is detected among entities waiting on artifacts (Solar Core, Lunar Blade, Eclipse Relic), the highest-priority victim is selected and preempted — its held resources are stripped.

```
Cycle detection: DFS coloring (white=0, gray=1, black=2)
Victim selection: highest entity ID in the detected cycle
Recovery: strip artifact, update resource_table, log action
```

### 6. Custom Turn Scheduler
Arbiter's `scheduling_loop` implements a **speed-weighted round-robin** scheduler. Each entity has a `speed` attribute and a `stamina` pool. The scheduler ticks every 50 ms, drains stamina, and selects the next active entity for its turn. It then blocks on `sem_wait(&state->action_submitted)` until the player or AI submits an action.

### 7. Resource (Artifact) Table
Three legendary weapons act as **exclusive resources** managed via a semaphore-protected `ResourceTable`. Entities must "acquire" an artifact to equip it; only one holder at a time is allowed — creating real resource contention and enabling deadlock scenarios.

---

## 🎮 Game Modes

| Mode | Description |
|------|-------------|
| **Solo** | 1 human player vs. waves of NPC enemies |
| **Co-op** | 2–8 human players (on one machine) vs. enemies |
| **PvP Local** | Two human processes on separate terminals — Team A vs. Team B |
| **Demo** | Fully automated NPC vs. NPC — watch the OS do everything |

---

## ⚔️ Weapons & Artifacts

| Weapon | Type | Notes |
|--------|------|-------|
| Solar Core | 🔴 Artifact | Exclusive resource — causes deadlock potential |
| Lunar Blade | 🔵 Artifact | Exclusive resource — causes deadlock potential |
| Eclipse Relic | 🟣 Artifact | Exclusive resource — triggers special Eclipse environment |
| Iron Halberd | Normal | High damage melee |
| Venom Dagger | Normal | Fast, poison-based |
| Thunderstaff | Normal | AoE capable |
| Obsidian Axe | Normal | Heavy, stamina cost |
| Frostbow | Normal | Ranged, slows targets |
| Splinter Stick | Normal | Weak fallback drop |

### Combat Actions

| Action | Description |
|--------|-------------|
| `STRIKE` | Basic physical attack |
| `USE_WEAPON` | Activate equipped weapon ability |
| `SWAP_IN` | Equip a weapon from long-term storage |
| `EXHAUST` | Burn stamina for power |
| `HEAL` | Restore HP |
| `ULTIMATE` | Special high-damage move |
| `PICKUP` | Accept a weapon drop from a defeated enemy |
| `SKIP` | Pass the turn |

---

## 📁 Project Structure

```
submission/
├── arbiter/                 # Arbiter process source
│   ├── arbiter.cpp          # Main: SHM init, fork/exec, scheduler entry
│   ├── scheduler.cpp        # Turn scheduling loop, combat resolution
│   ├── deadlock_monitor.cpp # Background DFS cycle detection thread
│   ├── artifact_table.cpp   # Artifact resource allocation/release
│   ├── arbiter_signals.cpp  # Signal handlers (SIGTERM, SIGCHLD)
│   ├── inventory.cpp        # Inventory management
│   ├── weapons.cpp          # Weapon definitions
│   └── utils.cpp            # Logging, timing utilities
│
├── hip/                     # Human Interface Process source
│   ├── hip.cpp              # Main: SHM attach, thread launch
│   ├── hud.cpp              # SFML HUD rendering (HP bars, logs, menus)
│   ├── input_handler.cpp    # Mouse/keyboard event processing
│   ├── player_thread.cpp    # Per-player pthread: reads intent, submits action
│   ├── render_thread.cpp    # Dedicated render pthread
│   ├── inventory.cpp
│   ├── weapons.cpp
│   └── utils.cpp
│
├── asp/                     # Autonomous/NPC Process source
│   ├── asp.cpp              # Main: SHM attach, spawn NPC threads
│   ├── npc_ai.cpp           # AI decision logic per enemy
│   ├── npc_thread.cpp       # Per-enemy pthread entry point
│   ├── inventory.cpp
│   ├── weapons.cpp
│   └── utils.cpp
│
├── include/                 # Shared headers (used by all three processes)
│   ├── shared_state.h       # The entire SharedState struct definition
│   ├── constants.h          # SHM key, window size, tick interval, etc.
│   └── roll_seed.h          # RNG seed helpers
│
├── Makefile                 # Builds all three binaries
├── Dockerfile               # Ubuntu 22.04 + SFML + build tools
└── requirements.txt         # Extra apt packages (libx11-dev)
```

---

## 🚀 Build & Run

### Prerequisites

**Ubuntu / Debian:**
```bash
sudo apt-get install build-essential libsfml-dev libx11-dev
```

**Arch Linux:**
```bash
sudo pacman -S sfml libx11
```

### Build
```bash
make
```
This produces three binaries: `arbiters`, `hips`, `asps`.

### Run
Simply launch the Arbiter — it automatically forks and execs the other two processes:
```bash
./arbiters
```
The SFML window will open. Use the on-screen menu to set the party size and game mode, then press **Begin**.

### Clean
```bash
make clean
```

---

## 🐳 Docker

A Dockerfile is included for a self-contained build environment:

```bash
docker build -t chrono-rift .
docker run -it --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix chrono-rift bash
# Inside container:
make && ./arbiters
```

> **Note:** Running a GUI from Docker requires an X11 display. On macOS use XQuartz; on Windows use VcXsrv or WSLg.

---

## ⚙️ Configuration

Key constants in `include/constants.h`:

| Constant | Default | Description |
|----------|---------|-------------|
| `SHM_KEY` | `0x52494654` | POSIX shared memory key |
| `WINDOW_W` / `WINDOW_H` | `1280 × 800` | SFML window dimensions |
| `TICK_INTERVAL_MS` | `50` | Scheduler tick rate (ms) |
| `DROP_CHANCE_PERCENT` | `70` | Weapon drop chance on enemy death |
| `MAX_PLAYERS` | `8` | Maximum concurrent human players |
| `MAX_ENEMIES` | `9` | Maximum simultaneous NPC enemies |

---

## 🔬 Key Implementation Details

**Shared memory layout:** The entire game world — all entities, inventories, semaphores, logs, and UI mailboxes — lives in a single `sizeof(SharedState)` chunk. No heap allocations cross process boundaries.

**Stamina-based scheduling:** Player speed is computed as `100 / num_players` so that more players means slower individual turns, keeping the game balanced.

**Wait-for graph:** `state->waits_for_resource[entity_id]` tracks which artifact resource (0, 1, or 2) each entity is waiting on. The deadlock monitor builds an adjacency list from this array and runs DFS every cycle.

**Atomic action handoff:** Player threads write into `input_buffer[player_id]` and set `pending_action.ready = true`, then signal `action_submitted`. The Arbiter's scheduler spins on this semaphore and consumes the action atomically under `state_lock`.

---
