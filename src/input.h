#ifndef __GAME_INPUT_H__
#define __GAME_INPUT_H__

#include <imgui.h>
#include <raylib.h>

#include "macros.h"

/// introduces a rudimentary concept of focus, something is either an input to
/// the game or an input to UI
[[nodiscard]] inline bool isMouseButtonDownInApp(int button)
{
    return not ImGui::GetIO().WantCaptureMouse and IsMouseButtonDown(button);
}

[[nodiscard]] inline f32 mouseScrollDeltaInApp()
{
    if (ImGui::GetIO().WantCaptureMouse)
        return {};
    return GetMouseWheelMoveV().y;
}

[[nodiscard]] inline bool isKeyPressedInApp(int button)
{
    return not ImGui::GetIO().WantCaptureMouse and IsKeyPressed(button);
}

[[nodiscard]] inline Vector2 getMouseDeltaInApp()
{
    if (ImGui::GetIO().WantCaptureMouse)
        return {};
    return GetMouseDelta();
}

#endif
