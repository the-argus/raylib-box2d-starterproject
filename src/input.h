#ifndef __GAME_INPUT_H__
#define __GAME_INPUT_H__

#include <imgui.h>
#include <raylib.h>

#include "macros.h"

// TODO: should probably know whether the app had focus last frame, then we can
// consider losing focus to cause a key release or regaining focus while pressed
// to cause a key press

/// introduces a rudimentary concept of focus, something is either an input to
/// the game or an input to UI
[[nodiscard]] inline bool isMouseButtonDownInApp(int button)
{
    return not ImGui::GetIO().WantCaptureMouse and IsMouseButtonDown(button);
}

[[nodiscard]] inline bool isMouseButtonJustPressedInApp(int button)
{
    return not ImGui::GetIO().WantCaptureMouse and IsMouseButtonPressed(button);
}

[[nodiscard]] inline f32 mouseScrollDeltaInApp()
{
    if (ImGui::GetIO().WantCaptureMouse)
        return {};
    return GetMouseWheelMoveV().y;
}

[[nodiscard]] inline bool isKeyJustPressedInApp(int button)
{
    return not ImGui::GetIO().WantCaptureKeyboard and IsKeyPressed(button);
}

[[nodiscard]] inline bool isKeyDownInApp(int button)
{
    return not ImGui::GetIO().WantCaptureKeyboard and IsKeyDown(button);
}

[[nodiscard]] inline bool isKeyUpInApp(int button)
{
    return ImGui::GetIO().WantCaptureKeyboard || IsKeyUp(button);
}

[[nodiscard]] inline bool isKeyJustReleasedInApp(int button)
{
    return not ImGui::GetIO().WantCaptureKeyboard and IsKeyReleased(button);
}

[[nodiscard]] inline Vector2 getMouseDeltaInApp()
{
    if (ImGui::GetIO().WantCaptureMouse)
        return {};
    return GetMouseDelta();
}

#endif
