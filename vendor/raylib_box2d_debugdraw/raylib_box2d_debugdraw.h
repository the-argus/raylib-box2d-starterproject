#ifndef __RAYLIB_BOX2D_DEBUGDRAW_H__
#define __RAYLIB_BOX2D_DEBUGDRAW_H__

/*
 * Taken from https://github.com/gilzoide/raylib-box2d-debugdraw
 */

#include <box2d/box2d.h>
#include <raylib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RAYLIB_BOX2D_DEBUG_DRAW_DEFAULT_COLOR_X
	#define RAYLIB_BOX2D_DEBUG_DRAW_DEFAULT_COLOR_X BLUE
#endif
#ifndef RAYLIB_BOX2D_DEBUG_DRAW_DEFAULT_COLOR_Y
	#define RAYLIB_BOX2D_DEBUG_DRAW_DEFAULT_COLOR_Y GREEN
#endif
#ifndef RAYLIB_BOX2D_DEBUG_DRAW_DEFAULT_LENGTH
	#define RAYLIB_BOX2D_DEBUG_DRAW_DEFAULT_LENGTH 25
#endif
#ifndef RAYLIB_BOX2D_DEBUG_DRAW_DEFAULT_FONT_SIZE
	#define RAYLIB_BOX2D_DEBUG_DRAW_DEFAULT_FONT_SIZE 16
#endif

/**
 * Extra configuration for the `b2DebugDraw` returned by `b2RaylibDebugDraw`.
 */
typedef struct b2RaylibDebugDrawConfig {
	/// Color used for drawing a Transform's X axis
	Color transformColorX;
	/// Color used for drawing a Transform's Y axis
	Color transformColorY;
	/// Length of the line used for drawing a Transform's axes
	float transformLength;
	/// Font size used when drawing text
	int fontSize;
} b2RaylibDebugDrawConfig;

/**
 * Returns the default configuration for `b2RaylibDebugDraw`.
 */
b2RaylibDebugDrawConfig b2DefaultRaylibDebugDrawConfig();

/**
 * Return a `b2DebugDraw` that draws using Raylib.
 * 
 * Users still need to set the `draw*` flags for drawing to actually happen.
 *
 * Optionally, set `context` to a pointer to `b2RaylibDebugDrawConfig` for extra configuration.
 * If the context is not set, the value returned by `b2DefaultRaylibDebugDrawConfig` will be used.
 */
b2DebugDraw b2RaylibDebugDraw();

#ifdef __cplusplus
}
#endif
#endif
