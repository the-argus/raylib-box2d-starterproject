#include "player.h"
#include "input.h"

#include <raymath.h>

constexpr f32 minSpeed = 0.1f; // meters per second
constexpr f32 maxSpeed = 6.0f;
constexpr f32 stopSpeed = 3.0f;
constexpr f32 friction = 0.2f;
constexpr f32 airTurnControl = 0.2f;
constexpr f32 acceleration = 20.f;
constexpr f32 gravity = 30.f;
constexpr f32 jumpSpeed = 10.f;

constexpr b2::PhysicsQueryFilter groundCheckFilter = {
    .categoryBits = u64(CollisionLayer::Player),
    .maskBits = u64(CollisionLayer::World),
};

/// When we check the ground for collision we use a shapecast which accepts not
/// a shape but rather a ShapeProxy. which seems to just be a pointlist
/// definition of a shape
std::tuple<b2::ShapeProxy, b2::Vec2>
makeGroundCheckShapecastProxy(const b2::Capsule &capsule)
{
    using namespace b2;
    const f32 pogoRestLength = 3.0f * capsule.radius;
    const f32 rayLength = pogoRestLength + capsule.radius;

    ShapeProxy proxy = {};
    Vec2 translation = {};

    // for a point pogo:
    // proxy = b2MakeProxy( &b2Vec2_zero, 1, 0.0f );
    // translation = { 0.0f, -rayLength };

    // for a circle pogo:
    const Circle circle = {b2Vec2_zero, 0.5f * capsule.radius};
    proxy = b2MakeProxy(&b2Vec2_zero, 1, circle.radius);
    translation = {0.0f, -rayLength + circle.radius};

    // for a segment pogo:
    // const Vec2 segmentOffset = {0.75f * capsule.radius, 0.0f};
    // const Segment segment = {
    //     .point1 = -segmentOffset,
    //     .point2 = segmentOffset,
    // };
    // proxy = b2MakeProxy( &segment.point1, 2, 0.0f );
    // translation = { 0.0f, -rayLength };

    return {proxy, translation};
}

void Player::solveMove(GameContext *ctx, f32 deltaTime, f32 horizontalInput)
{
    using namespace b2;

    const f32 speed = b2Length(this->velocity);
    if (speed < minSpeed) {
        this->velocity = {};
    } else if (this->isOnGround) {
        const f32 control = std::max(stopSpeed, speed);
        const f32 drop = control * friction * deltaTime;
        const f32 newSpeed = std::max(0.f, speed - drop);
        this->velocity *= newSpeed / speed; // change magnitude of velocity
    }

    const Vec2 desiredVelocity{maxSpeed * horizontalInput, 0.f};
    const f32 desiredSpeed = std::min(maxSpeed, b2Length(desiredVelocity));
    const Vec2 desiredDirection = b2Normalize(desiredVelocity);

    if (this->isOnGround)
        this->velocity.y = 0.f;

    const f32 currentSpeed = b2Dot(this->velocity, desiredDirection);
    const f32 addSpeed = desiredSpeed - currentSpeed;

    if (addSpeed > 0.f) {
        const f32 steer = this->isOnGround ? 1.f : airTurnControl;
        const f32 accelSpeed =
            std::min(addSpeed, steer * acceleration * maxSpeed * deltaTime);

        this->velocity += accelSpeed * desiredDirection;
    }

    this->velocity.y -= gravity * deltaTime;

    const auto [groundCheckProxy, groundCheckProxyTranslation] =
        makeGroundCheckShapecastProxy(this->shape);

    bool shapeCastHit = false;

    ctx->world.castShape(
        World::ShapeCastOptions{
            .proxy = &groundCheckProxy,
            .origin = this->position + this->shape.center1,
            .translation = groundCheckProxyTranslation,
            .filter = groundCheckFilter,
        },
        [&](ShapeConst shape, Vec2 point, Vec2 normal, f32 fraction) -> f32 {
            shapeCastHit = true;
            return fraction;
        });

    // isOnGround causes us to snap velocity, so if we are moving up and not on
    // ground then we just left a jump but are still close to the ground / the
    // shapecast still hit, so ignore it
    const bool isFalling = this->velocity.y <= 0.01f;
    this->isOnGround = shapeCastHit && (this->isOnGround || isFalling);

    const Vec2 targetPosition = this->position + (deltaTime * this->velocity);

    constexpr PhysicsQueryFilter collideFilter = {
        .categoryBits = u64(CollisionLayer::Player),
        .maskBits = CollisionLayer::Player | CollisionLayer::World,
    };

    // dont hit other mover which gives soft collision
    constexpr PhysicsQueryFilter castFilter = {
        .categoryBits = u64(CollisionLayer::Player),
        .maskBits = u64(CollisionLayer::World),
    };

    u64 totalIteration = 0;
    u64 planeCount = 0;
    const f32 tolerance = 0.01f;
    std::array<b2::CollisionPlane, 8> collisionPlaneBuffer{};
    u64 collisionPlaneBufferSize = 0;

    for (u64 iteration = 0; iteration < 5; ++iteration) {
        planeCount = 0;

        auto iterator = collisionPlaneBuffer.begin();

        ctx->world.collideMover(
            this->shape, this->position, collideFilter,
            [&](b2::ShapeConst otherShape,
                const b2::PlaneResult *plane) -> bool {
                if (!plane->hit)
                    return true;
                if (iterator == std::end(collisionPlaneBuffer))
                    return false;

                // each shape could store a pushLimit and clipVelocity. just
                // assume everything is hard for now
                *iterator = b2::CollisionPlane{
                    .plane = plane->plane,
                    // rigid collision
                    .pushLimit = FLT_MAX,
                    // hard collision, so dont accumulate velocity
                    .clipVelocity = true,
                };
                ++iterator;

                return true;
            });
        collisionPlaneBufferSize =
            std::distance(collisionPlaneBuffer.begin(), iterator);

        b2PlaneSolverResult result = b2SolvePlanes(
            targetPosition - this->position, collisionPlaneBuffer.data(),
            collisionPlaneBufferSize);

        totalIteration += result.iterationCount;

        const f32 fraction = ctx->world.castMover(
            this->shape, this->position, result.translation, castFilter);

        const Vec2 delta = fraction * result.translation;
        this->position += delta;

        if (b2LengthSquared(delta) < tolerance * tolerance) {
            break;
        }
    }

    this->velocity = b2ClipVector(this->velocity, collisionPlaneBuffer.data(),
                                  collisionPlaneBufferSize);
}

void Player::update(GameContext *ctx, f32 deltaTime)
{
    using namespace b2;

    constexpr b2::PhysicsQueryFilter filter{
        .categoryBits = u64(CollisionLayer::Player),
        .maskBits = u64(CollisionLayer::World),
    };

    const auto horizontalInput = static_cast<f32>(
        isKeyDownInApp(ctx->keyRight) - isKeyDownInApp(ctx->keyLeft));

    this->solveMove(ctx, deltaTime, horizontalInput);

    if (this->isOnGround && isKeyJustPressedInApp(ctx->keyUp)) {
        this->velocity.y = jumpSpeed;
        this->isOnGround = false;
    }
}

void Player::draw(GameContext *ctx)
{
    Rectangle source{
        .x = 0,
        .y = 0,
        .width = static_cast<f32>(ctx->textureMissing.width),
        .height = static_cast<f32>(ctx->textureMissing.height),
    };

    Rectangle dest{
        .x = this->position.x - this->shape.radius,
        .y = this->position.y - this->shape.radius,
        // this is a square because the texture is but for now the actual
        // physics body is a capsule
        .width = this->shape.radius * 2.f,
        .height = this->shape.radius * 2.f,
    };

    DrawTexturePro(ctx->textureMissing, source, dest, {}, 0.f, WHITE);
}
