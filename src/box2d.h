#ifndef __UHANDERS_BOX2D_H__
#define __UHANDERS_BOX2D_H__

#include "allocator.h"
#include "macros.h"
#include "result.h"

#include <box2d/box2d.h>

#include <cstring>
#include <span>

enum class CollisionLayer : u64
{
    // clang-format off
    World  = 0b0000001,
    Player = 0b0000010,
    // clang-format on
};

constexpr u64 operator|(CollisionLayer lhs, CollisionLayer rhs)
{
    return static_cast<u64>(lhs) | static_cast<u64>(rhs);
}
constexpr u64 operator|(u64 lhs, CollisionLayer rhs)
{
    return lhs | static_cast<u64>(rhs);
}
constexpr u64 operator|(CollisionLayer lhs, u64 rhs)
{
    return static_cast<u64>(lhs) | rhs;
}
constexpr u64 operator&(CollisionLayer lhs, CollisionLayer rhs)
{
    return static_cast<u64>(lhs) & static_cast<u64>(rhs);
}
constexpr u64 operator&(u64 lhs, CollisionLayer rhs)
{
    return lhs & static_cast<u64>(rhs);
}
constexpr u64 operator&(CollisionLayer lhs, u64 rhs)
{
    return static_cast<u64>(lhs) & rhs;
}

namespace b2 {
using Vec2 = b2Vec2;
struct Vec2I32
{
    i32 x = 0;
    i32 y = 0;
};
struct Vec2U32
{
    u32 x = 0;
    u32 y = 0;
};
using AABB = b2AABB;
using MassData = b2MassData;
using Rotation = b2Rot;
using Transform = b2Transform;

enum class BodyType
{
    /// zero mass, zero velocity, may be manually moved
    Static = 0,
    /// zero mass, velocity set by user, moved by solver
    Kinematic = 1,
    /// positive mass, velocity determined by forces, moved by solver
    Dynamic = 2
};

/// Made to align with b2ShapeType
enum class ShapeType
{
    /// A circle with an offset
    Circle,
    /// A capsule is an extruded circle
    Capsule,
    /// A line segment
    Segment,
    /// A convex polygon
    Polygon,
    /// A line segment owned by a chain shape
    ChainSegment,
};

using MotionLocks = b2MotionLocks;
using Circle = b2Circle;
using Capsule = b2Capsule;
using Segment = b2Segment;
using ChainSegment = b2ChainSegment;
using Polygon = b2Polygon;
using ShapeDef = b2ShapeDef;
using BodyDef = b2BodyDef;
using WorldDef = b2WorldDef;
using ContactData = b2ContactData;
using PlaneResult = b2PlaneResult;
using CollisionPlane = b2CollisionPlane;
using PhysicsSurfaceMaterial = b2SurfaceMaterial;
using PhysicsFilter = b2Filter;
using PhysicsQueryFilter = b2QueryFilter;
using PhysicsDirectShapeRaycastOutput = b2CastOutput;
using PhysicsRaycastInput = b2RayCastInput;
using PhysicsFrictionCallback = b2FrictionCallback;
using PhysicsRestitutionCallback = b2RestitutionCallback;
using PhysicsRaycastResultFunction = b2CastResultFcn;
using PhysicsPlaneResultFunction = b2PlaneResultFcn;
using PhysicsTreeStats = b2TreeStats;
struct PhysicsContactTuningOptions
{
    float hertz;
    float dampingRatio;
    float pushSpeed;
};

template <bool isConst> class WorldImpl;
template <bool isConst> class BodyImpl;
template <bool isConst> class ShapeImpl;

template <bool isConst> struct PhysicsRaycastResult
{
    ShapeImpl<isConst> shapeId;
    Vec2 point = {};
    Vec2 normal = {};
    f32 fraction = 1.0;
    bool hit = false;
};

[[nodiscard]] inline bool isValid(b2JointId joint)
{
    return b2Joint_IsValid(joint);
}

[[nodiscard]] inline bool isValid(b2BodyId body)
{
    return b2Body_IsValid(body);
}

[[nodiscard]] inline bool isValid(b2ShapeId shape)
{
    return b2Shape_IsValid(shape);
}

template <typename T>
    requires std::is_trivial_v<T>
[[nodiscard]] inline bool isDefault(const T &item)
{
    static const T defaulted{};
    if constexpr (requires(const T &a, const T &b) {
                      { a == b } -> std::same_as<bool>;
                  }) {
        return defaulted == item;
    } else {
        return std::memcmp(static_cast<const u8 *>(std::addressof(item)),
                           static_cast<const u8 *>(std::addressof(defaulted)),
                           sizeof(T)) == 0;
    }
}

namespace detail {
template <typename T, typename IDType, typename GetCapacityCallable,
          typename GetBufferCallable>
[[nodiscard]] Result<std::span<T>, alloc::Error>
getDataBufferHelper(Allocator *allocator, IDType id,
                    const GetCapacityCallable &getCapacity,
                    const GetBufferCallable &getBuffer)
{
    const u32 needed = getCapacity(id);
    const auto buffer = allocator->allocate(alloc::Request{
        .numBytes = sizeof(T) * needed,
        .alignment = alignof(T),
    });
    if (not isSuccess(buffer)) [[unlikely]]
        return buffer.error();

    auto *const start = reinterpret_cast<T *>(buffer.value().data());
    uassert(buffer.value().size_bytes() >= sizeof(T) * needed, "box2d bug");

    const int used = getBuffer(id, start, needed);
    uassert(needed == static_cast<u32>(used), "box2d bug");

    return std::span<T>(start, needed);
}

[[nodiscard]] inline int sensorGetOverlappingShapesUnsafe(b2ShapeId id,
                                                          b2ShapeId *visitorIds,
                                                          u32 capacity) NOEXCEPT
{
    return b2Shape_GetSensorData(id, visitorIds, static_cast<int>(capacity));
}

[[nodiscard]] inline u32
shapeSensorGetNumOverlappingShapes(b2ShapeId sensor) NOEXCEPT
{
    return static_cast<u32>(b2Shape_GetSensorCapacity(sensor));
}

[[nodiscard]] inline int shapeGetContactDataUnsafe(b2ShapeId id,
                                                   ContactData *contactData,
                                                   u32 capacity) NOEXCEPT
{
    return b2Shape_GetContactData(id, contactData, static_cast<int>(capacity));
}

[[nodiscard]] inline u32 shapeGetContactCapacity(b2ShapeId id) NOEXCEPT
{
    return static_cast<u32>(b2Shape_GetContactCapacity(id));
}

/// Get the touching contact data for a body.
/// @note Box2D uses speculative collision so some contact points may be
/// separated.
/// @returns the number of elements filled in the provided array
/// @warning do not ignore the return value, it specifies the valid number of
/// elements
[[nodiscard]] inline int bodyGetContactDataUnsafe(b2BodyId bodyId,
                                                  ContactData *contactData,
                                                  u32 capacity) NOEXCEPT
{
    return b2Body_GetContactData(bodyId, contactData,
                                 static_cast<int>(capacity));
}

/// Get the maximum capacity required for retrieving all the touching contacts
/// on a body
[[nodiscard]] inline u32 bodyGetContactCapacity(b2BodyId bodyId) NOEXCEPT
{
    return static_cast<u32>(b2Body_GetContactCapacity(bodyId));
}

[[nodiscard]] inline u32 bodyGetJointCount(b2BodyId bodyId) NOEXCEPT
{
    return static_cast<u32>(b2Body_GetJointCount(bodyId));
}

[[nodiscard]] inline int bodyGetJointsUnsafe(b2BodyId bodyId,
                                             b2JointId *jointIdBuffer,
                                             u32 capacity) NOEXCEPT
{
    return b2Body_GetJoints(bodyId, jointIdBuffer, static_cast<int>(capacity));
}

[[nodiscard]] inline u32 bodyGetShapeCount(b2BodyId bodyId) NOEXCEPT
{
    return static_cast<u32>(b2Body_GetShapeCount(bodyId));
}

[[nodiscard]] inline int bodyGetShapesUnsafe(b2BodyId bodyId,
                                             b2ShapeId *shapeIdBuffer,
                                             u32 capacity) NOEXCEPT
{
    return b2Body_GetShapes(bodyId, shapeIdBuffer, static_cast<int>(capacity));
}

template <bool isConst>
constexpr auto castShapeIDToShape = [](std::span<b2ShapeId> shapeIds) {
    using T = ShapeImpl<isConst>;
    return std::span<T>(std::launder(reinterpret_cast<T *>(shapeIds.data())),
                        shapeIds.size_bytes() / sizeof(T));
};
} // namespace detail

template <bool isConst> class WorldImpl;

template <bool isConst> class ShapeImpl
{
  private:
    b2ShapeId id;

    ShapeImpl(b2ShapeId _id) NOEXCEPT : id(_id) {}

  public:
    template <bool bodyConst> friend class BodyImpl;
    friend class ShapeImpl<not isConst>;
    template <bool worldConst> friend class WorldImpl;

    [[nodiscard]] static ShapeDef defaultDefinition()
    {
        return b2DefaultShapeDef();
    }

    ShapeImpl() = delete;

    constexpr operator b2ShapeId() const NOEXCEPT { return id; }

    constexpr operator ShapeImpl<true>() const NOEXCEPT
        requires(not isConst)
    {
        return ShapeImpl<true>(id);
    }

    void destroy(bool updateBodyMass = true) const NOEXCEPT
        requires(not isConst)
    {
        b2DestroyShape(id, updateBodyMass);
    }

    [[nodiscard]] bool isValid() const NOEXCEPT { return b2Shape_IsValid(id); }
    explicit operator bool() const NOEXCEPT { return isValid(); }

    [[nodiscard]] ShapeType type() const NOEXCEPT
    {
        return static_cast<ShapeType>(b2Shape_GetType(id));
    }

    [[nodiscard]] BodyImpl<isConst> body() const NOEXCEPT
    {
        return b2Shape_GetBody(id);
    }

    [[nodiscard]] WorldImpl<isConst> world() const NOEXCEPT
    {
        return b2Shape_GetWorld(id);
    }

    [[nodiscard]] bool isSensor() const NOEXCEPT
    {
        return b2Shape_IsSensor(id);
    }

    ShapeImpl setUserData(void *newUserData) const NOEXCEPT
        requires(not isConst)
    {
        b2Shape_SetUserData(id, newUserData);
        return *this;
    }

    [[nodiscard]] void *userData() const NOEXCEPT
    {
        return b2Shape_GetUserData(id);
    }

    ShapeImpl setDensity(f32 newDensity,
                         bool updateBodyMass = true) const NOEXCEPT
        requires(not isConst)
    {
        b2Shape_SetDensity(id, newDensity, updateBodyMass);
        return *this;
    }

    [[nodiscard]] f32 density() const NOEXCEPT
    {
        return b2Shape_GetDensity(id);
    }

    ShapeImpl setFriction(f32 newFriction) const NOEXCEPT
        requires(not isConst)
    {
        b2Shape_SetFriction(id, newFriction);
        return *this;
    }

    [[nodiscard]] f32 friction() const NOEXCEPT
    {
        return b2Shape_GetFriction(id);
    }

    ShapeImpl setRestitution(f32 newRestitution) const NOEXCEPT
        requires(not isConst)
    {
        b2Shape_SetRestitution(id, newRestitution);
        return *this;
    }

    [[nodiscard]] f32 restitution() const NOEXCEPT
    {
        return b2Shape_GetRestitution(id);
    }

    ShapeImpl setMaterial(i32 newMaterial) const NOEXCEPT
        requires(not isConst)
    {
        b2Shape_SetUserMaterial(id, newMaterial);
        return *this;
    }

    [[nodiscard]] i32 material() const NOEXCEPT
    {
        return b2Shape_GetUserMaterial(id);
    }

    ShapeImpl
    setSurfaceMaterial(const PhysicsSurfaceMaterial &newMaterial) const NOEXCEPT
        requires(not isConst)
    {
        b2Shape_SetSurfaceMaterial(id, &newMaterial);
        return *this;
    }

    [[nodiscard]] PhysicsSurfaceMaterial surfaceMaterial() const NOEXCEPT
    {
        return b2Shape_GetSurfaceMaterial(id);
    }

    ShapeImpl setFilter(PhysicsFilter newFilter) const NOEXCEPT
        requires(not isConst)
    {
        b2Shape_SetFilter(id, newFilter);
        return *this;
    }

    [[nodiscard]] PhysicsFilter filter() const NOEXCEPT
    {
        return b2Shape_GetFilter(id);
    }

    ShapeImpl setSensorEventsAreEnabled(bool newAreEnabled) const NOEXCEPT
        requires(not isConst)
    {
        b2Shape_EnableSensorEvents(id, newAreEnabled);
        return *this;
    }

    [[nodiscard]] bool areSensorEventsEnabled() const NOEXCEPT
    {
        return b2Shape_AreSensorEventsEnabled(id);
    }

    ShapeImpl setContactEventsAreEnabled(bool newAreEnabled) const NOEXCEPT
        requires(not isConst)
    {
        b2Shape_EnableContactEvents(id, newAreEnabled);
        return *this;
    }

    [[nodiscard]] bool areContactEventsEnabled() const NOEXCEPT
    {
        return b2Shape_AreContactEventsEnabled(id);
    }

    ShapeImpl setPreSolveEventsAreEnabled(bool newAreEnabled) const NOEXCEPT
        requires(not isConst)
    {
        b2Shape_EnablePreSolveEvents(id, newAreEnabled);
        return *this;
    }

    [[nodiscard]] bool arePreSolveEventsEnabled() const NOEXCEPT
    {
        return b2Shape_ArePreSolveEventsEnabled(id);
    }

    ShapeImpl setHitEventsAreEnabled(bool newAreEnabled) const NOEXCEPT
        requires(not isConst)
    {
        b2Shape_EnableHitEvents(id, newAreEnabled);
        return *this;
    }

    [[nodiscard]] bool areHitEventsEnabled() const NOEXCEPT
    {
        return b2Shape_AreHitEventsEnabled(id);
    }

    [[nodiscard]] PhysicsDirectShapeRaycastOutput
    raycast(const PhysicsRaycastInput &input) NOEXCEPT
    {
        return b2Shape_RayCast(id, input.origin, input.translation);
    }

    ShapeImpl setCircle(const Circle &circle) const NOEXCEPT
        requires(not isConst)
    {
        b2Shape_SetCircle(id, &circle);
        return *this;
    }

    Circle circle() const NOEXCEPT
    {
        uassert(type() == ShapeType::Circle);
        return b2Shape_GetCircle(id);
    }

    ShapeImpl setCapsule(const Capsule &capsule) const NOEXCEPT
        requires(not isConst)
    {
        b2Shape_SetCapsule(id, &capsule);
        return *this;
    }

    Capsule capsule() const NOEXCEPT
    {
        uassert(type() == ShapeType::Capsule);
        return b2Shape_GetCapsule(id);
    }

    ShapeImpl setSegment(const Segment &segment) const NOEXCEPT
        requires(not isConst)
    {
        b2Shape_SetSegment(id, &segment);
        return *this;
    }

    Segment segment() const NOEXCEPT
    {
        uassert(type() == ShapeType::Segment);
        return b2Shape_GetSegment(id);
    }

    ShapeImpl setPolygon(const Polygon &polygon) const NOEXCEPT
        requires(not isConst)
    {
        b2Shape_SetPolygon(id, &polygon);
        return *this;
    }

    Polygon polygon() const NOEXCEPT
    {
        uassert(type() == ShapeType::Polygon);
        return b2Shape_GetPolygon(id);
    }

    [[nodiscard]] b2ChainId parentChain() const NOEXCEPT
    {
        return b2Shape_GetParentChain(id);
    }

    [[nodiscard]] AABB aabb() const NOEXCEPT { return b2Shape_GetAABB(id); }

    [[nodiscard]] MassData computeMassData() const NOEXCEPT
    {
        return b2Shape_ComputeMassData(id);
    }

    [[nodiscard]] Vec2 closestPointTo(Vec2 worldPoint) NOEXCEPT
    {
        return b2Shape_GetClosestPoint(id, worldPoint);
    }

    [[nodiscard]] Result<std::span<ShapeImpl<isConst>>, alloc::Error>
    overlappingShapes(Allocator *allocator) const NOEXCEPT
    {
        static_assert(sizeof(b2ShapeId) == sizeof(ShapeImpl));
        static_assert(alignof(b2ShapeId) == alignof(ShapeImpl));

        return detail::getDataBufferHelper<b2ShapeId>(
                   allocator, id, detail::shapeSensorGetNumOverlappingShapes,
                   detail::sensorGetOverlappingShapesUnsafe)
            .map(detail::castShapeIDToShape<isConst>);
    }

    [[nodiscard]] Result<std::span<ContactData>, alloc::Error>
    contactData(Allocator *allocator) const NOEXCEPT
    {
        return detail::getDataBufferHelper<ContactData>(
            allocator, id, detail::shapeGetContactCapacity,
            detail::shapeGetContactDataUnsafe);
    }
};

using Shape = ShapeImpl<false>;
using ShapeConst = ShapeImpl<true>;

template <bool isConst> class BodyImpl
{
  private:
    b2BodyId id;

  private:
    constexpr explicit BodyImpl(b2BodyId id) NOEXCEPT : id(id) {}

  public:
    [[nodiscard]] static BodyDef defaultDefinition()
    {
        return b2DefaultBodyDef();
    }

    friend class BodyImpl<not isConst>;
    template <bool worldConst> friend class WorldImpl;
    template <bool shapeConst> friend class ShapeImpl;

    BodyImpl() = delete;

    constexpr operator b2BodyId() const NOEXCEPT { return id; }

    /// Allow conversion from non-const to const
    constexpr operator BodyImpl<true>() const NOEXCEPT
        requires(not isConst)
    {
        return BodyImpl<true>(id);
    }

    void destroy() const NOEXCEPT
        requires(not isConst)
    {
        b2DestroyBody(id);
    }

    /// Def passed as pointer here to remind that it should not be passed
    /// inline but instead should be initialized with Shape::defaultDefinition()
    Shape addSegmentShape(const ShapeDef *def, const Segment &segment) NOEXCEPT
    {
        if (!def) {
            const auto defaultDef = Shape::defaultDefinition();
            return b2CreateSegmentShape(id, &defaultDef, &segment);
        }
        return b2CreateSegmentShape(id, def, &segment);
    }

    /// Def passed as pointer here to remind that it should not be passed
    /// inline but instead should be initialized with Shape::defaultDefinition()
    Shape addCapsuleShape(const ShapeDef *def, const Capsule &capsule) NOEXCEPT
    {
        if (!def) {
            const auto defaultDef = Shape::defaultDefinition();
            return b2CreateCapsuleShape(id, &defaultDef, &capsule);
        }
        return b2CreateCapsuleShape(id, def, &capsule);
    }

    /// Def passed as pointer here to remind that it should not be passed
    /// inline but instead should be initialized with Shape::defaultDefinition()
    Shape addCircleShape(const ShapeDef *def, const Circle &circle) NOEXCEPT
    {
        if (!def) {
            const auto defaultDef = Shape::defaultDefinition();
            return b2CreateCircleShape(id, &defaultDef, &circle);
        }
        return b2CreateCircleShape(id, def, &circle);
    }

    Shape addPolygonShape(const ShapeDef *def, const Polygon &polygon) NOEXCEPT
    {
        if (!def) {
            const auto defaultDef = Shape::defaultDefinition();
            return b2CreatePolygonShape(id, &defaultDef, &polygon);
        }
        return b2CreatePolygonShape(id, def, &polygon);
    }

    /// Apply an angular impulse. The impulse is ignored if the body is not
    /// awake. This optionally wakes the body.
    /// @param bodyId The body id
    /// @param impulse the angular impulse, usually in units of kg*m*m/s
    /// @param wake also wake up the body
    /// @warning This should be used for one-shot impulses. If you need a steady
    /// force, use a force instead, which will work better with the sub-stepping
    /// solver.
    BodyImpl applyAngularImpulse(f32 impulse, bool wake = true) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_ApplyAngularImpulse(id, impulse, wake);
        return *this;
    }

    /// Apply a force at a world point. If the force is not applied at the
    /// center of mass, it will generate a torque and affect the angular
    /// velocity. This optionally wakes up the body. The force is ignored if the
    /// body is not awake.
    /// @param bodyId The body id
    /// @param force The world force vector, usually in newtons (N)
    /// @param point The world position of the point of application. If not
    /// provided, then the center of the body is used.
    /// @param wake Option to wake up the body
    BodyImpl applyForce(Vec2 impulse, Vec2 point,
                        bool wake = true) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_ApplyForce(id, impulse, point, wake);
        return *this;
    }

    BodyImpl applyForceToCenter(Vec2 impulse, bool wake = true) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_ApplyForceToCenter(id, impulse, wake);
        return *this;
    }

    /// Apply an impulse at a point. This immediately modifies the velocity.
    /// It also modifies the angular velocity if the point of application
    /// is not at the center of mass. This optionally wakes the body.
    /// The impulse is ignored if the body is not awake.
    /// @param bodyId The body id
    /// @param impulse the world impulse vector, usually in N*s or kg*m/s.
    /// @param point the world position of the point of application. If not
    /// provided, then the center of the body is used.
    /// @param wake also wake up the body
    /// @warning This should be used for one-shot impulses. If you need a steady
    /// force, use a force instead, which will work better with the sub-stepping
    /// solver.
    BodyImpl applyLinearImpulse(Vec2 impulse, Vec2 point,
                                bool wake = true) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_ApplyLinearImpulse(id, impulse, point, wake);
        return *this;
    }

    BodyImpl applyLinearImpulse(Vec2 impulse, bool wake = true) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_ApplyLinearImpulseToCenter(id, impulse, wake);
        return *this;
    }

    /// This update the mass properties to the sum of the mass properties of the
    /// shapes. This normally does not need to be called unless you called
    /// SetMassData to override the mass and you later want to reset the mass.
    /// You may also use this when automatic mass computation has been disabled.
    /// You should call this regardless of body type. Note that sensor shapes
    /// may have mass.
    BodyImpl applyMassFromShapes() const NOEXCEPT
        requires(not isConst)
    {
        b2Body_ApplyMassFromShapes(id);
        return *this;
    }

    /// Apply a torque. This affects the angular velocity without affecting the
    /// linear velocity. This optionally wakes the body. The torque is ignored
    /// if the body is not awake.
    /// @param bodyId The body id
    /// @param torque about the z-axis (out of the screen), usually in N*m.
    /// @param wake also wake up the body
    BodyImpl applyTorque(f32 torque, bool wake = true) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_ApplyTorque(id, torque, wake);
        return *this;
    }

    /// Get the current world AABB that contains all the attached shapes. Note
    /// that this may not encompass the body origin. If there are no shapes
    /// attached then the returned AABB is empty and centered on the body
    /// origin.
    [[nodiscard]] AABB computeAABB() const NOEXCEPT
    {
        return b2Body_ComputeAABB(id);
    }

    /// Enable/disable contact events on all shapes.
    /// @see b2ShapeDef::enableContactEvents
    /// @warning changing this at runtime may cause mismatched begin/end touch
    /// events
    BodyImpl setContactEventsEnabled(bool isEnabled) NOEXCEPT
        requires(not isConst)
    {
        b2Body_EnableContactEvents(id, isEnabled);
        return *this;
    }

    /// Enable/disable hit events on all shapes
    /// @see b2ShapeDef::enableHitEvents
    BodyImpl setHitEventsAreEnabled(bool isEnabled) NOEXCEPT
        requires(not isConst)
    {
        b2Body_EnableHitEvents(id, isEnabled);
        return *this;
    }

    /// Get the touching contact data for a body.
    /// @note Box2D uses speculative collision so some contact points may be
    /// separated.
    [[nodiscard]] Result<std::span<ContactData>, alloc::Error>
    contactData(Allocator *allocator) const NOEXCEPT
    {
        return detail::getDataBufferHelper<ContactData>(
            allocator, id, detail::bodyGetContactCapacity,
            detail::bodyGetContactDataUnsafe);
    }

    struct ContactDataResult
    {
        ContactData data;
        bool hasValue;
    };

    [[nodiscard]] ContactDataResult firstContactData() const NOEXCEPT
    {
        ContactData out{};
        const int numWritten = detail::bodyGetContactDataUnsafe(id, &out, 1);

        if (numWritten)
            return {out, true};
        return {};
    }

    /// Get the joint ids for all joints on this body
    [[nodiscard]] Result<std::span<b2JointId>, alloc::Error>
    joints(Allocator *allocator) const NOEXCEPT
    {
        return detail::getDataBufferHelper<b2JointId>(
            allocator, id, detail::bodyGetJointCount,
            detail::bodyGetJointsUnsafe);
    }

    /// May return a null joint, check isValid() on the result
    [[nodiscard]] b2JointId firstJoint() const NOEXCEPT
    {
        b2JointId out{};
        const int numWritten = detail::bodyGetJointsUnsafe(id, &out, 1);

        if (numWritten)
            return out;
        return {};
    }

    /// Get the shape ids for all shapes on this body, returning the relevant
    /// error
    ///
    /// caller owns returned memory
    [[nodiscard]] Result<std::span<ShapeImpl<isConst>>, alloc::Error>
    shapesErr(Allocator *allocator) const NOEXCEPT
    {
        return detail::getDataBufferHelper<b2ShapeId>(
                   allocator, id, detail::bodyGetShapeCount,
                   detail::bodyGetShapesUnsafe)
            .map(detail::castShapeIDToShape<isConst>);
    }

    /// Get all the shape ids for tall shapes on this body, returning empty
    /// span on an error
    ///
    /// caller owns returned memory
    [[nodiscard]] std::span<ShapeImpl<isConst>>
    shapes(Allocator *allocator) const NOEXCEPT
    {
        return detail::getDataBufferHelper<b2ShapeId>(
                   allocator, id, detail::bodyGetShapeCount,
                   detail::bodyGetShapesUnsafe)
            .map(detail::castShapeIDToShape<isConst>)
            .value_or({});
    }

    /// Might return null ID, check isValid on result
    [[nodiscard]] ShapeImpl<isConst> firstShape() const NOEXCEPT
    {
        b2ShapeId out{};
        const int numWritten = detail::bodyGetShapesUnsafe(id, &out, 1);

        if (numWritten)
            return out;
        return {};
    }

    /// Get the number of shapes on this body
    [[nodiscard]] u64 shapeCount() const NOEXCEPT
    {
        return detail::bodyGetShapeCount(id); // returns u32
    }

    /// Get the number of joints on this body
    [[nodiscard]] u64 jointCount() const NOEXCEPT
    {
        return detail::bodyGetJointCount(id); // returns u32
    }

    /// @return true if this body is awake
    [[nodiscard]] bool isAwake() const NOEXCEPT { return b2Body_IsAwake(id); }

    /// Wake a body from sleep. This wakes the entire island the body is
    /// touching.
    /// @warning Putting a body to sleep will put the entire island of bodies
    /// touching this body to sleep, which can be expensive and possibly
    /// unintuitive.
    BodyImpl setIsAwake(bool isAwakeNow) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_SetAwake(id, isAwakeNow);
        return *this;
    }

    /// Is this body a bullet?
    [[nodiscard]] bool isBullet() const NOEXCEPT { return b2Body_IsBullet(id); }

    /// Set this body to be a bullet. A bullet does continuous collision
    /// detection against dynamic bodies (but not other bullets).
    BodyImpl setIsBullet(bool isBulletNow) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_SetBullet(id, isBulletNow);
        return *this;
    }

    /// Get the current gravity scale
    [[nodiscard]] f32 gravityScale() const NOEXCEPT
    {
        return b2Body_GetGravityScale(id);
    }

    /// Adjust the gravity scale. Normally this is set in b2BodyDef before
    /// creation.
    /// @see b2BodyDef::gravityScale
    BodyImpl setGravityScale(f32 newGravityScale) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_SetGravityScale(id, newGravityScale);
    }

    /// Get the mass data for a body
    [[nodiscard]] MassData massData() const NOEXCEPT
    {
        return b2Body_GetMassData(id);
    }

    /// Override the body's mass properties. Normally this is computed
    /// automatically using the shape geometry and density. This information is
    /// lost if a shape is added or removed or if the body type changes.
    BodyImpl setMassData(const MassData &newMassData) const NOEXCEPT
        requires(not isConst)
    {
        return b2Body_SetMassData(id, newMassData);
    }

    /// Set the velocity to reach the given transform after a given time step.
    /// The result will be close but maybe not exact. This is meant for
    /// kinematic bodies. The target is not applied if the velocity would be
    /// below the sleep threshold. This will automatically wake the body if
    /// asleep.
    BodyImpl setTargetTransform(Transform target, f32 timeStep,
                                bool wake) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_SetTargetTransform(id, target, timeStep, wake);
        return *this;
    }

    /// Set the world transform of a body. This acts as a teleport and is fairly
    /// expensive.
    /// @note Generally you should create a body with then intended transform.
    /// @see b2BodyDef::position and b2BodyDef::angle
    BodyImpl setTransform(Vec2 newPosition, Rotation newRotation) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_SetTransform(id, newPosition, newRotation);
        return *this;
    }

    /// Get the world position of a body. This is the location of the body
    /// origin.
    [[nodiscard]] Vec2 position() const NOEXCEPT
    {
        return b2Body_GetPosition(id);
    }

    /// Get the world rotation of a body as a cosine/sine pair (complex number)
    [[nodiscard]] Rotation rotation() const NOEXCEPT
    {
        return b2Body_GetRotation(id);
    }

    /// Get the world transform of a body.
    [[nodiscard]] Transform transform() const NOEXCEPT
    {
        return b2Body_GetTransform(id);
    }

    /// Get the body type: static, kinematic, or dynamic
    [[nodiscard]] BodyType type() const NOEXCEPT
    {
        return static_cast<BodyType>(b2Body_GetType(id));
    }

    /// Change the body type. This is an expensive operation. This automatically
    /// updates the mass properties regardless of the automatic mass setting.
    BodyImpl setType(BodyType newType) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_SetType(id, static_cast<b2BodyType>(newType));
        return *this;
    }

    /// Body identifier validation. Can be used to detect orphaned ids. Provides
    /// validation for up to 64K allocations.
    [[nodiscard]] bool isValid() const NOEXCEPT { return b2Body_IsValid(id); }

    explicit operator bool() const NOEXCEPT { return isValid(); }

    /// Get the body name. May be null.
    [[nodiscard]] const char *name() const NOEXCEPT
    {
        return b2Body_GetName(id);
    }

    /// Set the body name. Up to 31 characters excluding 0 termination.
    BodyImpl setName(const char *newName) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_SetName(id, newName);
        return *this;
    }

    /// Set the user data for a body
    BodyImpl setUserData(void *userData) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_SetUserData(id, userData);
        return *this;
    }

    /// Get the user data stored in a body
    [[nodiscard]] void *userData() const NOEXCEPT
    {
        return b2Body_GetUserData(id);
    }

    /// Get a local point on a body given a world point
    [[nodiscard]] Vec2 toLocal(Vec2 worldPoint) const NOEXCEPT
    {
        return b2Body_GetLocalPoint(id, worldPoint);
    }

    /// Get a world point on a body given a local point
    [[nodiscard]] Vec2 toWorld(Vec2 localPoint) const NOEXCEPT
    {
        return b2Body_GetWorldPoint(id, localPoint);
    }

    /// Get the linear velocity of a body's center of mass. Usually in meters
    /// per second.
    [[nodiscard]] Vec2 linearVelocity() const NOEXCEPT
    {
        return b2Body_GetLinearVelocity(id);
    }

    /// Get the angular velocity of a body in radians per second
    [[nodiscard]] f32 angularVelocity() const NOEXCEPT
    {
        return b2Body_GetAngularVelocity(id);
    }

    /// Set the linear velocity of a body. Usually in meters per second.
    BodyImpl setLinearVelocity(Vec2 newLinearVelocity) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_SetLinearVelocity(id, newLinearVelocity);
        return *this;
    }

    /// Set the angular velocity of a body in radians per second
    BodyImpl setAngularVelocity(f32 newAngularVelocity) const NOEXCEPT
        requires(not isConst)
    {
        b2Body_SetAngularVelocity(id, newAngularVelocity);
        return *this;
    }

    BodyImpl enable() const NOEXCEPT
        requires(not isConst)
    {
        b2Body_Enable(id);
        return *this;
    }

    BodyImpl disable() const NOEXCEPT
        requires(not isConst)
    {
        b2Body_Disable(id);
        return *this;
    }

    [[nodiscard]] bool isEnabled() const NOEXCEPT
    {
        return b2Body_IsEnabled(id);
    }
};

using Body = BodyImpl<false>;
using BodyConst = BodyImpl<true>;

template <bool isConst> class WorldImpl
{
  private:
    b2WorldId id;

    WorldImpl(b2WorldId _id) NOEXCEPT : id(_id) {}

  public:
    [[nodiscard]] static WorldDef defaultDefinition()
    {
        return b2DefaultWorldDef();
    }

    friend class WorldImpl<not isConst>;
    template <bool bodyConst> friend class BodyImpl;
    template <bool shapeConst> friend class ShapeImpl;

    WorldImpl() = delete;

    constexpr operator b2WorldId() const NOEXCEPT { return id; }

    constexpr operator WorldImpl<true>() const NOEXCEPT
        requires(not isConst)
    {
        return WorldImpl<true>(id);
    }

    static WorldImpl createWorld(const b2WorldDef *def)
    {
        if (!def) {
            const auto defaultDef = WorldImpl::defaultDefinition();
            return WorldImpl(b2CreateWorld(&defaultDef));
        }
        return WorldImpl(b2CreateWorld(def));
    }

    Body createBody(const BodyDef *def) const NOEXCEPT
        requires(not isConst)
    {
        if (!def) {
            const auto defaultDef = Body::defaultDefinition();
            return Body(b2CreateBody(id, &defaultDef));
        }
        return Body(b2CreateBody(id, def));
    }

    void destroy() const NOEXCEPT
        requires(not isConst)
    {
        b2DestroyWorld(id);
    }

    /// Cast a capsule mover through the world. This is a special shape cast
    /// that handles sliding along other shapes while reducing clipping.
    [[nodiscard]] f32 castMover(const Capsule &mover, Vec2 origin,
                                Vec2 translation,
                                PhysicsQueryFilter filter) const NOEXCEPT
    {
        return b2World_CastMover(id, origin, &mover, translation, filter);
    }

    void collideMoverUserData(const Capsule &mover, Vec2 origin,
                              PhysicsQueryFilter filter,
                              PhysicsPlaneResultFunction planeResultFunction,
                              void *userdata) const NOEXCEPT
    {
        b2World_CollideMover(id, origin, &mover, filter, planeResultFunction,
                             userdata);
    }

    /// Collide a capsule mover with the world, gathering collision planes that
    /// can be fed to solvePlanes. Useful for kinematic character movement.
    /// The mover capsule and the resulting planes are relative to the origin.
    /// Return true from the callback function to continue looking at planes, or
    /// false to stop
    template <typename Callable>
        requires std::is_invocable_r_v<bool, Callable &, ShapeImpl<true>,
                                       const PlaneResult *>
    void collideMover(const Capsule &mover, Vec2 origin,
                      PhysicsQueryFilter filter, Callable &planeVisitor)
    {
        void *userdata = std::addressof(planeVisitor);
        auto function = [](b2ShapeId shape, const b2PlaneResult *plane,
                           void *ctx) -> bool {
            return std::invoke(static_cast<Callable *>(ctx),
                               ShapeImpl<true>(shape), plane);
        };
        this->collideMoverUserData(mover, origin, filter, function, userdata);
    }

    struct RayCastOptions
    {
        /// The start point of the ray
        Vec2 origin = {};
        /// The translation of the ray from the start point to the end point
        Vec2 translation = {};
        /// Contains bit flags to filter unwanted shapes from the results
        PhysicsQueryFilter filter = {};
    };

    /// Cast a ray into the world to collect shapes in the path of the ray.
    /// Your callback function controls whether you get the closest point, any
    /// point, or n-points.
    /// The return value of the callback can control things:
    /// return -1: ignore this shape and continue
    /// return 0: terminate the ray cast
    /// return fraction: clip the ray to this point
    /// return 1: don't clip the ray and continue
    /// @note The callback function may receive shapes in any order
    ///	@return traversal performance counters
    [[nodiscard]] PhysicsTreeStats
    castRayUserData(const RayCastOptions &options,
                    PhysicsRaycastResultFunction *fcn = nullptr,
                    void *userdata = nullptr) const NOEXCEPT
    {
        return b2WorldCastRay(id, options.origin, options.translation,
                              options.filter, options.context);
    }

    /// The return value of the callback can control things:
    /// return -1: ignore this shape and continue
    /// return 0: terminate the ray cast
    /// return fraction: clip the ray to this point
    /// return 1: don't clip the ray and continue
    template <typename Callable>
        requires std::is_invocable_r_v<
            f32, Callable &, ShapeImpl<true> /* shapeHit */,
            Vec2 /* position */, Vec2 /* collision normal */,
            f32 /* fraction of ray at which this hit occurred*/>
    PhysicsTreeStats castRay(const RayCastOptions &options,
                             Callable &hitCallback)
    {
        void *userdata = std::addressof(hitCallback);
        auto function = [](b2ShapeId shapeId, b2Pos point, b2Vec2 normal,
                           float fraction, void *context) -> bool {
            return std::invoke(static_cast<Callable *>(context),
                               ShapeImpl<true>(shapeId), point, normal,
                               fraction);
        };
        return this->castRayUserData(options, function, userdata);
    }

    struct RayCastClosestOptions
    {
        /// The start point of the ray
        Vec2 origin = {};
        /// The translation of the ray from the start point to the end point
        Vec2 translation = {};
        /// Contains bit flags to filter unwanted shapes from the results
        PhysicsQueryFilter filter = {};
    };

    /// Cast a ray into the world to collect the closest hit. This is a
    /// convenience function. Ignores initial overlap. This is less general than
    /// castRay() and does not allow for custom filtering.
    [[nodiscard]] PhysicsRaycastResult<isConst>
    castRayClosest(const RayCastClosestOptions &options) const NOEXCEPT
    {
        b2WorldCastRayClosest(id, options.origin, options.translation,
                              options.filter);
    }

    struct ShapeCastOptions
    {
        const b2ShapeProxy *proxy = nullptr;
        Vec2 translation = {};
        PhysicsQueryFilter filter = {};
    };

    /// Cast a shape through the world. Similar to a cast ray except that a
    /// shape is cast instead of a point.
    ///	@see castRay
    PhysicsTreeStats
    castShapeUserData(const ShapeCastOptions &options,
                      PhysicsRaycastResultFunction *callback = nullptr,
                      void *userdata = nullptr) const NOEXCEPT
    {
        b2WorldCastShape(id, *options.proxy, options.translation,
                         options.filter, callback, userdata);
    }

    template <typename Callable>
        requires std::is_invocable_r_v<f32, Callable, ShapeImpl<true>,
                                       Vec2 /* point */, Vec2 /* normal */,
                                       f32 /* fraction along moved ray */>
    PhysicsTreeStats castShape(const ShapeCastOptions &options,
                               Callable &&callable) const NOEXCEPT
    {
        auto *ptr = static_cast<void *>(std::addressof(callable));
        auto function = [](b2ShapeId shape, Vec2 point, Vec2 normal,
                           f32 fraction, void *userData) {
            std::invoke(static_cast<Callable *>(userData),
                        ShapeImpl<true>(shape), point, normal, fraction);
        };
        b2WorldCastShape(id, *options.proxy, options.translation,
                         options.filter, options.function, options.context);
    }

    /// Enable/disable continuous collision between dynamic and static bodies.
    /// Generally you should keep continuous collision enabled to prevent fast
    /// moving objects from going through static objects. The performance gain
    /// from disabling continuous collision is minor.
    /// @see b2WorldDef
    WorldImpl setIsContinuousCollisionEnabled(bool isEnabledNow) const NOEXCEPT
        requires(not isConst)
    {
        b2World_EnableContinuous(id, isEnabledNow);
        return *this;
    }

    /// Enable/disable sleep. If your application does not need sleeping, you
    /// can gain some performance by disabling sleep completely at the world
    /// level.
    /// @see b2WorldDef
    WorldImpl setIsSleepingEnabled(bool isEnabledNow) const NOEXCEPT
        requires(not isConst)
    {
        b2World_EnableSleeping(id, isEnabledNow);
        return *this;
    }

    /// Adjust contact tuning parameters
    /// @param worldId The world id
    /// @param hertz The contact stiffness (cycles per second)
    /// @param dampingRatio The contact bounciness with 1 being critical damping
    /// (non-dimensional)
    /// @param pushSpeed The maximum contact constraint push out speed (meters
    /// per second)
    /// @note Advanced feature
    WorldImpl
    setContactTuning(const PhysicsContactTuningOptions &options) const NOEXCEPT
        requires(not isConst)
    {
        b2World_SetContactTuning(id, options.hertz, options.dampingRatio,
                                 options.pushSpeed);
        return *this;
    }

    /// Adjust the restitution threshold. It is recommended not to make this
    /// value very small because it will prevent bodies from sleeping. Usually
    /// in meters per second.
    /// @see b2WorldDef
    WorldImpl setRestitutionThreshhold(f32 value) const NOEXCEPT
        requires(not isConst)
    {
        b2World_SetRestitutionThreshold(id, value);
        return *this;
    }

    /// Simulate a world for one time step. This performs collision detection,
    /// integration, and constraint solution.
    /// @param worldId The world to simulate
    /// @param timeStep The amount of time to simulate, this should be a fixed
    /// number. Usually 1/60.
    /// @param subStepCount The number of sub-steps, increasing the sub-step
    /// count can increase accuracy. Usually 4.
    void step(f32 timeStep = 1.0f / 60.f, u16 subStepCount = 4) const NOEXCEPT
        requires(not isConst)
    {
        b2World_Step(id, timeStep, subStepCount);
    }
};

using World = WorldImpl<false>;
using WorldConst = WorldImpl<true>;
} // namespace b2
#endif
