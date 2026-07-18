#include "player.h"
#include "convert.h"
#include "input.h"

#include <raymath.h>

constexpr f32 minSpeed = 0.1f; // meters per second
constexpr f32 maxSpeed = 6.0f;
constexpr f32 stopSpeed = 3.0f;
constexpr f32 friction = 0.2f;
constexpr f32 airTurnControl = 0.2f;
constexpr f32 acceleration = 20.f;
constexpr f32 gravity = 30.f;

void Player::solveMove(GameContext *ctx, f32 deltaTime, f32 maxSpeedThrottle)
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

    const Vec2 desiredVelocity{maxSpeed * maxSpeedThrottle, 0.f};
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

    const Vec2 origin = this->body.position() + this->shape.center1;

    ctx->world.castShape({});
}

void Player::update(GameContext *ctx, f32 deltaTime)
{
    using namespace b2;

    const auto start = this->body.position();

    constexpr b2::PhysicsQueryFilter filter{
        .categoryBits = u64(CollisionLayer::Player),
        .maskBits = u64(CollisionLayer::World),
    };

    const Vector2 inputVec{
        .x = static_cast<f32>(isKeyPressedInApp(ctx->keyRight) -
                              isKeyPressedInApp(ctx->keyLeft)),
        .y = static_cast<f32>(isKeyPressedInApp(ctx->keyDown) -
                              isKeyPressedInApp(ctx->keyUp)),
    };

    bool onGround = false;
    const auto raycastCallback = [&](ShapeConst shape, Vec2 position,
                                     Vec2 normal,
                                     f32 hitFraction) { return 1.f; };

    // checking if on ground
    ctx->world.castRay(
        b2::World::RayCastOptions{
            .origin = start,
            .translation = Vec2{.x = 0, .y = 1.f},
            .filter = filter,
        },
        raycastCallback);

    const auto delta = this->body.linearVelocity();
    const f32 fraction =
        ctx->world.castMover(this->shape, start, delta, filter);

    std::array<b2::CollisionPlane, 128> collisionPlaneBuffer;
    auto iterator = collisionPlaneBuffer.begin();

    auto callback = [&](b2::ShapeConst otherShape,
                        const b2::PlaneResult *plane) -> bool {
        if (!plane->hit)
            return true;
        if (iterator == std::end(collisionPlaneBuffer))
            return false;

        *iterator = b2::CollisionPlane{
            .plane = plane->plane,
            .pushLimit = FLT_MAX, // rigid collision
            .clipVelocity = true, // hard collision, so dont accumulate velocity
        };
        ++iterator;

        return true;
    };
    ctx->world.collideMover(this->shape, start, filter, callback);

    const auto numPlanes =
        std::distance(iterator, collisionPlaneBuffer.begin());
    b2PlaneSolverResult result =
        b2SolvePlanes(delta, collisionPlaneBuffer.data(), numPlanes);

    this->body.setTransform(conv(conv(start) + conv(result.translation)),
                            this->body.rotation());

    // clip velocity to not build up when moving into walls
    const auto velocity = this->body.linearVelocity();
    this->body.setLinearVelocity(
        b2ClipVector(velocity, collisionPlaneBuffer.data(), numPlanes));
}

// clang-format off
/*
	// https://github.com/id-Software/Quake/blob/master/QW/client/pmove.c#L390
	void SolveMove( float timeStep, float throttle )
	{
		// Friction
		float speed = b2Length( m_velocity );
		if ( speed < m_minSpeed )
		{
			m_velocity.x = 0.0f;
			m_velocity.y = 0.0f;
		}
		else if ( m_onGround )
		{
			// Linear damping above stopSpeed and fixed reduction below stopSpeed
			float control = speed < m_stopSpeed ? m_stopSpeed : speed;

			// friction has units of 1/time
			float drop = control * m_friction * timeStep;
			float newSpeed = b2MaxFloat( 0.0f, speed - drop );
			m_velocity *= newSpeed / speed;
		}

		b2Vec2 desiredVelocity = { m_maxSpeed * throttle, 0.0f };
		float desiredSpeed;
		b2Vec2 desiredDirection = b2GetLengthAndNormalize( &desiredSpeed, desiredVelocity );

		if ( desiredSpeed > m_maxSpeed )
		{
			desiredSpeed = m_maxSpeed;
		}

		if ( m_onGround )
		{
			m_velocity.y = 0.0f;
		}

		// Accelerate
		float currentSpeed = b2Dot( m_velocity, desiredDirection );
		float addSpeed = desiredSpeed - currentSpeed;
		if ( addSpeed > 0.0f )
		{
			float steer = m_onGround ? 1.0f : m_airSteer;
			float accelSpeed = steer * m_accelerate * m_maxSpeed * timeStep;
			if ( accelSpeed > addSpeed )
			{
				accelSpeed = addSpeed;
			}

			m_velocity += accelSpeed * desiredDirection;
		}

		m_velocity.y -= m_gravity * timeStep;

		float pogoRestLength = 3.0f * m_capsule.radius;
		float rayLength = pogoRestLength + m_capsule.radius;
		b2Circle circle = { b2Vec2_zero, 0.5f * m_capsule.radius };
		b2Vec2 segmentOffset = { 0.75f * m_capsule.radius, 0.0f };
		b2Segment segment = {
			.point1 = -segmentOffset,
			.point2 = segmentOffset,
		};

		b2ShapeProxy proxy = {};
		b2Vec2 translation;
		b2QueryFilter pogoFilter = { MoverBit, StaticBit | DynamicBit };
		CastResult castResult = {};

		if ( m_pogoShape == PogoPoint )
		{
			proxy = b2MakeProxy( &b2Vec2_zero, 1, 0.0f );
			translation = { 0.0f, -rayLength };
		}
		else if ( m_pogoShape == PogoCircle )
		{
			proxy = b2MakeProxy( &b2Vec2_zero, 1, circle.radius );
			translation = { 0.0f, -rayLength + circle.radius };
		}
		else
		{
			proxy = b2MakeProxy( &segment.point1, 2, 0.0f );
			translation = { 0.0f, -rayLength };
		}

		b2Pos origin = m_position + m_capsule.center1;
		b2World_CastShape( m_worldId, origin, &proxy, translation, pogoFilter, CastCallback, &castResult );

		// Avoid snapping to ground if still going up
		if ( m_onGround == false )
		{
			m_onGround = castResult.hit && m_velocity.y <= 0.01f;
		}
		else
		{
			m_onGround = castResult.hit;
		}

		if ( castResult.hit == false )
		{
			m_pogoVelocity = 0.0f;

			b2Vec2 delta = translation;
			DrawLine( m_draw, origin, origin + delta, b2_colorGray );

			if ( m_pogoShape == PogoPoint )
			{
				DrawPoint( m_draw, origin + delta, 10.0f, b2_colorGray );
			}
			else if ( m_pogoShape == PogoCircle )
			{
				DrawCircle( m_draw, origin + delta, circle.radius, b2_colorGray );
			}
			else
			{
				DrawLine( m_draw, origin + segment.point1 + delta, origin + segment.point2 + delta, b2_colorGray );
			}
		}
		else
		{
			float pogoCurrentLength = castResult.fraction * rayLength;

			float offset = pogoCurrentLength - pogoRestLength;
			m_pogoVelocity = b2SpringDamper( m_pogoHertz, m_pogoDampingRatio, offset, m_pogoVelocity, timeStep );

			b2Vec2 delta = castResult.fraction * translation;
			DrawLine( m_draw, origin, origin + delta, b2_colorGray );

			if ( m_pogoShape == PogoPoint )
			{
				DrawPoint( m_draw, origin + delta, 10.0f, b2_colorPlum );
			}
			else if ( m_pogoShape == PogoCircle )
			{
				DrawCircle( m_draw, origin + delta, circle.radius, b2_colorPlum );
			}
			else
			{
				DrawLine( m_draw, origin + segment.point1 + delta, origin + segment.point2 + delta, b2_colorPlum );
			}

			b2Body_ApplyForce( castResult.bodyId, { 0.0f, -50.0f }, castResult.point, true );
		}

		DrawTransform( m_draw, { m_position, b2Rot_identity }, 0.25f );

		b2Pos target = m_position + timeStep * m_velocity + timeStep * m_pogoVelocity * b2Vec2{ 0.0f, 1.0f };

		// Mover overlap filter
		b2QueryFilter collideFilter = { MoverBit, StaticBit | DynamicBit | MoverBit };

		// Movers don't sweep against other movers, allows for soft collision
		b2QueryFilter castFilter = { MoverBit, StaticBit | DynamicBit };

		m_totalIterations = 0;
		float tolerance = 0.01f;

		for ( int iteration = 0; iteration < 5; ++iteration )
		{
			m_planeCount = 0;

			b2Capsule mover = m_capsule;

			b2World_CollideMover( m_worldId, m_position, &mover, collideFilter, PlaneResultFcn, this );
			b2PlaneSolverResult result = b2SolvePlanes( target - m_position, m_planes, m_planeCount );

			m_totalIterations += result.iterationCount;

			float fraction = b2World_CastMover( m_worldId, m_position, &mover, result.translation, castFilter );

			b2Vec2 delta = fraction * result.translation;
			m_position = m_position + delta;

			if ( b2LengthSquared( delta ) < tolerance * tolerance )
			{
				break;
			}
		}

		m_velocity = b2ClipVector( m_velocity, m_planes, m_planeCount );
	}
*/
// clang-format on

void Player::draw(GameContext *ctx)
{
    Rectangle source{
        .x = 0,
        .y = 0,
        .width = static_cast<f32>(ctx->textureMissing.width),
        .height = static_cast<f32>(ctx->textureMissing.height),
    };

    const auto bb = toRect(this->body.computeAABB());

    DrawTexturePro(ctx->textureMissing, source, bb, {}, 0.f, WHITE);
}
