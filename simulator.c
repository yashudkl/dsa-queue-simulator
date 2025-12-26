// Build (MSYS2 MinGW64 example):
// gcc simulator.c -o simulator.exe -lraylib -lopengl32 -lgdi32 -lwinmm

#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Roads: 0=A (top), 1=B (bottom), 2=C (right), 3=D (left)
// Lanes per road: 0 = L1 incoming, 1 = L2 outgoing (obeys light), 2 = L3 free left-turn
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
static int currentGreen = 1;          // only this road is green (state 2), others red (state 1); start with road B
static float phaseTimer = 0.0f;       // tracks elapsed time in current green phase
static float currentGreenDuration = 0.0f; // duration for current green phase, computed from lane averages
static const float TIME_PER_VEHICLE = 0.8f; // T = constant time per vehicle (seconds)
static long vehiclesFilePos = 0;
static const float VEH_SPEED = 80.0f;   // slower base speed
static const float CAR_LEN = 36.0f;      // vehicle length for spacing
static const float CAR_WID = 18.0f;      // vehicle width for drawing
static const float MIN_HEADWAY = 24.0f;  // min gap to avoid overlap
static const int MAX_SPAWNS_PER_TICK = 16; // allow faster buildup for saturation
static float laneSatTimer[4][3] = {0};    // saturation alert timers per lane

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

static int LaneCount(int road, int lane) {
    int c = 0;
    for (int i = 0; i < MAX_VEH; i++) {
        if (vehicles[i].active && vehicles[i].road == road && vehicles[i].lane == lane) c++;
    }
    return c;
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

// Average queue length across controlled lanes (BL2, CL2, DL2)
static float calculateAverageVehicles(void) {
    int sum = 0;
    // include all four approaches' L2 (outgoing) lanes so green durations reflect overall demand
    sum += LaneCount(0, 1); // road A, lane L2
    sum += LaneCount(1, 1); // road B, lane L2
    sum += LaneCount(2, 1); // road C, lane L2
    sum += LaneCount(3, 1); // road D, lane L2
    return sum / 4.0f;      // n = 4 lanes
}

// Green duration derived from average vehicles; clamp to at least one vehicle slot
static float calculateGreenDuration(void) {
    float avg = calculateAverageVehicles();
    float duration = avg * TIME_PER_VEHICLE;
    if (duration < TIME_PER_VEHICLE) duration = TIME_PER_VEHICLE;
    return duration;
}

// Deterministic rotation among controlled roads: B -> C -> D -> B ...
// Deterministic rotation among controlled roads: A -> B -> C -> D -> A ...
static int NextControlledRoad(int road) {
    return (road + 1) % 4;
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

// Lane rules:
// - L1 (lane index 0) = incoming lane (does NOT obey traffic light here)
// - L2 (lane index 1) = controlled/outgoing lane (obeys light)
// - L3 (lane index 2) = free left-turn (never stops)
// Only L2 should stop when the road is not green.
static bool ShouldStop(const Vehicle *v) {
    if (v->lane == 2) return false;        // free left-turn never stops
    if (v->lane == 1) {                    // only L2 obeys the traffic light
        if (v->road != currentGreen) return true; // red for this road
        return false;
    }
    // L1 (lane 0) does not obey the road-level traffic light in this simplified model
    return false;
}

// Get lead vehicle distance along travel axis for simple car-following spacing
static float LeadGap(const Vehicle *self) {
    float best = 1e9f;
    for (int i = 0; i < MAX_VEH; i++) {
        const Vehicle *o = &vehicles[i];
        if (!o->active || o == self) continue;
        if (o->road != self->road || o->lane != self->lane) continue;

        // Project positions along travel axis depending on road
        float myS = 0, otherS = 0;
        switch (self->road) {
            case 0: myS = self->y; otherS = o->y; break;           // increasing y
            case 1: myS = -self->y; otherS = -o->y; break;         // decreasing y
            case 2: myS = -self->x; otherS = -o->x; break;         // decreasing x
            case 3: myS = self->x; otherS = o->x; break;           // increasing x
        }
        if (otherS > myS && (otherS - myS) < best) best = otherS - myS;
    }
    return best;
}

static void UpdateVehicles(float dt) {
    float stopOffset = roadWidth / 2.0f + 15.0f; // stop line distance to center
    for (int i = 0; i < MAX_VEH; i++) {
        Vehicle *v = &vehicles[i];
        if (!v->active) continue;

        bool stop = ShouldStop(v);

        // car-following headway check (do not run into the vehicle ahead)
        float gap = LeadGap(v);
        bool tooClose = gap < (CAR_LEN + MIN_HEADWAY);

        // We'll operate in a unified "s" coordinate that increases along the vehicle's travel direction.
        // This lets us compute a desired center position `desiredS` which is either the stop-line position
        // or a position behind the leader (leaderS - spacing). Vehicles will drive toward `desiredS` and
        // stop exactly at that point (snapping to avoid jitter). If not stopped by a light, vehicles travel
        // at `VEH_SPEED` as before.
        float s = 0.0f;            // center coordinate along travel axis
        float stopLineS = 0.0f;    // stop line coordinate in s-space
        float desiredS_stop = 0.0f; // desired center s to align front with stop line
        float desiredS = 0.0f;

        switch (v->road) {
            case 0: // top -> down (y increasing)
                s = v->y;
                stopLineS = centerY - stopOffset;
                desiredS_stop = stopLineS - (CAR_LEN * 0.5f);
                break;
            case 1: // bottom -> up (y decreasing)
                s = -v->y;
                stopLineS = -(centerY + stopOffset);
                desiredS_stop = stopLineS - (CAR_LEN * 0.5f);
                break;
            case 2: // right -> left (x decreasing)
                s = -v->x;
                stopLineS = -(centerX + stopOffset);
                desiredS_stop = stopLineS - (CAR_LEN * 0.5f);
                break;
            case 3: // left -> right (x increasing)
                s = v->x;
                stopLineS = centerX - stopOffset;
                desiredS_stop = stopLineS - (CAR_LEN * 0.5f);
                break;
        }

        // Default desired is the stop-line center (front aligned to stop line)
        desiredS = desiredS_stop;

        // If there's a leader, compute position behind the leader with spacing
        if (gap < 1e8f) {
            float leaderS = s + gap; // LeadGap returns (leaderS - myS)
            float spacingCenter = (CAR_LEN + MIN_HEADWAY);
            float desiredBehindLeader = leaderS - spacingCenter;
            if (desiredBehindLeader < desiredS) desiredS = desiredBehindLeader;
        }

        // If the lane obeys the light and it's red, drive toward desiredS (or stop there).
        // Use a tolerant check so snapped vehicles do not immediately resume and cross the intersection.
        const float eps = 1.0f;
        if (stop) {
            if (tooClose) {
                v->vx = 0; v->vy = 0;
            } else if (s < desiredS - eps) {
                // move forward toward the desired stop
                switch (v->road) {
                    case 0: v->vy = VEH_SPEED; v->vx = 0; break;
                    case 1: v->vy = -VEH_SPEED; v->vx = 0; break;
                    case 2: v->vx = -VEH_SPEED; v->vy = 0; break;
                    case 3: v->vx = VEH_SPEED; v->vy = 0; break;
                }
            } else if (s <= desiredS + eps) {
                // reached or slightly past desiredS: stop and snap to exact coordinate
                v->vx = 0; v->vy = 0;
                switch (v->road) {
                    case 0: v->y = desiredS; break;
                    case 1: v->y = -desiredS; break;
                    case 2: v->x = -desiredS; break;
                    case 3: v->x = desiredS; break;
                }
            } else {
                // already past desired stop point (likely inside intersection) — allow to continue
                switch (v->road) {
                    case 0: v->vy = VEH_SPEED; v->vx = 0; break;
                    case 1: v->vy = -VEH_SPEED; v->vx = 0; break;
                    case 2: v->vx = -VEH_SPEED; v->vy = 0; break;
                    case 3: v->vx = VEH_SPEED; v->vy = 0; break;
                }
            }
        } else {
            // Not required to stop here: normal travel, but respect car-following spacing
            if (tooClose) {
                v->vx = 0; v->vy = 0;
            } else {
                switch (v->road) {
                    case 0: v->vy = VEH_SPEED; v->vx = 0; break;
                    case 1: v->vy = -VEH_SPEED; v->vx = 0; break;
                    case 2: v->vx = -VEH_SPEED; v->vy = 0; break;
                    case 3: v->vx = VEH_SPEED; v->vy = 0; break;
                }
            }
        }

        v->x += v->vx * dt;
        v->y += v->vy * dt;

        // Despawn when inside the intersection box to avoid mid-cross collisions
        float bx = centerX - roadWidth/2;
        float by = centerY - roadWidth/2;
        if (v->x > bx && v->x < bx + roadWidth && v->y > by && v->y < by + roadWidth) {
            v->active = false;
            continue;
        }

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

static void DrawLaneMarkers(void) {
    // Label lanes near the stop line for each approach
    const char *laneNames[3] = {"L1", "L2", "L3"};
    int textSize = 16;
    int gap = 6;

    // Road A (top), lanes stacked horizontally across road width
    for (int i = 0; i < 3; i++) {
        int lx = centerX - roadWidth/2 + laneWidth * i + laneWidth/2 - 10;
        int ly = centerY - roadWidth/2 - 40;
        DrawText(laneNames[i], lx, ly, textSize, BLACK);
    }

    // Road B (bottom)
    for (int i = 0; i < 3; i++) {
        int lx = centerX + roadWidth/2 - laneWidth * i - laneWidth/2 - 10;
        int ly = centerY + roadWidth/2 + 20;
        DrawText(laneNames[i], lx, ly, textSize, BLACK);
    }

    // Road C (right)
    for (int i = 0; i < 3; i++) {
        int lx = centerX + roadWidth/2 + 20;
        int ly = centerY + roadWidth/2 - laneWidth * i - laneWidth/2 - textSize - gap;
        DrawText(laneNames[i], lx, ly, textSize, BLACK);
    }

    // Road D (left)
    for (int i = 0; i < 3; i++) {
        int lx = centerX - roadWidth/2 - 40;
        int ly = centerY - roadWidth/2 + laneWidth * i + laneWidth/2 - textSize;
        DrawText(laneNames[i], lx, ly, textSize, BLACK);
    }
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
    DrawText("L1 incoming, L2 outgoing (obeys light), L3 free left-turn", 20, screenH - 60, 18, DARKGRAY);
    DrawText("Only one road green at a time to avoid deadlock", 20, screenH - 35, 18, DARKGRAY);
}

static void DrawLaneAlerts(void) {
    int y = 50; // push down to avoid overlapping the HUD header
    for (int r = 0; r < 4; r++) {
        for (int l = 0; l < 3; l++) {
            if (laneSatTimer[r][l] > 0) {
                const char roadChar = 'A' + r;
                DrawText(TextFormat("Lane %c L%d saturated (>=10 vehicles)", roadChar, l+1), 20, y, 18, RED);
                y += 22;
            }
        }
    }
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
    int spawned = 0;
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
        int before = LaneCount(road, lane);
        if (before >= 10) laneSatTimer[road][lane] = 3.0f; // already saturated

        SpawnVehicle(road, lane, plate);

        int after = LaneCount(road, lane);
        if (after >= 10) laneSatTimer[road][lane] = 3.0f; // hit or stay saturated after spawn
        if (++spawned >= MAX_SPAWNS_PER_TICK) break; // avoid burst spawning
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

    // Initialize first green duration using current queue snapshot
    currentGreenDuration = calculateGreenDuration();

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

        // Light FSM: only one green; rotate B -> C -> D with duration based on queue average
        phaseTimer += dt;
        if (phaseTimer >= currentGreenDuration) {
            phaseTimer = 0.0f;
            currentGreen = NextControlledRoad(currentGreen);
            currentGreenDuration = calculateGreenDuration();
        }

        // Decay lane saturation timers
        for (int r = 0; r < 4; r++)
            for (int l = 0; l < 3; l++)
                if (laneSatTimer[r][l] > 0) laneSatTimer[r][l] -= dt;

        // Pull new vehicles appended by traffic_generator.c
        PollVehicleFile();

        UpdateVehicles(dt);

        BeginDrawing();
        DrawRoads();
        DrawLights();
        DrawVehicles();
        DrawLaneMarkers();
        DrawLaneLabels();
        DrawLaneAlerts();
        DrawText(TextFormat("Green: %c   Phase: %.1f/%.1f", 'A'+currentGreen, phaseTimer, currentGreenDuration), 20, 20, 22, BLACK);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
