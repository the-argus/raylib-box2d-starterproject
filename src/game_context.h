#ifndef __GAME_GAME_CONTEXT_H__
#define __GAME_GAME_CONTEXT_H__

#include "arena.h"
#include "box2d.h"

#include <raylib.h>

struct GameContext
{
    Arena frameAllocator;

    b2::World world;
    b2::Body floor;
    b2::Body square;
    b2::Vec2U32 windowSize{}; // changes on events

    f32 lastZoomTime{};
    f32 cameraDragSpeed = 1.f;
    bool trackingPlayer = true;
    Camera2D camera{.zoom = 1.f};

    Texture textureMissing{};

    int keyLeft = KEY_A;
    int keyRight = KEY_D;
    int keyUp = KEY_W;
    int keyDown = KEY_S;

    GameContext(const GameContext &) = delete;
    GameContext &operator=(const GameContext &) = delete;
    GameContext(GameContext &&) = delete;
    GameContext &operator=(GameContext &&) = delete;

    ~GameContext() { UnloadTexture(textureMissing); }
};

#endif
