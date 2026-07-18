#ifndef __GAME_PLAYER_H__
#define __GAME_PLAYER_H__

#include "game_context.h"

struct Player
{
    // there is no body/shape id in player because it moves itself, it observes
    // the physics engine to figure out what to collide with but otherwise is
    // not involved with it
    b2::Capsule shape{{0.0f, -0.5f}, {0.0f, 0.5f}, 0.3f};
    b2::Vec2 velocity{};
    b2::Vec2 position{};
    bool isOnGround = false;

    void update(GameContext *ctx, f32 deltaTime);
    void draw(GameContext *ctx);

  private:
    void solveMove(GameContext *ctx, f32 deltaTime, f32 maxSpeedThrottle);
};

#endif
