#pragma once
#include "../include/shared_state.h"
#include "hud.h"
#include <SFML/Graphics.hpp>

// Render thread prototype.
void* render_thread_main(void* arg);
