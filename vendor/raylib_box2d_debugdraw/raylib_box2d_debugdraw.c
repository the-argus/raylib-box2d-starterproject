#include "raylib_box2d_debugdraw.h"

static Color to_raylib_color(b2HexColor b2Color) {
	Color raylib_color = {
		(b2Color >> 16) & 255,
		(b2Color >> 8) & 255,
		(b2Color >> 0) & 255,
		255
	};
	return raylib_color;
}

static Vector2 to_raylib_vector2(b2Vec2 b2vec) {
	Vector2 raylib_vec = {
		b2vec.x,
		b2vec.y,
	};
	return raylib_vec;
}

static b2Vec2 to_b2Vec2(Vector2 rlvec) {
	b2Vec2 b2vec = {
		rlvec.x,
		rlvec.y,
	};
	return b2vec;
}

static b2RaylibDebugDrawConfig get_draw_config(void *context) {
	if (context) {
		return *(b2RaylibDebugDrawConfig *) context;
	}
	else {
		return b2DefaultRaylibDebugDrawConfig();
	}
}

static void DrawSolidCapsuleFcn(b2Vec2 b2p1, b2Vec2 b2p2, float radius, b2HexColor b2color, void* context);

/// Draw a closed polygon provided in CCW order.
static void DrawPolygonFcn(const b2Vec2* vertices, int vertexCount, b2HexColor b2color, void* context) {
	Color color = to_raylib_color(b2color);
	DrawLineStrip((const Vector2 *) vertices, vertexCount, color);
	DrawLineV(to_raylib_vector2(vertices[vertexCount - 1]), to_raylib_vector2(vertices[0]), color);
}

/// Draw a solid closed polygon provided in CCW order.
static void DrawSolidPolygonFcn(b2Transform transform, const b2Vec2* vertices, int vertexCount, float radius, b2HexColor b2color, void* context) {
	// First setup transformed vertices in CW order
	Vector2 transformed_vertices[vertexCount + 1];
	for (int i = 0; i < vertexCount; i++) {
		b2Vec2 cw_vertex = vertices[vertexCount - 1 - i];
		b2Vec2 transformed_vertex = b2TransformPoint(transform, cw_vertex);
		transformed_vertices[i] = to_raylib_vector2(transformed_vertex);
	}
	// Close the triangle strip with a copy of the first vertex
	transformed_vertices[vertexCount] = transformed_vertices[0];

	Color color = to_raylib_color(b2color);
	DrawTriangleStrip(transformed_vertices, vertexCount + 1, color);

	if (radius > 0) {
		for (int i = 0; i < vertexCount; i++) {
			DrawSolidCapsuleFcn(to_b2Vec2(transformed_vertices[i]), to_b2Vec2(transformed_vertices[i + 1]), radius, b2color, context);
		}
	}
}

/// Draw a circle.
static void DrawCircleFcn(b2Vec2 center, float radius, b2HexColor b2color, void* context) {
	Color color = to_raylib_color(b2color);
	DrawCircleLinesV(to_raylib_vector2(center), radius, color);
}

/// Draw a solid circle.
static void DrawSolidCircleFcn(b2Transform transform, float radius, b2HexColor b2color, void* context) {
	Color color = to_raylib_color(b2color);
	Vector2 center = to_raylib_vector2(transform.p);
	DrawCircleV(center, radius, color);
}

/// Draw a solid capsule.
static void DrawSolidCapsuleFcn(b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor b2color, void* context) {
	Color color = to_raylib_color(b2color);
	DrawCircleV(to_raylib_vector2(p1), radius, color);
	DrawCircleV(to_raylib_vector2(p2), radius, color);

	// Draw center rectangle as a polygon
	b2Vec2 center = b2Lerp(p1, p2, 0.5);
	b2Vec2 delta = b2Sub(p2, p1);
	float length = b2Length(delta);
	b2Transform transform = {
		center,
		{ delta.x / length, delta.y / length },
	};
	b2Vec2 points[] = {
		{ -length / 2, radius },
		{ -length / 2, -radius },
		{ length / 2, -radius },
		{ length / 2, radius },
	};
	DrawSolidPolygonFcn(transform, points, sizeof(points) / sizeof(points[0]), 0, b2color, context);
}

/// Draw a line segment.
static void DrawSegmentFcn(b2Vec2 p1, b2Vec2 p2, b2HexColor b2color, void* context) {
	Color color = to_raylib_color(b2color);
	DrawLineV(to_raylib_vector2(p1), to_raylib_vector2(p2), color);
}

/// Draw a transform. Choose your own length scale.
static void DrawTransformFcn(b2Transform transform, void* context) {
	b2RaylibDebugDrawConfig config = get_draw_config(context);

	b2Vec2 x_vec = { config.transformLength, 0 };
	b2Vec2 transformed_x = b2TransformPoint(transform, x_vec);
	DrawLineV(to_raylib_vector2(transform.p), to_raylib_vector2(transformed_x), config.transformColorX);
	
	b2Vec2 y_vec = { 0, config.transformLength };
	b2Vec2 transformed_y = b2TransformPoint(transform, y_vec);
	DrawLineV(to_raylib_vector2(transform.p), to_raylib_vector2(transformed_y), config.transformColorY);
}

/// Draw a point.
static void DrawPointFcn(b2Vec2 p, float size, b2HexColor b2color, void* context) {
	Color color = to_raylib_color(b2color);
	DrawCircleV(to_raylib_vector2(p), size, color);
}

/// Draw a string in world space
static void DrawStringFcn(b2Vec2 p, const char* s, b2HexColor b2color, void* context) {
	b2RaylibDebugDrawConfig config = get_draw_config(context);
	Color color = to_raylib_color(b2color);
	DrawText(s, p.x, p.y, config.fontSize, color);
}

b2RaylibDebugDrawConfig b2DefaultRaylibDebugDrawConfig() {
	b2RaylibDebugDrawConfig debug_draw_config = {
		RAYLIB_BOX2D_DEBUG_DRAW_DEFAULT_COLOR_X,
		RAYLIB_BOX2D_DEBUG_DRAW_DEFAULT_COLOR_Y,
		RAYLIB_BOX2D_DEBUG_DRAW_DEFAULT_LENGTH,
		RAYLIB_BOX2D_DEBUG_DRAW_DEFAULT_FONT_SIZE,
	};
	return debug_draw_config;
}

b2DebugDraw b2RaylibDebugDraw() {
	b2DebugDraw debug_draw = b2DefaultDebugDraw();
	debug_draw.DrawPolygonFcn = DrawPolygonFcn;
	debug_draw.DrawSolidPolygonFcn = DrawSolidPolygonFcn;
	debug_draw.DrawCircleFcn = DrawCircleFcn;
	debug_draw.DrawSolidCircleFcn = DrawSolidCircleFcn;
	debug_draw.DrawSolidCapsuleFcn = DrawSolidCapsuleFcn;
	debug_draw.DrawSegmentFcn = DrawSegmentFcn;
	debug_draw.DrawTransformFcn = DrawTransformFcn;
	debug_draw.DrawPointFcn = DrawPointFcn;
	debug_draw.DrawStringFcn = DrawStringFcn;
	return debug_draw;
}
