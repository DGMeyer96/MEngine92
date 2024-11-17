/*
Raylib example file.
This is an example main file for a simple raylib project.
Use this as a starting point or replace it with your code.

For a C++ project simply rename the file to .cpp and re-run the build script 

-- Copyright (c) 2020-2024 Jeffery Myers
--
--This software is provided "as-is", without any express or implied warranty. In no event 
--will the authors be held liable for any damages arising from the use of this software.

--Permission is granted to anyone to use this software for any purpose, including commercial 
--applications, and to alter it and redistribute it freely, subject to the following restrictions:

--  1. The origin of this software must not be misrepresented; you must not claim that you 
--  wrote the original software. If you use this software in a product, an acknowledgment 
--  in the product documentation would be appreciated but is not required.
--
--  2. Altered source versions must be plainly marked as such, and must not be misrepresented
--  as being the original software.
--
--  3. This notice may not be removed or altered from any source distribution.

*/

#include "raylib.h"
#include "rlgl.h"
#include "main.h" 

#include "resource_dir.h"			// utility header for SearchAndSetResourceDir
#include <stdio.h>                  // Required for: fopen(), fclose(), fputc(), fwrite(), printf(), fprintf(), funopen()
#include <time.h>                   // Required for: time_t, tm, time(), localtime(), strftime()
#include <math.h>

// Rendering stuff
#define SCREEN_WIDTH 1400
#define SCREEN_HEIGHT 800
#define VIEWPORT_WIDTH 900
#define VIEWPORT_HEIGHT 675
#define FOV 70
#define HALF_FOV (FOV / 2)
#define VERTICAL_FOV (2 * atanf(tanf(FOV / 2) * (VIEWPORT_HEIGHT / VIEWPORT_WIDTH)))
#define COLUMN_PIXEL_WIDTH 5
#define RAY_COUNT (VIEWPORT_WIDTH / COLUMN_PIXEL_WIDTH)
#define DRAW_DISTANCE 20
#define ANGLE_STEP (float)FOV / (float)RAY_COUNT
#define TILE_SIZE_PIXELS 50
#define MAP_LENGTH 10
// FISHEYE Correction stuff
#define PROJECTION_PLANE_WIDTH (DRAW_DISTANCE * tanf(DEG2RAD * (FOV / 2)) * 2)
#define PROJECTION_PLANE_HALF_WIDTH (PROJECTION_PLANE_WIDTH / 2)
#define X_MAX (VIEWPORT_WIDTH - 1)
#define HEIGHT_RATIO ((float)VIEWPORT_HEIGHT / (float)VIEWPORT_WIDTH) / (FOV / 90.0f)

#define PROJECTION_PLANE_HEIGHT (DRAW_DISTANCE * tanf(VERTICAL_FOV / 2))
#define HALF_WALL_HEIGHT (10 / 2)

const int map[MAP_LENGTH][MAP_LENGTH] = {
	{ 1,1,1,1,1,1,1,1,1,1 },
	{ 1,0,0,0,0,0,0,0,0,1 },
	{ 1,0,0,0,0,1,0,0,1,1 },
	{ 1,0,0,0,0,0,0,0,0,1 },
	{ 1,1,1,0,0,0,0,0,0,1 },
	{ 1,0,0,0,0,0,0,0,0,1 },
	{ 1,0,1,0,0,1,1,1,0,1 },
	{ 1,0,1,0,0,1,1,1,0,1 },
	{ 1,0,0,0,0,0,0,0,0,1 },
	{ 1,1,1,1,1,1,1,1,1,1, },
};

struct Player player = {
	.position.x = 1.5f,
	.position.y = 1.5f,
	.rotation = 0.0f,
	.moveSpeed = 2.0f,
	.rotateSpeed = 90.0f,
	.colliderRadius = 0.2f,
};

// Custom logging function
void CustomLog(int msgType, const char* text, va_list args)
{
	char timeStr[64] = { 0 };
	time_t now = time(NULL);
	struct tm* tm_info = localtime(&now);

	strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
	printf("[%s] ", timeStr);

	switch (msgType)
	{
		case LOG_INFO: printf("[INFO] : "); break;
		case LOG_ERROR: printf("[ERROR]: "); break;
		case LOG_WARNING: printf("[WARN] : "); break;
		case LOG_DEBUG: printf("[DEBUG]: "); break;
		default: break;
	}

	vprintf(text, args);
	printf("\n");
}

void fillRays(struct MyRay rays[], const int length, Vector2 startPosition, float startAngle)
{
	const float rayDistance = 50.0f;
	for (int i = 0; i < length; i++)
	{
		rays[i].start = startPosition;
		rays[i].end = rotateAroundPoint(
			startPosition, 
			Vector2Add(startPosition, (Vector2) { rayDistance, 0.0f }), 
			(i - (length / 2)) + startAngle
		);
	}
}

void DDA(struct MyRay rays[], Vector2 position, float angle)
{
	//const float PROJECTION_PLANE_WIDTH = DRAW_DISTANCE * tanf(fov / 2) * 2;
	//TraceLog(LOG_INFO, "PROJECTION_PLANE_WIDTH = %f", PROJECTION_PLANE_WIDTH);
	//const float xProjPlane = (float)(((VIEWPORT_WIDTH * 2.0f) - VIEWPORT_WIDTH) / VIEWPORT_WIDTH) * (float)(PROJECTION_PLANE_WIDTH / 2.0f);
	
	const float angleStep = (float)FOV / (float)RAY_COUNT;

	for (int i = 0; i <= RAY_COUNT; i++)
	{
		rays[i].start = position;

		// This is slope (m = dx / dy), dx = cos(angle), dy = sin(angle)
		// First step is -45
		// Last step is 45
		// Middle step is 0
		// (i * step)
		float rayAngle = (i * angleStep) + angle - HALF_FOV;
		Vector2 forward = forwardVector(position, rayAngle);

		// Convert pixel coords into map grid coords
		int mapCol = position.x;
		int mapRow = position.y;

		// With slope we need to calculate the how far 1 step in either direction is
		// i.e. 
		// if dx = 1 then dy / dx = 1
		// By pythagoras dx/dy = sqrt[1^2 + (dy/dx)^2]
		// 1 unit in x = sqrt[1^2 + (dy/dx)^2]
		// 1 unit in y = sqrt[1^2 + (dx/dy)^2]
		Vector2 step = (Vector2){
			sqrtf(1 + ((forward.y / forward.x) * (forward.y / forward.x))),
			sqrtf(1 + ((forward.x / forward.y) * (forward.x / forward.y)))
		};
		// Step math simplified thanks to https://lodev.org/cgtutor/raycasting.html
		//Vector2 step = (Vector2){ abs(1.0f / forward.x), abs(1.0f / forward.y) };
		//if (forward.x == 0) { step.x = INFINITY; }
		//if (forward.y == 0) { step.y = INFINITY; }
		//TraceLog(LOG_INFO, "stepX = %f | stepY = %f", step.x, step.y);

		Vector2 rayLength = Vector2Zero();
		int dirX, dirY;	// Saves which direction we are moving (Up, Down, Left, Right)
		// Get length of ray to move 1 step along X-Axis
		if (forward.x < 0)
		{
			dirX = -1;
			rayLength.x = (position.x - (float)(mapCol)) * step.x;
		}
		else
		{
			dirX = 1;
			rayLength.x = ((float)(mapCol + 1) - position.x) * step.x;
		}
		// Get length of ray to move 1 step along Y-Axis
		if (forward.y < 0)
		{
			dirY = -1;
			rayLength.y = (position.y - (float)(mapRow)) * step.y;
		}
		else
		{
			dirY = 1;
			rayLength.y = ((float)(mapRow + 1) - position.y) * step.y;
		}

		bool hitWall = false;
		bool hitX = false;
		float distanceChecked = 0.0f;
		while (!hitWall && distanceChecked < DRAW_DISTANCE)
		{
			// Step along shortest length
			if (rayLength.x < rayLength.y)
			{
				mapCol += dirX;
				distanceChecked = rayLength.x;
				rayLength.x += step.x;
				hitX = true;
			}
			else
			{
				mapRow += dirY;
				distanceChecked = rayLength.y;
				rayLength.y += step.y;
				hitX = false;
			}

			hitWall = map[mapRow][mapCol] == 1;
		}

		// Save for rendering shadowed walls
		rays[i].hitX = hitX;
		if (hitX)
		{
			rays[i].distance = rayLength.x - step.x;
		}
		else
		{
			rays[i].distance = rayLength.y - step.y;
		}
		//rays[i].distance = (rayLength.x * cosf(rayAngle)) - (rayLength.y * sinf(rayAngle));

		rays[i].end = Vector2Add(position, Vector2Scale(forward, distanceChecked));

		// Draw Ray and collision point
		//DrawLine(rays[i].start.x * TILE_SIZE_PIXELS, rays[i].start.y * TILE_SIZE_PIXELS, rays[i].end.x * TILE_SIZE_PIXELS, rays[i].end.y * TILE_SIZE_PIXELS, PURPLE);
		//DrawCircle(rays[i].end.x * TILE_SIZE_PIXELS, rays[i].end.y * TILE_SIZE_PIXELS, 2.0f, PURPLE);
	}
}

void DDASingle(Vector2 position, float angle)
{
	struct MyRay ray;
	ray.start = position;

	// Need to calculate slope (dx / dy)
	Vector2 forward = forwardVector(position, angle);	// This is slope (m = dx / dy), dx = cos(angle), dy = sin(angle)
	//TraceLog(LOG_INFO, "Forward = %f | %f", forward.x, forward.y);

	// Convert pixel coords into map grid coords
	int mapCol = position.x;
	int mapRow = position.y;
	//TraceLog(LOG_INFO, "Map Coord: (%d,%d)", mapX, mapY);

	// With slope we need to calculate the how far 1 step in either direction is
	// i.e. 
	// if dx = 1 then dy / dx = 1
	// By pythagoras dx/dy = sqrt[1^2 + (dy/dx)^2]
	// 1 unit in x = sqrt[1^2 + (dy/dx)^2]
	// 1 unit in y = sqrt[1^2 + (dx/dy)^2]
	Vector2 step = (Vector2){
		sqrtf(1 + ((forward.y / forward.x) * (forward.y / forward.x))),
		sqrtf(1 + ((forward.x / forward.y) * (forward.x / forward.y)))
	};
	// Step math simplified thanks to https://lodev.org/cgtutor/raycasting.html
	//Vector2 step = (Vector2){ abs(1.0f / forward.x), abs(1.0f / forward.y) };
	//if (forward.x == 0) { step.x = INFINITY; }
	//if (forward.y == 0) { step.y = INFINITY; }
	//TraceLog(LOG_INFO, "stepX = %f | stepY = %f", step.x, step.y);

	Vector2 rayLength = Vector2Zero();
	int dirX, dirY;	// Saves which direction we are moving (Up, Down, Left, Right)
	// Get length of ray to move 1 step along X-Axis
	if (forward.x < 0) 
	{ 
		dirX = -1;
		rayLength.x = (position.x - (float)(mapCol)) * step.x;
	}
	else
	{
		dirX = 1;
		rayLength.x = ((float)(mapCol + 1) - position.x) * step.x;
	}
	// Get length of ray to move 1 step along Y-Axis
	if (forward.y < 0) 
	{ 
		dirY = -1; 
		rayLength.y = (position.y - (float)(mapRow)) * step.y;
	}
	else
	{
		dirY = 1;
		rayLength.y = ((float)(mapRow + 1) - position.y) * step.y;
	}
	//TraceLog(LOG_INFO, "rayLength = %f | %f", rayLength.x, rayLength.y);
	
	bool hitWall = false;
	const float maxDistance = 500.0f;	// Draw Distance
	float distanceChecked = 0.0f;
	//TraceLog(LOG_INFO, "");
	while (!hitWall && distanceChecked < maxDistance)
	{
		// Step along shortest length
		if (rayLength.x < rayLength.y)
		{
			mapCol += dirX;
			distanceChecked = rayLength.x;
			rayLength.x += step.x;
		}
		else
		{
			mapRow += dirY;
			distanceChecked = rayLength.y;
			rayLength.y += step.y;
		}
		ray.end = (Vector2){ position.x, position.y };
		ray.end = Vector2Add(ray.end, Vector2Scale(forward, distanceChecked));
		DrawCircle(ray.end.x * TILE_SIZE_PIXELS, ray.end.y * TILE_SIZE_PIXELS, 5.0f, PURPLE);

		hitWall = map[mapRow][mapCol] == 1;
	}

	ray.end = (Vector2){ position.x, position.y};
	ray.end = Vector2Add(ray.end, Vector2Scale(forward, distanceChecked));
	//ray.end = Vector2Scale(ray.end, TILE_SIZE_PIXELS);

	// Draw Ray and collision point
	DrawLine(ray.start.x * TILE_SIZE_PIXELS, ray.start.y * TILE_SIZE_PIXELS, ray.end.x * TILE_SIZE_PIXELS, ray.end.y * TILE_SIZE_PIXELS, PURPLE);
	DrawCircle(ray.end.x * TILE_SIZE_PIXELS, ray.end.y * TILE_SIZE_PIXELS, 5.0f, PURPLE);


	DrawText(
		TextFormat("Ray Start: (%f, %f)", ray.start.x, ray.start.y),
		500, 100, 20, WHITE
	);
	DrawText(
		TextFormat("Ray End: (%f, %f)", ray.end.x, ray.end.y),
		500, 125, 20, WHITE
	);
	//float expectedDistance = Vector2Distance(
	//	position,
	//	(Vector2) { 300.0f, 225.0f }
	//);
	//DrawText(TextFormat("Expected Distance = %f", expectedDistance), 450, 150, 20, WHITE);
	//DrawLine(ray.start.x, ray.start.y, 300.0f, 225.0f, RAYWHITE);
	//DrawCircle(300.0f, 225.0f, 5.0f, RAYWHITE);


	DrawText(TextFormat("Forward = (%f, %f)", forward.x, forward.y), 500, 175, 20, WHITE);
	DrawText(TextFormat("Direction = (%d, %d)", dirX, dirY), 500, 200, 20, WHITE);
	DrawText(TextFormat("Step = (%f, %f)", step.x, step.y), 500, 225, 20, WHITE);

	DrawText(TextFormat("End Map Coords = (%d, %d)", mapCol, mapRow), 500, 250, 20, WHITE);
	DrawText(TextFormat("Distance Checked = %f", distanceChecked), 500, 275, 20, WHITE);
	DrawText(TextFormat("Ray Length = (%f, %f)", rayLength.x, rayLength.y), 500, 300, 20, WHITE);
}

void DDANonLinear(struct MyRay rays[], Vector2 position, float angle)
{
	const int xPixelWidth = VIEWPORT_WIDTH / RAY_COUNT;
	const int halfRayCount = RAY_COUNT / 2;

	// Calculate angles
	for (int i = 0; i <= halfRayCount; i++)
	{
		float xScreen = i * xPixelWidth;
		float X_PROJECTION_PLANE = (((float)(xScreen * 2) - X_MAX) / X_MAX) * (PROJECTION_PLANE_HALF_WIDTH);
		float castAngle = atan2f(X_PROJECTION_PLANE, DRAW_DISTANCE);

		rays[i].castAngleRadians = castAngle;
		rays[RAY_COUNT - i].castAngleRadians = -castAngle;
	}

	for (int i = 0; i <= RAY_COUNT; i++)
	{
		rays[i].start = position;

		float angle = (rays[i].castAngleRadians * RAD2DEG) + player.rotation;
		Vector2 forward = forwardVector(position, angle);
		Vector2 step = (Vector2){
			sqrtf(1 + ((forward.y / forward.x) * (forward.y / forward.x))),
			sqrtf(1 + ((forward.x / forward.y) * (forward.x / forward.y)))
		};

		// Convert pixel coords into map grid coords
		int mapCol = position.x;
		int mapRow = position.y;

		Vector2 rayLength = Vector2Zero();
		int dirX, dirY;	// Saves which direction we are moving (Up, Down, Left, Right)
		// Get length of ray to move 1 step along X-Axis
		if (forward.x < 0)
		{
			dirX = -1;
			rayLength.x = (position.x - (float)(mapCol)) * step.x;
		}
		else
		{
			dirX = 1;
			rayLength.x = ((float)(mapCol + 1) - position.x) * step.x;
		}
		// Get length of ray to move 1 step along Y-Axis
		if (forward.y < 0)
		{
			dirY = -1;
			rayLength.y = (position.y - (float)(mapRow)) * step.y;
		}
		else
		{
			dirY = 1;
			rayLength.y = ((float)(mapRow + 1) - position.y) * step.y;
		}

		bool hitWall = false;
		bool hitX = false;
		float distanceChecked = 0.0f;
		while (!hitWall && distanceChecked < DRAW_DISTANCE)
		{
			// Step along shortest length
			if (rayLength.x < rayLength.y)
			{
				mapCol += dirX;
				distanceChecked = rayLength.x;
				rayLength.x += step.x;
				hitX = true;
			}
			else
			{
				mapRow += dirY;
				distanceChecked = rayLength.y;
				rayLength.y += step.y;
				hitX = false;
			}

			hitWall = map[mapRow][mapCol] == 1;
		}

		
		rays[i].end = Vector2Add(position, Vector2Scale(forward, distanceChecked));

		// Save for rendering shadowed walls
		rays[i].hitX = hitX;
		if (hitX)
		{
			rays[i].distance = (rayLength.x - step.x) * cosf(rays[i].castAngleRadians);
			rays[i].offset = fmod(rays[i].end.y, 1.0f);
		}
		else
		{
			rays[i].distance = (rayLength.y - step.y) * cosf(rays[i].castAngleRadians);
			rays[i].offset = fmod(rays[i].end.x, 1.0f);
		}
	}
}

void DrawDebug()
{
	Vector2 forward = forwardVector(player.position, player.rotation);
	// FPS & Frametime
	DrawText(TextFormat("FPS: %d", (int)(1 / GetFrameTime())), 5, 500, 20, WHITE);
	DrawText(TextFormat("Frametime: %.2fms", GetFrameTime() * 1000.0f), 5, 530, 20, WHITE);
	DrawText(TextFormat("Player Position: ( %f , %f )", player.position.x, player.position.y), 5, 560, 20, WHITE);
	DrawText(TextFormat("Player Rotation: %f", player.rotation), 5, 590, 20, WHITE);
	DrawText(TextFormat("Player Forward: ( %f , %f )", forward.x, forward.y), 5, 620, 20, WHITE);
}

void Draw2D(const struct MyRay rays[])
{
	// Draw Map
	for (int row = 0; row < MAP_LENGTH; row++)
	{
		for (int col = 0; col < MAP_LENGTH; col++)
		{
			if (map[row][col] == 1)
			{
				// Walls
				DrawRectangle(TILE_SIZE_PIXELS * col, TILE_SIZE_PIXELS * row, TILE_SIZE_PIXELS - 2, TILE_SIZE_PIXELS - 2, RED);
			}
			else
			{
				// Open Space
				DrawRectangle(TILE_SIZE_PIXELS * col, TILE_SIZE_PIXELS * row, TILE_SIZE_PIXELS - 2, TILE_SIZE_PIXELS - 2, BLUE);
			}
		}
	}

	for (int i = 0; i < RAY_COUNT; i++)
	{
		if (i >= (RAY_COUNT / 2) - 3 && i <= (RAY_COUNT / 2) + 3)
		{
			DrawLine(
				rays[i].start.x * TILE_SIZE_PIXELS,
				rays[i].start.y * TILE_SIZE_PIXELS,
				rays[i].end.x * TILE_SIZE_PIXELS,
				rays[i].end.y * TILE_SIZE_PIXELS,
				YELLOW
			);
		}
		else {
			DrawLine(
				rays[i].start.x * TILE_SIZE_PIXELS,
				rays[i].start.y * TILE_SIZE_PIXELS,
				rays[i].end.x * TILE_SIZE_PIXELS,
				rays[i].end.y * TILE_SIZE_PIXELS,
				PURPLE
			);
		}
	}
}

void DrawPlayer()
{
	// Draw Player
	DrawCircle(player.position.x * TILE_SIZE_PIXELS, player.position.y * TILE_SIZE_PIXELS, player.colliderRadius * TILE_SIZE_PIXELS, GREEN);
	Vector2 temp = forwardVector(player.position, player.rotation);
	temp = Vector2Scale(temp, 25.0f);
	temp = Vector2Add(temp, Vector2Scale(player.position, TILE_SIZE_PIXELS));
	DrawLine(player.position.x * TILE_SIZE_PIXELS, player.position.y * TILE_SIZE_PIXELS, temp.x, temp.y, GREEN);
}

void Draw3D(struct MyRay rays[], Texture tex)
{
	const float widthPercent = (float)COLUMN_PIXEL_WIDTH / (float)VIEWPORT_WIDTH;
	// Draw Ceiling
	DrawRectangle(SCREEN_WIDTH - VIEWPORT_WIDTH, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT / 2, LIGHTGRAY);
	// Draw Floor
	DrawRectangle(SCREEN_WIDTH - VIEWPORT_WIDTH, VIEWPORT_HEIGHT / 2, VIEWPORT_WIDTH, VIEWPORT_HEIGHT / 2, DARKGRAY);
	// Walls
	for (int i = 0; i <= RAY_COUNT; i++)
	{
		// Calculate the height based on distance from camera
		float height = (VIEWPORT_HEIGHT * HEIGHT_RATIO) / rays[i].distance;
		float heightPercent = 1.0f;

		float texOffset = (float)tex.height;
		float texStartOffset = 0.0f;
		/*TraceLog(LOG_INFO, "Height: %f | Radio: %f", height, height * HEIGHT_RATIO);*/
		// Clamp the height so we don't draw outside of the viewport
		if (height > VIEWPORT_HEIGHT) { 
			heightPercent = height / VIEWPORT_HEIGHT;
			 
			texStartOffset = 1.0f / heightPercent;
			texOffset *= texStartOffset;
			//TraceLog(LOG_INFO, "[%d] Percent pixels to render: %f", i, texStartOffset);
			//TraceLog(LOG_INFO, "[%d] Num pixels to render: %f", i, texOffset);
			texStartOffset = ((1.0f - texStartOffset) / 2.0f) * tex.height;
			//TraceLog(LOG_INFO, "[%d] texStartOffset: %f", i, texStartOffset);
			//TraceLog(LOG_INFO, "Height: %f | Height Percent: %f | Corrected Height: %f", height, heightPercent, height / heightPercent);
			
			//height = VIEWPORT_HEIGHT;

			// Keep height from exceeding height of viewport
			height /= heightPercent;
		}

		// Draw Wall
		//DrawRectangle(
		//	(SCREEN_WIDTH - VIEWPORT_WIDTH) + (i * width),
		//	(VIEWPORT_HEIGHT / 2) - (height / 2),
		//	width,
		//	height,
		//	wallColor
		//);
			
		Color wallColor = DARKGRAY;
		// Shade walls darker if they are perpedicular
		if (rays[i].hitX) {
			wallColor = WHITE;
		}
		// Scale for brightness, lower number reduces amount of "light" emitted by player
		const float brightnessScaler = 4.0f;
		float brightness = brightnessScaler / rays[i].distance;
		if (brightness > 1.0f) { brightness = 1.0f; }
		wallColor.r *= brightness;
		wallColor.g *= brightness;
		wallColor.b *= brightness;

		Rectangle texCoords = (Rectangle){
			rays[i].offset * tex.width,
			texStartOffset,
			widthPercent * tex.width,
			texOffset,
		};
		Rectangle position = (Rectangle){
			(SCREEN_WIDTH - VIEWPORT_WIDTH) + (i * COLUMN_PIXEL_WIDTH),
			(VIEWPORT_HEIGHT / 2) - (height / 2),
			COLUMN_PIXEL_WIDTH,
			height,
		};
		DrawTexturePro(
			tex,
			texCoords,
			position,
			Vector2Zero(),
			0.0f,
			wallColor
		);
		


		//DrawTextureRec(tex, rec, (Vector2) { rec.x, rec.y }, wallColor);
		
		//rlSetTexture(tex.id);
		////rlSetTexture(GetShapesTexture().id);
		//rlBegin(RL_QUADS);

		//	rlColor4ub(wallColor.r, wallColor.g, wallColor.b, wallColor.a);
		//	rlNormal3f(0.0f, 0.0f, 1.0f);	// Normal vector pointing towards viewer

		//	// Top Left
		//	rlVertex2f(rec.x, rec.y);
		//	rlTexCoord2f(texRec.x, texRec.y + texRec.height);
		//	// Bottom Left
		//	rlVertex2f(rec.x, rec.y + rec.height);
		//	rlTexCoord2f(texRec.x, texRec.y);
		//	// Bottom Right
		//	rlVertex2f(rec.x + rec.width, rec.y + rec.height);
		//	rlTexCoord2f(texRec.x + texRec.width, texRec.y);
		//	// Top Right
		//	rlVertex2f(rec.x + rec.width, rec.y);
		//	rlTexCoord2f(texRec.x + texRec.width, texRec.y + texRec.height);

		//rlEnd();
		//rlSetTexture(0);
	}
}

bool CanMove(Vector2 position)
{
	bool hitDown = false;
	bool hitLeft = false;
	bool hitRight = false;
	bool hitUp = false;
	Rectangle wall;

	if (map[(int)position.y][(int)position.x + 1] == 1)
	{
		wall = (Rectangle){(int)position.x + 1, (int)position.y, 1.0f, 1.0f };
		DrawRectangle(wall.x * TILE_SIZE_PIXELS, wall.y * TILE_SIZE_PIXELS, wall.width * TILE_SIZE_PIXELS, wall.height * TILE_SIZE_PIXELS, ORANGE);
		hitRight = CheckCollisionCircleRec(position, player.colliderRadius, wall);
	}
	if (map[(int)position.y][(int)position.x - 1] == 1)
	{
		wall = (Rectangle){ (int)position.x - 1, (int)position.y, 1.0f, 1.0f };
		DrawRectangle(wall.x * TILE_SIZE_PIXELS, wall.y * TILE_SIZE_PIXELS, wall.width * TILE_SIZE_PIXELS, wall.height * TILE_SIZE_PIXELS, ORANGE);
		hitLeft = CheckCollisionCircleRec(position, player.colliderRadius, wall);
	}
	if (map[(int)position.y + 1][(int)position.x] == 1)
	{
		wall = (Rectangle){ (int)position.x, (int)position.y + 1, 1.0f, 1.0f };
		DrawRectangle(wall.x * TILE_SIZE_PIXELS, wall.y * TILE_SIZE_PIXELS, wall.width * TILE_SIZE_PIXELS, wall.height * TILE_SIZE_PIXELS, ORANGE);
		hitDown = CheckCollisionCircleRec(position, player.colliderRadius, wall);
	}
	if (map[(int)position.y - 1][(int)position.x] == 1)
	{
		wall = (Rectangle){ (int)position.x, (int)position.y - 1, 1.0f, 1.0f };
		DrawRectangle(wall.x * TILE_SIZE_PIXELS, wall.y * TILE_SIZE_PIXELS, wall.width * TILE_SIZE_PIXELS, wall.height * TILE_SIZE_PIXELS, ORANGE);
		hitUp = CheckCollisionCircleRec(position, player.colliderRadius, wall);
	}

	return !hitDown && !hitLeft && !hitRight && !hitUp;
}

void handleInput()
{
	if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
	{
		player.rotation -= player.rotateSpeed * GetFrameTime();
		if (player.rotation > 360.0f) { player.rotation -= 360.0f; }
		if (player.rotation < 0.0f) { player.rotation += 360.0f; }
	}
	else if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
	{
		player.rotation += player.rotateSpeed * GetFrameTime();
		if (player.rotation > 360.0f) { player.rotation -= 360.0f; }
		if (player.rotation < 0.0f) { player.rotation += 360.0f; }
	}

	if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
	{
		Vector2 newPosition = Vector2Add(
			Vector2Scale(
				forwardVector(player.position, player.rotation),
				player.moveSpeed * GetFrameTime()
			),
			player.position
		);
		if (CanMove(newPosition)) { player.position = newPosition; }
	}
	else if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
	{
		Vector2 newPosition = Vector2Add(
			Vector2Scale(
				forwardVector(player.position, player.rotation),
				-player.moveSpeed * GetFrameTime()
			),
			player.position
		);
		if (CanMove(newPosition)) { player.position = newPosition; }
	}
}

int main ()
{
	// Tell the window to use vysnc and work on high DPI displays
	SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
	//SetConfigFlags(FLAG_VSYNC_HINT);

	//SetTraceLogCallback(CustomLog);
	SetTraceLogLevel(LOG_ALL);

	// Create the window and OpenGL context
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "MEngine92");

	// Utility function from resource_dir.h to find the resources folder and set it as the current working directory so we can load from it
	SearchAndSetResourceDir("resources");

	// Load a texture from the resources directory
	Texture wabbit = LoadTexture("wabbit_alpha.png");
	Texture checkerboard = LoadTexture("checkerboard.png");
	Texture checkerboard2 = LoadTexture("checkerboard2.png");
	Texture checkerboard64 = LoadTexture("checkerboard64.png");
	Texture greyBrick = LoadTexture("grey_brick_32.png");
	Texture redBrick = LoadTexture("red_brick.png");
	Texture metal = LoadTexture("metal.png");
	Texture texCoordsDebug = LoadTexture("tex_coords.png");

	struct MyRay rays[RAY_COUNT + 1];

	// game loop
	while (!WindowShouldClose())		// run the loop untill the user presses ESCAPE or presses the Close button on the window
	{
		// drawing
		BeginDrawing();

		// Setup the backbuffer for drawing (clear color and depth buffers)
		ClearBackground(BLACK);

		// draw some text using the default font
		//DrawText("Hello Raylib", 200,200,20,WHITE);

		// draw our texture to the screen
		//DrawTexture(wabbit, 400, 200, WHITE);

		Draw2D(rays);
		handleInput();
		//DDA(rays, player.position, player.rotation);
		DDANonLinear(rays, player.position, player.rotation);
		DrawPlayer();
		Draw3D(rays, checkerboard64);
		DrawDebug();
		
		//DrawTexture(checkerboard64, 50, 50, WHITE);

		//Rectangle texCoords = (Rectangle){
		//	0.0f,
		//	0.0f,
		//	326.0f,
		//	254.0f,
		//};
		//Rectangle position = (Rectangle){
		//	0.0f,
		//	500.0f,
		//	326.0f,
		//	254.0f,
		//};
		//DrawTexturePro(
		//	texCoordsDebug,
		//	texCoords,
		//	position,
		//	Vector2Zero(),
		//	0.0f,
		//	WHITE
		//);
		//texCoords.x = 200.0f;
		//texCoords.width = 32;
		//position.x = 200.0f;
		//position.width = 32;
		//DrawTexturePro(
		//	texCoordsDebug,
		//	texCoords,
		//	position,
		//	Vector2Zero(),
		//	0.0f,
		//	GREEN
		//);

		//Vector2 scale = (Vector2){ 5.0f, 5.0f };
		//texCoords.x = 0;
		//texCoords.y = 0;
		//texCoords.width = 32;
		//texCoords.height = 32;
		//position.x = 400;
		//position.y = 600;
		//position.width = 32 * scale.x;
		//position.height = 32 * scale.y;
		//DrawTexturePro(
		//	greyBrick,
		//	texCoords,
		//	position,
		//	(Vector2) { 0.0f, 0.0f },
		//	0.0f,
		//	WHITE
		//);

		//texCoords.x = 0;
		//texCoords.y = 12;
		//texCoords.width = 32.0f;
		//texCoords.height = 6;
		//position.x = 560;
		//position.y = 640;
		//position.width = 160;
		//position.height = 80;
		//DrawTexturePro(
		//	greyBrick,
		//	texCoords,
		//	position,
		//	(Vector2) {0.0f, 0.0f},
		//	0.0f,
		//	GREEN
		//);

		//texCoords.x = 16;
		//texCoords.y = 16;
		//texCoords.width = 16;
		//texCoords.height = 16;
		//position.x = 560;
		//position.y = 600;
		//position.width = 160;
		//position.height = 160;
		//DrawTexturePro(
		//	greyBrick,
		//	texCoords,
		//	position,
		//	(Vector2) {0.5f, 0.5f},
		//	0.0f,
		//	MAGENTA
		//);


		//rlSetTexture(checkerboard64.id);
		////rlSetTexture(GetShapesTexture().id);
		//rlBegin(RL_QUADS);

		//	rlColor4ub(WHITE.r, WHITE.g, WHITE.b, WHITE.a);
		//	rlNormal3f(0.0f, 0.0f, 1.0f);	// Normal vector pointing towards viewer

		//	// Top Left
		//	rlVertex2f(50, 500);
		//	// Bottom Left
		//	rlVertex2f(50, 564);
		//	// Bottom Right
		//	rlVertex2f(114, 564);
		//	// Top Right
		//	rlVertex2f(114, 500);

		//	rlTexCoord2f(0, 0);
		//	rlTexCoord2f(1, 0);
		//	rlTexCoord2f(1, 1);
		//	rlTexCoord2f(0, 1);

		//rlEnd();
		//rlSetTexture(0);

		//DDASingle(player.position, player.rotation);

		// end the frame and get ready for the next one  (display frame, poll input, etc...)
		EndDrawing();
	}

	// cleanup
	// unload our texture so it can be cleaned up
	UnloadTexture(wabbit);
	UnloadTexture(checkerboard);
	UnloadTexture(checkerboard2);
	UnloadTexture(greyBrick);
	UnloadTexture(redBrick);
	UnloadTexture(metal);
	UnloadTexture(texCoordsDebug);

	// destory the window and cleanup the OpenGL context
	CloseWindow();
	return 0;
}