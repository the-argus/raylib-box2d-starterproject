#include "arena.h"
#include "assets.h"
#include "box2d.h"
#include "convert.h"
#include "defer.h"
#include "game_lib.h"
#include "logging.h"

#include <raylib.h>
#include <raymath.h>
#include <rlImGui.h>



void debugDrawPhysicsBody(Allocator *allocator, b2::Body body, Color color)
{
    auto shapes = body.shapesErr(allocator);
    if (shapes.isError()) {
        LOGWARN(Renderer, "failed to debug draw shape, error {}",
                static_cast<int>(shapes.error()));
        return;
    }

    for (const b2::Shape &shape : shapes.value()) {
        switch (shape.type()) {
        case b2::ShapeType::Segment: {
            auto segment = shape.segment();
            DrawLineEx(conv(segment.point1), conv(segment.point2), 1.f, color);
            break;
        }
        case b2::ShapeType::Capsule: {
            auto capsule = shape.capsule();
            DrawCircleLines(capsule.center1.x, capsule.center1.y,
                            capsule.radius, color);
            DrawCircleLines(capsule.center2.x, capsule.center2.y,
                            capsule.radius, color);
            Vector2 start{capsule.center1.x, capsule.center1.y};
            Vector2 end{capsule.center2.x, capsule.center2.y};
            DrawLineEx(start, end, capsule.radius, color);
            break;
        }
        case b2::ShapeType::Polygon: {
            auto polygon = shape.polygon();
            std::array<Vector2, 9> raylibFormat{};
            static_assert(sizeof(raylibFormat) ==
                          sizeof(polygon.vertices) + sizeof(Vector2));
            auto iter = raylibFormat.begin();
            for (b2::Vec2 vert : polygon.vertices) {
                uassert(iter != raylibFormat.end());
                iter->x = vert.x;
                iter->y = vert.y;
                ++iter;
            }
            raylibFormat.back() = raylibFormat.front(); // loop lines
            DrawLineStrip(raylibFormat.data(),
                          static_cast<int>(raylibFormat.size()), color);
            break;
        }
        case b2::ShapeType::Circle: {
            auto circle = shape.circle();
            DrawCircleLines(circle.center.x, circle.center.y, circle.radius,
                            color);
            break;
        }
        case b2::ShapeType::ChainSegment: {
            /* unused */
            break;
        }
        }
    }
}

extern "C"
{
    // called once at startup, we return the ctx which will be passed on all
    // future calls
    HOTRELOAD_EXPORT void *init()
    {
        using namespace b2;
        LOGINFO_MSG(Gameplay, "gamelib init() called");
        const auto world = World::createWorld({});

        auto box1 = Body::defaultDefinition();
        box1.type = b2BodyType::b2_staticBody;
        box1.position = {.x = 0, .y = -10};
        box1.name = "floor";
        auto box2 = Body::defaultDefinition();
        box2.type = b2BodyType::b2_kinematicBody;
        box2.position = {.x = 0, .y = 10};
        box2.name = "square";
        box2.motionLocks = {.angularZ = true};

        auto *out = new Context{
            .world = world,
            .floor = world.createBody(&box1),
            .square = world.createBody(&box2),
            .camera = Camera2D{.target = conv(box2.position), .zoom = 16.f},
            .textureMissing = assets::uploadTexture("missing.png"),
        };

        out->square.addPolygonShape({}, b2MakeSquare(1.f));
        out->floor.addSegmentShape({}, {.point1 = {-10, 0}, .point2 = {10, 0}});

        return out;
    }

    HOTRELOAD_EXPORT void deinit(void *context)
    {
        auto *const ctx = static_cast<Context *>(context);
        delete ctx;
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

        CAllocator mainAllocator;
        Arena frameArena(&mainAllocator);

        // TODO: fix our timestep
        // https://www.gafferongames.com/post/fix_your_timestep/
        // (Requires drawing things interpolated). We will just enable vsync
        // and pretend that every computer will not fall behind on physics
        // updates :D
        ctx->world.step(1.0f / 60.0f);

        // deal with camera inputs
        if (ctx->trackingPlayer) {
            // TODO: not framerate dependent lerp?
            ctx->camera.target = Vector2Lerp(ctx->camera.target,
                                             conv(ctx->square.position()), 0.1);

            ctx->camera.offset = {
                static_cast<float>(GetScreenWidth()) / 2.f,
                static_cast<float>(GetScreenHeight()) / 2.f,
            };
        } else if (isMouseButtonDownInApp(MOUSE_BUTTON_LEFT)) {
            ctx->camera.target -=
                getMouseDeltaInApp() * ctx->cameraDragSpeed / ctx->camera.zoom;
        }

        if (isKeyPressedInApp(KEY_P)) {
            ctx->trackingPlayer = not ctx->trackingPlayer;
        }

        if (isKeyPressedInApp(KEY_F)) {
#ifndef __EMSCRIPTEN__ // fullscreen is broken on some older emscripten versions
            ToggleFullscreen();
#endif
        }

        // zoom camera every tenth of a second while scrolling
        if (GetTime() - ctx->lastZoomTime > 0.1f) {
            const f32 scrollDelta = mouseScrollDeltaInApp();
            const bool zoomed = scrollDelta != 0.f;

            if (zoomed) {
                ctx->lastZoomTime = GetTime();

                const auto mousePos = GetMousePosition();
                const auto mouseWorldPos =
                    GetScreenToWorld2D(mousePos, ctx->camera);

                if (not ctx->trackingPlayer) {
                    ctx->camera.offset = mousePos;
                    ctx->camera.target = mouseWorldPos;
                }

                const float scale = 0.2f * scrollDelta;
                ctx->camera.zoom =
                    Clamp(expf(logf(ctx->camera.zoom) + scale), 0.125f, 64.0f);
            }
        }

        // calculations done, begin draw
        BeginDrawing();
        defer endDrawing = [] { EndDrawing(); };

        ClearBackground(DARKGRAY);

        // draw the world
        {
            BeginMode2D(ctx->camera);
            defer endMode2D = [] { EndMode2D(); };

            debugDrawPhysicsBody(&frameArena, ctx->square, RED);
            debugDrawPhysicsBody(&frameArena, ctx->floor, GRAY);

            DrawTexture(ctx->textureMissing, ctx->square.position().x,
                        ctx->square.position().y, WHITE);

            DrawRectangle(50, 50, 100, 100, BLUE);
        }

        rlImGuiBegin();
        defer rlEnd = [] { rlImGuiEnd(); };

        bool keepRunning = true;
        {
            ImGui::SetNextWindowSize(ImVec2{400, 400}, ImGuiCond_Appearing);
            ImGui::Begin("my window");
            defer end = [] { ImGui::End(); };

            ImGui::TextUnformatted("Press P to switch camera mode");
            ImGui::TextUnformatted("Press F to toggle fullscreen");
            ImGui::Text("Camera mode: %s",
                        ctx->trackingPlayer ? "Follow Player" : "Freecam");

            if (ImGui::Button("testing button")) {
                fmt::println("testing 3...");
            }
            if (ImGui::Button("Quit")) {
                keepRunning = false;
            }
        }

        return keepRunning;
    }
}
