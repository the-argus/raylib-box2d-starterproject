#ifndef __GAME_CONVERT_H__
#define __GAME_CONVERT_H__

#include <box2d/box2d.h>
#include <raylib.h>

constexpr Vector2 conv(b2Vec2 vec) { return {vec.x, vec.y}; }
constexpr b2Vec2 conv(Vector2 vec) { return {vec.x, vec.y}; }

struct BoundingBox2D
{
    Vector2 min;
    Vector2 max;
};

constexpr BoundingBox2D conv(b2AABB bb)
{
    return BoundingBox2D{.min = conv(bb.lowerBound),
                         .max = conv(bb.upperBound)};
}

constexpr b2AABB conv(BoundingBox2D bb)
{
    return b2AABB{.lowerBound = conv(bb.min), .upperBound = conv(bb.max)};
}

constexpr Rectangle toRect(b2AABB bb)
{
    return Rectangle{
        .x = bb.lowerBound.x,
        .y = bb.lowerBound.y,
        .width = bb.upperBound.x - bb.lowerBound.x,
        .height = bb.upperBound.y - bb.lowerBound.y,
    };
}

constexpr Rectangle toRect(BoundingBox2D bb)
{
    return Rectangle{
        .x = bb.min.x,
        .y = bb.min.y,
        .width = bb.max.x - bb.min.x,
        .height = bb.max.y - bb.min.y,
    };
}

#endif
