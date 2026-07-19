#include "arena.h"
#include "assets.h"
#include "box2d.h"
#include "convert.h"
#include "defer.h"
#include "game_context.h"
#include "game_lib.h"
#include "input.h"
#include "logging.h"
#include "player.h"

#include <raylib.h>
#include <raymath.h>
#include <rlImGui.h>

#include <raylib_box2d_debugdraw.h>

void debugDrawBox2dWorld(b2::World world, b2RaylibDebugDrawConfig *config)
{
    auto debugDrawer = b2RaylibDebugDraw();

    debugDrawer.drawBodyNames = true;
    debugDrawer.drawShapes = true;
    debugDrawer.drawBounds = true;
    debugDrawer.drawContacts = true;
    debugDrawer.drawContactNormals = true;
    debugDrawer.drawContactForces = true;
    debugDrawer.drawMass = true;

    debugDrawer.context = config;

    b2World_Draw(world, &debugDrawer);
}

extern "C"
{
    // called once at startup, we return the ctx which will be passed on all
    // future calls
    HOTRELOAD_EXPORT void *init()
    {
        using namespace b2;
        LOGINFO_MSG(Gameplay, "gamelib init() called");
        b2::WorldDef def = World::defaultDefinition();
        def.gravity = {0.f, 10.f};
        const auto world = World::createWorld(&def);

        auto box1 = Body::defaultDefinition();
        box1.type = b2BodyType::b2_staticBody;
        box1.position = {.x = 0, .y = 10};
        box1.name = "floor";
        auto box2 = Body::defaultDefinition();
        box2.type = b2BodyType::b2_dynamicBody;
        box2.position = {.x = 0, .y = -10};
        box2.name = "square";
        box2.motionLocks = {.angularZ = true};

        auto *out = new GameContext{
            .world = world,
            .floor = world.createBody(&box1),
            .square = world.createBody(&box2),
            .camera = Camera2D{.target = conv(box2.position), .zoom = 16.f},
            .textureMissing = assets::uploadTexture("missing.png"),
        };

        Result player = out->gameAllocator.make<Player>();
        Result debugDrawConfig =
            out->gameAllocator.make<b2RaylibDebugDrawConfig>();
        if (!player || !debugDrawConfig)
            LOGFATAL_MSG(Gameplay, "OOM");

        out->player = &player.value();
        out->debugDrawConfig = &debugDrawConfig.value();
        b2DefaultRaylibDebugDrawConfig(out->debugDrawConfig);
        out->debugDrawConfig->fontSize = 8;

        out->square.addPolygonShape({}, b2MakeSquare(1.f));
        out->floor.addSegmentShape({}, {.point1 = {-10, 0}, .point2 = {10, 0}});

        return out;
    }

    HOTRELOAD_EXPORT void deinit(void *context)
    {
        auto *const ctx = static_cast<GameContext *>(context);
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
        auto *const ctx = static_cast<GameContext *>(context);

        CAllocator mainAllocator;
        Arena frameArena(&mainAllocator);

        ctx->frameAllocator = &frameArena;
        defer resetToNull = [ctx] { ctx->frameAllocator = nullptr; };

        ctx->player->update(ctx, GetFrameTime());

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
                                             conv(ctx->player->position), 0.1);

            ctx->camera.offset = {
                static_cast<float>(GetScreenWidth()) / 2.f,
                static_cast<float>(GetScreenHeight()) / 2.f,
            };
        } else if (isMouseButtonDownInApp(MOUSE_BUTTON_LEFT)) {
            ctx->camera.target -=
                getMouseDeltaInApp() * ctx->cameraDragSpeed / ctx->camera.zoom;
        }

        if (isKeyJustPressedInApp(KEY_P)) {
            ctx->trackingPlayer = not ctx->trackingPlayer;
        }

        if (isKeyJustPressedInApp(KEY_F)) {
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

            debugDrawBox2dWorld(ctx->world, ctx->debugDrawConfig);

            ctx->player->draw(ctx);

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
