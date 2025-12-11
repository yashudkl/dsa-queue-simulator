// Build (MSYS2 MinGW64 example):
// gcc main.c -o simulator.exe -lraylib -lopengl32 -lgdi32 -lwinmm

#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Roads: 0=A (top), 1=B (bottom), 2=C (right), 3=D (left)
// Lanes per road: 0 = L1 incoming, 1 = L2 priority (obeys light), 2 = L3 free left-turn
typedef struct {
    float x, y;
    float vx, vy;
    int road;
    int lane;
    bool active;
    char plate[10];
} Vehicle;

#define MAX_VEH 64

static Vehicle vehicles[MAX_VEH];
static int currentGreen = 0;          // only this road is green (state 2), others red (state 1)
static float lightTimer = 0.0f;
static const float LIGHT_PERIOD = 6.0f; // seconds per phase
static long vehiclesFilePos = 0;
static const float VEH_SPEED = 80.0f;   // slower base speed

// Layout constants
static int screenW = 1200;
static int screenH = 900;
static const int roadWidth = 180;
static const int laneWidth = 60;
static int centerX = 1200 / 2;
static int centerY = 900 / 2;
static int prevCenterX = 1200 / 2;
static int prevCenterY = 900 / 2;

static Color roadColor = {90, 90, 90, 255};
static Color laneColor = {140, 140, 140, 255};

static void InitVehicles(void) {
    for (int i = 0; i < MAX_VEH; i++) vehicles[i].active = false;
}

// Generate a random vehicle plate similar to traffic_generator.c
static void GenerateVehicleNumber(char *buffer) {
    buffer[0] = 'A' + GetRandomValue(0, 25);
    buffer[1] = 'A' + GetRandomValue(0, 25);
    buffer[2] = '0' + GetRandomValue(0, 9);
    buffer[3] = 'A' + GetRandomValue(0, 25);
    buffer[4] = 'A' + GetRandomValue(0, 25);
    buffer[5] = '0' + GetRandomValue(0, 9);
    buffer[6] = '0' + GetRandomValue(0, 9);
    buffer[7] = '0' + GetRandomValue(0, 9);
    buffer[8] = '\0';
}

// Spawn a vehicle at the edge of a given road/lane
static void SpawnVehicle(int road, int lane, const char *plateOpt) {
    for (int i = 0; i < MAX_VEH; i++) {
        if (!vehicles[i].active) {
            vehicles[i].active = true;
            vehicles[i].road = road;
            vehicles[i].lane = lane;
            if (plateOpt) strncpy(vehicles[i].plate, plateOpt, sizeof(vehicles[i].plate)-1);
            else GenerateVehicleNumber(vehicles[i].plate);
            vehicles[i].plate[sizeof(vehicles[i].plate)-1] = '\0';

            switch (road) {
                case 0: // A from top downward
                    vehicles[i].x = centerX - roadWidth/2 + laneWidth * lane + laneWidth/2;
                    vehicles[i].y = -40;
                    vehicles[i].vx = 0; vehicles[i].vy = 120;
                    break;
                case 1: // B from bottom upward
                    vehicles[i].x = centerX + roadWidth/2 - laneWidth * lane - laneWidth/2;
                    vehicles[i].y = screenH + 40;
                    vehicles[i].vx = 0; vehicles[i].vy = -120;
                    break;
                case 2: // C from right leftward
                    vehicles[i].x = screenW + 40;
                    vehicles[i].y = centerY + roadWidth/2 - laneWidth * lane - laneWidth/2;
                    vehicles[i].vx = -120; vehicles[i].vy = 0;
                    break;
                case 3: // D from left rightward
                    vehicles[i].x = -40;
                    vehicles[i].y = centerY - roadWidth/2 + laneWidth * lane + laneWidth/2;
                    vehicles[i].vx = 120; vehicles[i].vy = 0;
                    break;
            }
            break;
        }
    }
}

// Lane rules: L3 free left-turn never stops; L1/L2 obey lights; only one road green at a time
static bool ShouldStop(const Vehicle *v) {
    if (v->lane == 2) return false;        // free left-turn
    if (v->road != currentGreen) return true; // red for this road
    return false;
}

static void UpdateVehicles(float dt) {
    float stopOffset = roadWidth / 2.0f + 15.0f; // stop line distance to center
    for (int i = 0; i < MAX_VEH; i++) {
        Vehicle *v = &vehicles[i];
        if (!v->active) continue;

        bool stop = ShouldStop(v);

        switch (v->road) {
            case 0: { // top moving down
                bool beforeStop = v->y < centerY - stopOffset;
                v->vy = (stop && beforeStop) ? 0 : VEH_SPEED;
                v->vx = 0;
            } break;
            case 1: { // bottom moving up
                bool beforeStop = v->y > centerY + stopOffset;
                v->vy = (stop && beforeStop) ? 0 : -VEH_SPEED;
                v->vx = 0;
            } break;
            case 2: { // right moving left
                bool beforeStop = v->x > centerX + stopOffset;
                v->vx = (stop && beforeStop) ? 0 : -VEH_SPEED;
                v->vy = 0;
            } break;
            case 3: { // left moving right
                bool beforeStop = v->x < centerX - stopOffset;
                v->vx = (stop && beforeStop) ? 0 : VEH_SPEED;
                v->vy = 0;
            } break;
        }

        v->x += v->vx * dt;
        v->y += v->vy * dt;

        // Despawn when off screen
        if (v->x < -200 || v->x > screenW + 200 || v->y < -200 || v->y > screenH + 200) {
            v->active = false;
        }
    }
}

static void DrawRoads(void) {
    ClearBackground((Color){220, 226, 230, 255});

    // Vertical road (A/B)
    DrawRectangle(centerX - roadWidth/2, 0, roadWidth, screenH, roadColor);
    // Horizontal road (C/D)
    DrawRectangle(0, centerY - roadWidth/2, screenW, roadWidth, roadColor);

    // Lane lines
    for (int i = 1; i < 3; i++) {
        DrawLine(centerX - roadWidth/2 + laneWidth * i, 0,
                 centerX - roadWidth/2 + laneWidth * i, screenH, laneColor);
        DrawLine(0, centerY - roadWidth/2 + laneWidth * i,
                 screenW, centerY - roadWidth/2 + laneWidth * i, laneColor);
    }

    // Intersection box
    DrawRectangleLines(centerX - roadWidth/2, centerY - roadWidth/2, roadWidth, roadWidth, WHITE);
}

static void DrawLights(void) {
    const char *labels[4] = {"A", "B", "C", "D"};
    // Aligned close to each approach's stop line
    Vector2 pos[4] = {
        { centerX - 25, centerY - roadWidth/2 - 110 }, // A top
        { centerX - 25, centerY + roadWidth/2 + 20 },  // B bottom
        { centerX + roadWidth/2 + 20, centerY - 25 },  // C right
        { centerX - roadWidth/2 - 70, centerY - 25 }   // D left
    };

    for (int i = 0; i < 4; i++) {
        DrawRectangle(pos[i].x, pos[i].y, 50, 90, DARKGRAY);
        DrawRectangleLines(pos[i].x, pos[i].y, 50, 90, WHITE);
        Color red = (i == currentGreen) ? (Color){80,80,80,255} : RED;
        Color green = (i == currentGreen) ? GREEN : (Color){40,40,40,255};
        DrawCircle(pos[i].x + 25, pos[i].y + 22, 12, red);
        DrawCircle(pos[i].x + 25, pos[i].y + 68, 12, green);
        DrawText(labels[i], pos[i].x + 18, pos[i].y + 44, 12, WHITE);
    }
}

static void DrawLaneLabels(void) {
    DrawText("L1 incoming, L2 priority (obeys light), L3 free left-turn", 20, screenH - 60, 18, DARKGRAY);
    DrawText("Only one road green at a time to avoid deadlock", 20, screenH - 35, 18, DARKGRAY);
}

static void DrawVehicles(void) {
    for (int i = 0; i < MAX_VEH; i++) {
        if (!vehicles[i].active) continue;
        Color c = (vehicles[i].lane == 1) ? ORANGE : SKYBLUE;
        if (vehicles[i].lane == 2) c = LIME;

        // Draw vehicle as a rounded car shape instead of a square box
        float carW = 18.0f;
        float carL = 36.0f;
        float px = vehicles[i].x - carW * 0.5f;
        float py = vehicles[i].y - carL * 0.5f;
        DrawRectangleRounded((Rectangle){px, py, carW, carL}, 0.35f, 6, c);
        DrawRectangleRoundedLines((Rectangle){px, py, carW, carL}, 0.35f, 6, BLACK);
        DrawText(vehicles[i].plate, (int)(px - 6), (int)(py - 14), 10, BLACK);
    }
}

// Read appended lines from vehicles.data in format PLATE:ROAD:LANE
static void PollVehicleFile(void) {
    FILE *f = fopen("vehicles.data", "r");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < vehiclesFilePos) vehiclesFilePos = 0; // file truncated/rotated
    fseek(f, vehiclesFilePos, SEEK_SET);

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        char plate[16];
        char roadChar;
        int lane;
        if (sscanf(line, "%15[^:]:%c:%d", plate, &roadChar, &lane) != 3) continue;
        int road = -1;
        switch (roadChar) {
            case 'A': road = 0; break;
            case 'B': road = 1; break;
            case 'C': road = 2; break;
            case 'D': road = 3; break;
        }
        if (road < 0 || lane < 0 || lane > 2) continue;
        SpawnVehicle(road, lane, plate);
    }

    vehiclesFilePos = ftell(f);
    fclose(f);
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(screenW, screenH, "Queue Simulator - Raylib UI");
    SetTargetFPS(60);

    InitVehicles();
    SetRandomSeed((unsigned int)GetTime());

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // Responsive layout: update size each frame
        screenW = GetScreenWidth();
        screenH = GetScreenHeight();
        int newCenterX = screenW / 2;
        int newCenterY = screenH / 2;

        // Shift all active vehicles by the delta so they stay in-lane after resize
        int dx = newCenterX - centerX;
        int dy = newCenterY - centerY;
        if (dx != 0 || dy != 0) {
            for (int i = 0; i < MAX_VEH; i++) {
                if (vehicles[i].active) {
                    vehicles[i].x += dx;
                    vehicles[i].y += dy;
                }
            }
        }
        prevCenterX = centerX;
        prevCenterY = centerY;
        centerX = newCenterX;
        centerY = newCenterY;

        // Light FSM: only one green; rotate A->B->C->D
        lightTimer += dt;
        if (lightTimer >= LIGHT_PERIOD) {
            lightTimer = 0.0f;
            currentGreen = (currentGreen + 1) % 4;
        }

        // Pull new vehicles appended by traffic_generator.c
        PollVehicleFile();

        UpdateVehicles(dt);

        BeginDrawing();
        DrawRoads();
        DrawLights();
        DrawVehicles();
        DrawLaneLabels();
        DrawText(TextFormat("Green: %c   Phase: %.1f/%.0f", 'A'+currentGreen, lightTimer, LIGHT_PERIOD), 20, 20, 22, BLACK);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}