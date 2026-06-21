#include "box2d.h"
#include "game_lib.h"
#include "logging.h"

struct Context
{
    World world;
    Body floor;
    Body square;
    Vec2U32 windowSize{}; // changes on events
};

extern "C"
{
    // called once at startup, we return the ctx which will be passed on all
    // future calls
    HOTRELOAD_EXPORT void *init()
    {
        LOGINFO_MSG(Gameplay, "gamelib init() called");
        const auto world = World::createWorld({});

        auto box1 = Body::defaultDefinition();
        box1.type = b2BodyType::b2_staticBody;
        box1.position = {.x = 0, .y = -10};
        box1.name = "floor";
        auto box2 = Body::defaultDefinition();
        box1.type = b2BodyType::b2_kinematicBody;
        box1.position = {.x = 0, .y = 10};
        box1.name = "square";

        auto *out = new Context{
            .world = world,
            .floor = world.createBody(&box1),
            .square = world.createBody(&box2),
        };

        constexpr Polygon squarePolygon = {
            .vertices = {{-1, 1}, {-1, -1}, {1, -1}, {1, 1}},
            .normals = {{-1, 0}, {0, -1}, {1, 0}, {0, 1}},
            .centroid = {},
            .count = 4,
        };
        out->square.addPolygonShape({}, squarePolygon);
        out->floor.addSegmentShape({}, {.point1 = {-10, 0}, .point2 = {10, 0}});

        return out;
    }

    HOTRELOAD_EXPORT void onHotReload(const hotreload::GlobalContext *context)
    {
        ImGui::SetCurrentContext(context->imguiContext);
        ImGui::SetAllocatorFunctions(context->imguiAlloc, context->imguiFree,
                                     context->imguiAllocUsrData);
    }

    /// Called every frame
    /// return true if continue, false if quit
    HOTRELOAD_EXPORT bool frame(void *context)
    {
        auto *const ctx = static_cast<Context *>(context);

        // TODO: put correct frame timestep here? honestly I'm not sure how
        // physics process vs. frame process is usually implemented. I guess it
        // could be an event on a timer? or just force vsync and use frametime
        // here... but then maybe things become unstable at low framerates
        ctx->world.step(1.0f / 60.0f);

        // if (ctx->windowSize) {
        //     SDL_SetRenderLogicalPresentation(
        //         renderer, ctx->windowSize->x, ctx->windowSize->y,
        //         SDL_LOGICAL_PRESENTATION_LETTERBOX);
        //     ctx->windowSize = null;
        // }
        return true;
    }
}
