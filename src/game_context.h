#ifndef __GAME_GAME_CONTEXT_H__
#define __GAME_GAME_CONTEXT_H__

#include "allocator.h"
#include "arena.h"
#include "box2d.h"

#include <raylib.h>

struct Player;

struct GameContext
{
    // only allocate stuff here if it lives until the end of the game
    Arena gameAllocator{cAllocator()};
    // everything in this allocator is freed at the end of the frame
    Allocator *frameAllocator = nullptr;

    Player *player = nullptr;

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

    // GameContext(const GameContext &) = delete;
    // GameContext &operator=(const GameContext &) = delete;
    // GameContext(GameContext &&) = delete;
    // GameContext &operator=(GameContext &&) = delete;

    ~GameContext() { UnloadTexture(textureMissing); }
};

#endif
