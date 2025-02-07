#pragma once

#include "raylib.h"

typedef struct Renderer {
	RenderTexture2D renderTex;
	Texture2D textures[8];
	float renderScale;
	Vector2 virtualMouse;
	Vector2 cameraPosition;
	float cameraRotation;
	Vector2 cameraForward;
	unsigned int ray_count;
	unsigned int column_pixel_width;
} Renderer;

typedef enum DrawMode {
	GAME,
	MAP,
	GAME_DEBUG,
	MAP_DEBUG
} DrawMode;

typedef enum ShadingMode {
	TEXTURED,
	FLAT
} ShadingMode;

typedef enum RenderQuality {
	VERY_LOW,
	LOW,
	MEDIUM,
	HIGH,
	ULTRA
} RenderQuality;

typedef enum GameMode {
	MAIN_MENU,
	EDITOR,
	SETTINGS,
	PLAYING
} GameMode;

typedef struct RayData {
	Vector2 start;
	Vector2 end;
	float distance;
	bool hitX;
	float castAngleRadians;
	float offset;
} RayData;

void CreateRenderer(bool fullscreen, bool vsync, unsigned int screenWidth, unsigned int screenHeight, unsigned int fov, unsigned int mapData[10][10]);
void LoadTextures();
void UnloadTextures();
void UpdateRendererMapData(unsigned int mapData[10][10]);
void UpdateRenderingSettings(bool fullscreen, bool vsync, unsigned int screenWidth, unsigned int screenHeight, unsigned int fov);

void RendererInput();

void UpdateFrameBuffer();
void UpdateScreen();
void UpdateRenderCamera(Vector2 position, float rotation);
void UpdateDrawMode(DrawMode newDrawMode);
void UpdateShadingMode(ShadingMode newShadingMode);
void UpdateRenderQuality(RenderQuality newRenderQuality);

void DDA(struct RayData rays[], Vector2 position, float angle);
void DDASingle(Vector2 position, float angle);
void DDANonLinear(struct RayData rays[], Vector2 position, float angle);

void DrawDebug();
void Draw2D(const struct RayData rays[]);
void Draw3D(const struct RayData rays[], Texture2D tex);
void DrawMainMenu();