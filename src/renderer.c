#include "renderer.h"
#include "resource_dir.h"
#include "helpful_math.h"

#define MIN_SCREEN_WIDTH 640
#define MIN_SCREEN_HEIGHT 480
#define VIEWPORT_WIDTH 640
#define VIEWPORT_HEIGHT 480
#define DRAW_DISTANCE 20
#define X_MAX (VIEWPORT_WIDTH - 1)

static Renderer renderer;
static enum DrawMode drawMode = GAME;
static enum ShadingMdoe shadingMode = TEXTURED;
static enum RenderQuality renderQuality = ULTRA;
// Auto fill with largest amount, can't resize smaller in C without too much dynamic allocation overhead for array this small.
// Basically fill it with the width of the game viewport and add 1
static struct RayData rays[VIEWPORT_WIDTH + 1];
static int map[10][10];

static unsigned int horizontal_fov;
static unsigned int half_fov;
static float vertical_fov;
static unsigned int tile_size_pixels = VIEWPORT_HEIGHT / 10;
// Fisheye Correction Stuff
static float projection_plane_width;
static float projection_plane_half_width;
static float project_plane_height;
static float height_ratio;
static float half_wall_height;

void CreateRenderer(bool fullscreen, bool vsync, unsigned int screenWidth, unsigned int screenHeight, unsigned int fov, unsigned int mapData[10][10])
{
	UpdateRenderingSettings(fullscreen, vsync, screenWidth, screenHeight, fov);
	UpdateRendererMapData(mapData);

	// Utility function from resource_dir.h to find the resources folder and set it as the current working directory so we can load from it
	SearchAndSetResourceDir("resources");
	LoadTextures();

	renderer.column_pixel_width = 1;
	renderer.ray_count = VIEWPORT_WIDTH;
	UpdateRenderCamera((Vector2) { 1.5, 1.5 }, 0.0);
}

void UpdateRendererMapData(unsigned int mapData[10][10])
{
	// Copy over each value from new map data
	for (int row = 0; row < 10; row++)
	{
		for (int col = 0; col < 10; col++)
		{
			map[row][col] = mapData[row][col];
		}
	}
}

void UpdateRenderingSettings(bool fullscreen, bool vsync, unsigned int screenWidth, unsigned int screenHeight, unsigned int fov)
{
	// Set the proper flags for the window based on settings
	int flags = FLAG_WINDOW_MAXIMIZED | FLAG_WINDOW_RESIZABLE;
	if (fullscreen)
	{
		flags |= FLAG_FULLSCREEN_MODE;
	}
	if (vsync)
	{
		flags |= FLAG_VSYNC_HINT;
	}
	SetConfigFlags(flags);

	// Create the window and OpenGL context
	InitWindow(screenWidth, screenHeight, "MEngine92");
	SetWindowMinSize(MIN_SCREEN_WIDTH, MIN_SCREEN_HEIGHT);

	// Render texture initialization, used to hold the rendering result so we can easily resize it
	renderer.renderTex = LoadRenderTexture(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
	// Texture scale filter to use
	SetTextureFilter(renderer.renderTex.texture, TEXTURE_FILTER_POINT);

	// Calculate all render related constants
	horizontal_fov = fov;
	half_fov = horizontal_fov / 2;
	vertical_fov = 2.0 * atanf(tanf(half_fov) * (float)(VIEWPORT_WIDTH / VIEWPORT_HEIGHT));
	projection_plane_width = (float)DRAW_DISTANCE * tanf(DEG2RAD * half_fov) * 2.0;
	projection_plane_half_width = projection_plane_width / 2.0;
	height_ratio = ((float)VIEWPORT_HEIGHT / (float)VIEWPORT_WIDTH) / ((float)horizontal_fov / 90.0);
	project_plane_height = (float)DRAW_DISTANCE * tanf(vertical_fov / 2.0);
	half_wall_height = 5;
}

/*
 * Handles all input from keyboard.
 * TAB, R, T are used for debug functions such as switching draw modes, render resolution and shading.
 */
void RendererInput()
{
	// Toggle between Auto Map and Game View
	if (IsKeyPressed(KEY_TAB))
	{
		switch (drawMode)
		{
		case GAME:
			UpdateDrawMode(GAME_DEBUG);
			break;
		case GAME_DEBUG:
			UpdateDrawMode(MAP);
			break;
		case MAP:
			UpdateDrawMode(MAP_DEBUG);
			break;
		case MAP_DEBUG:
			UpdateDrawMode(GAME);
			break;
		}
	}
	// Cycle through render resolution (ray count)
	if (IsKeyPressed(KEY_R))
	{
		switch (renderQuality)
		{
		case VERY_LOW:
			UpdateRenderQuality(LOW);
			break;
		case LOW:
			UpdateRenderQuality(MEDIUM);
			break;
		case MEDIUM:
			UpdateRenderQuality(HIGH);
			break;
		case HIGH:
			UpdateRenderQuality(ULTRA);
			break;
		case ULTRA:
			UpdateRenderQuality(VERY_LOW);
			break;
		}
	}
	// Toggle between shading modes (texture/flat)
	if (IsKeyPressed(KEY_T))
	{
		switch (shadingMode)
		{
		case TEXTURED:
			UpdateShadingMode(FLAT);
			break;
		case FLAT:
			UpdateShadingMode(TEXTURED);
			break;
		}
	}
}

void LoadTextures()
{
	renderer.textures[0] = LoadTexture("wabbit_alpha.png");
	renderer.textures[1] = LoadTexture("checkerboard.png");
	renderer.textures[2] = LoadTexture("checkerboard2.png");
	renderer.textures[3] = LoadTexture("checkerboard64.png");
	renderer.textures[4] = LoadTexture("grey_brick_32.png");
	renderer.textures[5] = LoadTexture("red_brick.png");
	renderer.textures[6] = LoadTexture("metal.png");
	renderer.textures[7] = LoadTexture("tex_coords.png");
}

void UnloadTextures()
{
	// Unload render texture
	UnloadRenderTexture(renderer.renderTex);
	// Unload remaining textures
	for (int i = 0; i < sizeof(renderer.textures) / sizeof(renderer.textures[0]); i++)
	{
		UnloadTexture(renderer.textures[i]);
	}
}

void UpdateFrameBuffer()
{
	// Compute required framebuffer scaling
	renderer.renderScale = MIN((float)GetScreenWidth() / VIEWPORT_WIDTH, (float)GetScreenHeight() / VIEWPORT_HEIGHT);

	// Update virtual mouse (clamped mouse value behind game screen)
	Vector2 mouse = GetMousePosition();
	renderer.virtualMouse.x = (mouse.x - (GetScreenWidth() - (VIEWPORT_WIDTH * renderer.renderScale)) * 0.5f) / renderer.renderScale;
	renderer.virtualMouse.y = (mouse.y - (GetScreenHeight() - (VIEWPORT_HEIGHT * renderer.renderScale)) * 0.5f) / renderer.renderScale;
	renderer.virtualMouse = Vector2Clamp(
		renderer.virtualMouse, 
		(Vector2) { 0, 0 }, 
		(Vector2) { (float)VIEWPORT_WIDTH, (float)VIEWPORT_HEIGHT }
	);

	// Draw everything in the render texture, note this will not be rendered on screen, yet
	BeginTextureMode(renderer.renderTex);
		// Setup the backbuffer for drawing (clear color and depth buffers)
		ClearBackground(BLACK);

		DDANonLinear(rays, renderer.cameraPosition, renderer.cameraRotation);
		if (drawMode == GAME || drawMode == GAME_DEBUG)
		{
			Draw3D(rays, renderer.textures[3]);
		}
		else if (drawMode == MAP || drawMode == MAP_DEBUG)
		{
			Draw2D(rays);
		}

		if (drawMode == GAME_DEBUG || drawMode == MAP_DEBUG) { DrawDebug(); }

		//DrawText(TextFormat("Default Mouse: [%i , %i]", (int)mouse.x, (int)mouse.y), 350, 25, 20, GREEN);
		//DrawText(TextFormat("Virtual Mouse: [%i , %i]", (int)virtualMouse.x, (int)virtualMouse.y), 350, 55, 20, YELLOW);
	EndTextureMode();
}

void UpdateScreen()
{
	BeginDrawing();
		// Clear screen background
		ClearBackground(BLACK);
		// Draw render texture to screen, properly scaled
		DrawTexturePro(
			renderer.renderTex.texture,
			(Rectangle) {
				0.0f,
				0.0f,
				(float)renderer.renderTex.texture.width,
				(float)-renderer.renderTex.texture.height
			},
			(Rectangle) {
				(GetScreenWidth() - ((float)VIEWPORT_WIDTH * renderer.renderScale)) * 0.5f,
				(GetScreenHeight() - ((float)VIEWPORT_HEIGHT * renderer.renderScale)) * 0.5f,
				(float)VIEWPORT_WIDTH * renderer.renderScale,
				(float)VIEWPORT_HEIGHT * renderer.renderScale
			},
			(Vector2) { 0, 0 },
			0.0f,
			WHITE
		);
	// end the frame and get ready for the next one  (display frame, poll input, etc...)
	EndDrawing();
}

void UpdateRenderCamera(Vector2 position, float rotation)
{
	renderer.cameraPosition = position;
	renderer.cameraRotation = rotation;
	renderer.cameraForward = Vector2Forward(rotation);
}

void UpdateDrawMode(DrawMode newDrawMode) { drawMode = newDrawMode; }

void UpdateShadingMode(ShadingMode newShadingMode) { shadingMode = newShadingMode; }

void UpdateRenderQuality(RenderQuality newRenderQuality)
{
	renderQuality = newRenderQuality;
	switch (renderQuality)
	{
	case VERY_LOW:
		renderer.column_pixel_width = 5;
		break;
	case LOW:
		renderer.column_pixel_width = 4;
		break;
	case MEDIUM:
		renderQuality = HIGH;
		renderer.column_pixel_width = 2;
		break;
	case HIGH:
		renderer.column_pixel_width = 1;
		break;
	case ULTRA:
		renderer.column_pixel_width = 8;
		break;
	}
	renderer.ray_count = VIEWPORT_WIDTH / renderer.column_pixel_width;
}

/*
 * Standard DDA algorithm that uses fixed angle step for casting each ray. As a result, this does
 * produce the "fisheye" distortion that can be corrected through cos().  However, this distortion
 * can only be corrected for an FOV of around 75 degrees.  Anything above this you start to see
 * a reverse fisheye distortion around the edges of the screen.
 */
void DDA(struct RayData rays[], Vector2 position, float angle)
{
	const float angleStep = (float)horizontal_fov / (float)renderer.ray_count;

	for (int i = 0; i <= renderer.ray_count; i++)
	{
		rays[i].start = position;

		// This is slope (m = dx / dy), dx = cos(angle), dy = sin(angle)
		// First step is -45
		// Last step is 45
		// Middle step is 0
		// (i * step)
		float rayAngle = (i * angleStep) + angle - half_fov;
		Vector2 forward = Vector2Forward(rayAngle);

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

		rays[i].end = Vector2Add(position, Vector2Scale(forward, distanceChecked));

		// Draw Ray and collision point
		//DrawLine(rays[i].start.x * tile_size_pixels, rays[i].start.y * tile_size_pixels, rays[i].end.x * tile_size_pixels, rays[i].end.y * tile_size_pixels, PURPLE);
		//DrawCircle(rays[i].end.x * tile_size_pixels, rays[i].end.y * tile_size_pixels, 2.0f, PURPLE);
	}
}

/*
 * DEPRECATED
 * Mostly used for debugging. Fires a single ray ddirectly if front of the user and draws each
 * step until it hits the end.
 */
void DDASingle(Vector2 position, float angle)
{
	struct RayData ray;
	ray.start = position;

	Vector2 forward = Vector2Forward(angle);

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
		DrawCircle(ray.end.x * tile_size_pixels, ray.end.y * tile_size_pixels, 5.0f, PURPLE);

		hitWall = map[mapRow][mapCol] == 1;
	}

	ray.end = (Vector2){ position.x, position.y };
	ray.end = Vector2Add(ray.end, Vector2Scale(forward, distanceChecked));

	// Draw Ray and collision point
	DrawLine(ray.start.x * tile_size_pixels, ray.start.y * tile_size_pixels, ray.end.x * tile_size_pixels, ray.end.y * tile_size_pixels, PURPLE);
	DrawCircle(ray.end.x * tile_size_pixels, ray.end.y * tile_size_pixels, 5.0f, PURPLE);

	DrawText(
		TextFormat("Ray Start: (%f, %f)", ray.start.x, ray.start.y),
		500, 100, 20, WHITE
	);
	DrawText(
		TextFormat("Ray End: (%f, %f)", ray.end.x, ray.end.y),
		500, 125, 20, WHITE
	);


	DrawText(TextFormat("Forward = (%f, %f)", forward.x, forward.y), 500, 175, 20, WHITE);
	DrawText(TextFormat("Direction = (%d, %d)", dirX, dirY), 500, 200, 20, WHITE);
	DrawText(TextFormat("Step = (%f, %f)", step.x, step.y), 500, 225, 20, WHITE);

	DrawText(TextFormat("End Map Coords = (%d, %d)", mapCol, mapRow), 500, 250, 20, WHITE);
	DrawText(TextFormat("Distance Checked = %f", distanceChecked), 500, 275, 20, WHITE);
	DrawText(TextFormat("Ray Length = (%f, %f)", rayLength.x, rayLength.y), 500, 300, 20, WHITE);
}

/*
 * DDA using a non-linear angle step for casting each ray. The math for calculating the angles and
 * distance can be found at https://www.scottsmitelli.com/articles/we-can-fix-your-raycaster/.
 */
void DDANonLinear(struct RayData rays[], Vector2 position, float angle)
{
	const unsigned int xPixelWidth = VIEWPORT_WIDTH / renderer.ray_count;
	const unsigned int half_ray_count = renderer.ray_count / 2;

	// Calculate angles
	for (int i = 0; i <= half_ray_count; i++)
	{
		float xScreen = i * xPixelWidth;
		float X_PROJECTION_PLANE = (((float)(xScreen * 2) - X_MAX) / X_MAX) * (projection_plane_half_width);
		float castAngle = atan2f(X_PROJECTION_PLANE, DRAW_DISTANCE);

		rays[i].castAngleRadians = castAngle;
		rays[renderer.ray_count - i].castAngleRadians = -castAngle;
	}

	// Cast the rays
	for (int i = 0; i <= renderer.ray_count; i++)
	{
		rays[i].start = position;

		float angle = (rays[i].castAngleRadians * RAD2DEG) + renderer.cameraRotation;
		Vector2 forward = Vector2Forward(angle);
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
		// Choose which distance and offset value to store based on if we hit horizontal or vertical wall
		if (hitX)	// Horizontal wall hit
		{
			rays[i].distance = (rayLength.x - step.x) * cosf(rays[i].castAngleRadians);
			rays[i].offset = fmod(rays[i].end.y, 1.0f);
		}
		else       // Vertical wall hit
		{
			rays[i].distance = (rayLength.y - step.y) * cosf(rays[i].castAngleRadians);
			rays[i].offset = fmod(rays[i].end.x, 1.0f);
		};
	}
}

/*
 * DDA using a non-linear angle step for casting each ray. The math for calculating the angles and
 * distance can be found at https://www.scottsmitelli.com/articles/we-can-fix-your-raycaster/.
 */
void DrawDebug()
{
	// FPS & Frametime
	DrawText(TextFormat("FPS: %d", (int)(1 / GetFrameTime())), 0, 0, 20, WHITE);
	DrawText(TextFormat("Frametime: %.2fms", GetFrameTime() * 1000.0f), 0, 20, 20, WHITE);
	DrawText(TextFormat("Draw Mode: %d", drawMode), 0, 40, 20, WHITE);
	DrawText(TextFormat("Scale: %f", renderer.renderScale), 0, 60, 20, WHITE);
	DrawText(TextFormat("Screen: ( %d , %d )", GetScreenWidth(), GetScreenHeight()), 0, 80, 20, WHITE);
	//DrawText(TextFormat("Render: ( %d , %d )", GetRenderWidth(), GetRenderHeight()), 0, 140, 20, WHITE);
	//DrawText(TextFormat("Player Position: ( %f , %f )", player.position.x, player.position.y), 0, 40, 20, WHITE);
	//DrawText(TextFormat("Player Rotation: %f", player.rotation), 0, 60, 20, WHITE);
	//DrawText(TextFormat("Player Forward: ( %f , %f )", forward.x, forward.y), 0, 80, 20, WHITE);
}

/*
 * Draws the 2D version of the map. Usefule as a type of "automap" and useful for debugging.
 */
void Draw2D(const struct RayData rays[])
{
	// Draw Map
	for (int row = 0; row < 10; row++)
	{
		for (int col = 0; col < 10; col++)
		{
			if (map[row][col] == 1)
			{
				// Walls
				DrawRectangle(tile_size_pixels * col, tile_size_pixels * row, tile_size_pixels - 2, tile_size_pixels - 2, RED);
			}
			else
			{
				// Open Space
				DrawRectangle(tile_size_pixels * col, tile_size_pixels * row, tile_size_pixels - 2, tile_size_pixels - 2, BLUE);
			}
		}
	}

	for (int i = 0; i <= renderer.ray_count; i++)
	{
		if (i >= (renderer.ray_count / 2) - 3 && i <= (renderer.ray_count / 2) + 3)
		{
			DrawLine(
				rays[i].start.x * tile_size_pixels,
				rays[i].start.y * tile_size_pixels,
				rays[i].end.x * tile_size_pixels,
				rays[i].end.y * tile_size_pixels,
				YELLOW
			);
		}
		else {
			DrawLine(
				rays[i].start.x * tile_size_pixels,
				rays[i].start.y * tile_size_pixels,
				rays[i].end.x * tile_size_pixels,
				rays[i].end.y * tile_size_pixels,
				PURPLE
			);
		}
	}

	// Draw Player
	DrawCircle(renderer.cameraPosition.x * tile_size_pixels, renderer.cameraPosition.y * tile_size_pixels, 0.2 * tile_size_pixels, GREEN);
	Vector2 temp = renderer.cameraForward;
	temp = Vector2Scale(temp, 25.0f);
	temp = Vector2Add(temp, Vector2Scale(renderer.cameraPosition, tile_size_pixels));
	DrawLine(renderer.cameraPosition.x * tile_size_pixels, renderer.cameraPosition.y * tile_size_pixels, temp.x, temp.y, GREEN);
}

/*
 * Draws the 3D version of the map. Takes an array of rays that have been filled by DDANonLinear().
 * Also takes in a texture to draw on the walls. Will update this later to look at map data for
 * the texture to use instead of a single texture. Draws Ceiling and Floor first.  Next goes
 * through the ray data and draws each column at a fixed width and adjsuts the height based on
 * distance from the Player.
 */
void Draw3D(struct RayData rays[], Texture2D tex)
{
	const float widthPercent = (float)renderer.column_pixel_width / (float)VIEWPORT_WIDTH;
	// Draw Ceiling
	DrawRectangle(0, 0, VIEWPORT_WIDTH, VIEWPORT_HEIGHT / 2, LIGHTGRAY);
	// Draw Floor
	DrawRectangle(0, VIEWPORT_HEIGHT / 2, VIEWPORT_WIDTH, VIEWPORT_HEIGHT / 2, DARKGRAY);
	// Walls
	for (int i = 0; i <= renderer.ray_count; i++)
	{
		// Calculate the height based on distance from camera
		float height = (VIEWPORT_HEIGHT * height_ratio) / rays[i].distance;
		float heightPercent = 1.0f;

		float texOffset = (float)tex.height;
		float texStartOffset = 0.0f;
		/*TraceLog(LOG_INFO, "Height: %f | Radio: %f", height, height * HEIGHT_RATIO);*/
		// Clamp the height so we don't draw outside of the viewport
		if (height > VIEWPORT_HEIGHT) {
			heightPercent = height / VIEWPORT_HEIGHT;

			texStartOffset = 1.0f / heightPercent;
			texOffset *= texStartOffset;
			texStartOffset = ((1.0f - texStartOffset) / 2.0f) * tex.height;

			// Keep height from exceeding height of viewport
			height /= heightPercent;
		}

		/*TraceLog(LOG_INFO, "height: %f", height);*/

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

		if (shadingMode == TEXTURED)
		{
			// Draw Wall (Textured)
			Rectangle texCoords = (Rectangle){
				rays[i].offset * tex.width,
				texStartOffset,
				widthPercent * tex.width,
				texOffset,
			};
			Rectangle position = (Rectangle){
				i * renderer.column_pixel_width,
				(VIEWPORT_HEIGHT / 2) - (height / 2),
				renderer.column_pixel_width,
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
		}
		else if (shadingMode == FLAT)
		{
			wallColor = RED;
			// Shade walls darker if they are perpedicular
			if (!rays[i].hitX) {
				wallColor.r *= 0.5f;
				wallColor.g *= 0.5f;
				wallColor.b *= 0.5f;
			}
			// Draw Wall (Flat Shaded)
			DrawRectangle(
				i * renderer.column_pixel_width,
				(VIEWPORT_HEIGHT / 2) - (height / 2),
				renderer.column_pixel_width,
				height,
				wallColor
			);
		}
	}
}