// main.cpp - RTS Game Entry Point
// This is a basic template using raylib for an RTS game

#include "raylib.h"
#include <iostream>
#include <vector>
#include <format>

int main(void)
{
	const int screenWidth = 800;
	const int screenHeight = 450;

	InitWindow(screenWidth, screenHeight, "raylib basic window");
	SetTargetFPS(60);

	while (!WindowShouldClose())
	{
		BeginDrawing();
		ClearBackground(RAYWHITE);
		DrawText("Welcome to raylib!", 200, 200, 40, DARKGRAY);
		EndDrawing();
	}

	CloseWindow();
	return 0;
}
