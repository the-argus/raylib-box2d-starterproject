#ifndef __GAME_BULLET_H__
#define __GAME_BULLET_H__

#include "box2d.h"
#include "pool.h"

struct Bullet
{
    Bullet(b2::World world, b2::Vec2 position, b2::Vec2 velocity);

    b2::Body body;

    void draw();
};

using BulletHandle = Pool<Bullet>::Handle;

// a function that is like std::make_unique where it takes
// constructor arguments and returns something pointer-like (a handle) to a
// bullet.
template <typename... ConstructorArgs>
    requires std::is_constructible_v<Bullet, ConstructorArgs...>
BulletHandle makeBullet(ConstructorArgs &&...args)
{
    return Pool<Bullet>::instance()->make(
        std::forward<ConstructorArgs>(args)...);
}

#endif
