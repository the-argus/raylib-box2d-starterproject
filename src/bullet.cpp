#include "bullet.h"

#include <raylib.h>

Bullet::Bullet(b2::World world, b2::Vec2 position, b2::Vec2 velocity)
{
    BulletHandle selfHandle = Pool<Bullet>::instance()->handleForItem(*this);
    uassert(selfHandle, "Bullets should only be created within the bullet "
                        "pool, via makeBullet()");

    auto def = b2::Body::defaultDefinition();
    def.isBullet = true;
    def.position = position;
    def.linearVelocity = velocity;
    def.type = static_cast<b2BodyType>(b2::BodyType::Dynamic);

    // TODO: find a good way to do userdata so the body can point back to this
    // bullet. Could just put index/generation into userdata, but on a browser
    // build the void* is 32 bits and so they won't both fit...
    // (maybe modify box2d to use uint64_t for userdata instead of void* tbh...)

    this->body = world.createBody(&def);
}

void Bullet::draw()
{
    DrawRectangle(body.position().x, body.position().y, 1, 1, RED);
}
