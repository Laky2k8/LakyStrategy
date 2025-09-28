#include "raylib.h"
#include <string>
#include "map_engine.hpp"

#define TITLE "LakyStrategy"
#define VERSION_NUM "0.0.1"
#define LAKYSTRATEGY_ERROR "LakyStrategy::Error: "

int main() 
{
	const int screenWidth = 1280;
	const int screenHeight = 720;
	InitWindow(screenWidth, screenHeight, (string(TITLE) + " " + string(VERSION_NUM)).c_str());
	SetTargetFPS(144);

	// --- Load provinces ---

	MapEngine mapEngine(screenWidth, screenHeight);
	string map_json = "./assets/map_full.geojson";

	// Loading screen :]
	BeginDrawing();
	ClearBackground(BLACK);
	DrawText("Loading map...", (screenWidth - MeasureText("Loading map...", 20)) / 2, screenHeight / 2, 20, WHITE);
	EndDrawing();

	if(!mapEngine.LoadMap(map_json))
	{
		cerr << LAKYSTRATEGY_ERROR << "Failed to load map data!" << endl;
		CloseWindow();
		return 1;
	}
	// ----------------------

	Camera2D camera = { 0 };
	camera.target = (Vector2){ 0, 0 };  // Start at world origin, not screen center
	camera.offset = (Vector2){ screenWidth/2.0f, screenHeight/2.0f };  // Center the view
	camera.rotation = 0.0f;
	camera.zoom = 1.0f;

	string hoveredRegion = "";

	while (!WindowShouldClose())
	{

		// FPS counter in title
		SetWindowTitle((string(TITLE) + " " + string(VERSION_NUM) + " - " + to_string(GetFPS()) + " FPS").c_str());

		// Update
		
		// Handle zoom with mouse wheel
		camera.zoom = expf(logf(camera.zoom) + ((float)GetMouseWheelMove()*0.1f));

		if (camera.zoom > 10.0f) camera.zoom = 10.0f;
		else if (camera.zoom < 0.1f) camera.zoom = 0.1f;

		// Handle panning with dragging
		if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
			Vector2 delta = GetMouseDelta();
			camera.target.x -= delta.x / camera.zoom;
			camera.target.y -= delta.y / camera.zoom;
		}

		// Get hovered region
		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			Vector2 mousePos = GetMousePosition();
			Vector2 worldPos = {
				(mousePos.x - camera.offset.x) / camera.zoom + camera.target.x,
				(mousePos.y - camera.offset.y) / camera.zoom + camera.target.y
			};

			hoveredRegion = mapEngine.getProvinceAt((int)worldPos.x, (int)worldPos.y);
        }


		BeginDrawing();
		ClearBackground(DARKBLUE);

		BeginMode2D(camera);

        mapEngine.render();

		EndMode2D();

		DrawText(("NUTS 3 Regions: " + to_string(mapEngine.getProvinces().size())).c_str(), 10, 10, 20, WHITE);
        DrawText("Use mouse to explore", 10, 35, 16, LIGHTGRAY);
		DrawText("Left Click: Get region info", 10, 55, 16, LIGHTGRAY);

		// Show hovered region info
        if (!hoveredRegion.empty()) {
            DrawText(hoveredRegion.c_str(), 10, screenHeight - 30, 16, YELLOW);
        }

		EndDrawing();
	}
	CloseWindow();
	return 0;
}