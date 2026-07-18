#ifndef __GAME_PLAYER_H__
#define __GAME_PLAYER_H__

#include "game_context.h"

struct Player
{
    b2::Body body;
    b2::Capsule shape; // not registered in engine, just used for shapecasts
    b2::Vec2 velocity; // we move the physics body ourselves, its vel is 0
    bool isOnGround = false;

    void update(GameContext *ctx, f32 deltaTime);
    void draw(GameContext *ctx);

  private:
    void solveMove(GameContext *ctx, f32 deltaTime, f32 maxSpeedThrottle);
};

#endif
