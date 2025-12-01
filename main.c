//Compiling Command gcc main.c -o game.exe -lraylib -lopengl32 -lgdi32 -lwinmm

#include "raylib.h"

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE); // Maximizing window command

    InitWindow(800, 450, "Raylib Test!");
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Hello from Raylib!", 190, 200, 20, DARKGRAY);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}