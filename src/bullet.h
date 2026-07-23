#ifndef __GAME_BULLET_H__
#define __GAME_BULLET_H__

#include "box2d.h"
#include "game_context.h"
#include "pool.h"

struct Bullet
{
    Bullet(b2::World world, b2::Vec2 position, b2::Vec2 velocity);

    b2::Body body;

    f32 lifetimer{};

    void update(GameContext *ctx, f32 deltaTime);
    void draw();
};

using BulletHandle = Pool<Bullet>::Handle;

#endif
