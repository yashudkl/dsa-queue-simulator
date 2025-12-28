// Build (MSYS2 MinGW64 example): 
// gcc simulator.c -o simulator.exe -lraylib -lopengl32 -lgdi32 -lwinmm

#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>


// Vehicle structure

typedef struct {
    float x, y;       // position
    float vx, vy;     // velocity
    int road;         // road: 0=A,1=B,2=C,3=D
    int lane;         // lane index: 0=L1,1=L2,2=L3
    bool active;      // is vehicle active
    char plate[16];   // vehicle plate
} Vehicle;

#define MAX_VEH 64
static Vehicle vehicles[MAX_VEH];


// Queue for each lane

typedef struct {
    int indices[MAX_VEH];
    int front;
    int rear;
    int count;
} LaneQueue;

static LaneQueue laneQueues[4][3]; // 4 roads × 3 lanes

// Initialize queues
static void InitQueues(void) {
    for (int r = 0; r < 4; r++)
        for (int l = 0; l < 3; l++) {
            laneQueues[r][l].front = 0;
            laneQueues[r][l].rear = -1;
            laneQueues[r][l].count = 0;
        }
}

// Enqueue vehicle index into lane queue
static void Enqueue(LaneQueue *q, int vehIndex) {
    if (q->count >= MAX_VEH) return; // safety
    q->rear = (q->rear + 1) % MAX_VEH;
    q->indices[q->rear] = vehIndex;
    q->count++;
}

// Dequeue vehicle index from lane queue
static int Dequeue(LaneQueue *q) {
    if (q->count <= 0) return -1;
    int idx = q->indices[q->front];
    q->front = (q->front + 1) % MAX_VEH;
    q->count--;
    return idx;
}

// Simulation variables

static int currentGreen = 1;          
static float phaseTimer = 0.0f;       
static float currentGreenDuration = 0.0f;
static const float TIME_PER_VEHICLE = 0.8f;
static long vehiclesFilePos = 0;
static const float VEH_SPEED = 80.0f;
static const float CAR_LEN = 36.0f;
static const float CAR_WID = 18.0f;
static const float MIN_HEADWAY = 24.0f;
static const int MAX_SPAWNS_PER_TICK = 16;
static float laneSatTimer[4][3] = {0};
static bool al2PriorityActive = false;
static const int PRIORITY_ON_THRESHOLD = 10;
static const int PRIORITY_OFF_THRESHOLD = 5;


// Layout

static int screenW = 1200;
static int screenH = 900;
static const int roadWidth = 180;
static const int laneWidth = 60;
static int centerX = 1200 / 2;
static int centerY = 900 / 2;

static Color roadColor = {90, 90, 90, 255};
static Color laneColor = {140, 140, 140, 255};


// Utility functions

static float LaneLateralOffset(int road, int lane) {
    static const int slotMap[4][3] = {
        {0,1,2}, {2,1,0}, {0,1,2}, {2,1,0}
    };
    int slot = slotMap[road & 3][lane];
    return -roadWidth/2.0f + laneWidth*slot + laneWidth*0.5f;
}

static void SetLaneSpeed(Vehicle *v, float speed) {
    switch (v->road) {
        case 0: v->vx=0; v->vy=(v->lane==0)?-speed:speed; break;
        case 1: v->vx=0; v->vy=(v->lane==0)?speed:-speed; break;
        case 2: v->vy=0; v->vx=(v->lane==0)?speed:-speed; break;
        case 3: v->vy=0; v->vx=(v->lane==0)?-speed:speed; break;
        default: v->vx=v->vy=0; break;
    }
}

static void InitVehicles(void) {
    for(int i=0;i<MAX_VEH;i++) vehicles[i].active=false;
}


// Lane counting & averaging

static int LaneCount(int road, int lane) {
    int c=0;
    for(int i=0;i<MAX_VEH;i++)
        if(vehicles[i].active && vehicles[i].road==road && vehicles[i].lane==lane) c++;
    return c;
}

static float calculateAverageVehicles(void) {
    int sum = LaneCount(0,1)+LaneCount(1,1)+LaneCount(2,1)+LaneCount(3,1);
    return sum/4.0f;
}

static float calculateGreenDuration(void) {
    float avg = calculateAverageVehicles();
    float duration = avg*TIME_PER_VEHICLE;
    if(duration<TIME_PER_VEHICLE) duration=TIME_PER_VEHICLE;
    return duration;
}

static void UpdateAl2PriorityState(void) {
    int al2Count = LaneCount(0,1);
    if(!al2PriorityActive && al2Count>=PRIORITY_ON_THRESHOLD){
        al2PriorityActive=true;
        currentGreen=0;
        phaseTimer=0.0f;
    } else if(al2PriorityActive && al2Count<=PRIORITY_OFF_THRESHOLD){
        al2PriorityActive=false;
        phaseTimer=0.0f;
        currentGreenDuration=calculateGreenDuration();
    }
}

// Vehicle plate generator

static void GenerateVehicleNumber(char *buffer) {
    buffer[0]='A'+GetRandomValue(0,25);
    buffer[1]='A'+GetRandomValue(0,25);
    buffer[2]='0'+GetRandomValue(0,9);
    buffer[3]='A'+GetRandomValue(0,25);
    buffer[4]='A'+GetRandomValue(0,25);
    buffer[5]='0'+GetRandomValue(0,9);
    buffer[6]='0'+GetRandomValue(0,9);
    buffer[7]='0'+GetRandomValue(0,9);
    buffer[8]='\0';
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

static float LaneTravelCoordinate(const Vehicle *v) {
    switch (v->road) {
        case 0: return (v->lane == 0) ? -v->y : v->y;
        case 1: return (v->lane == 0) ? v->y : -v->y;
        case 2: return (v->lane == 0) ? v->x : -v->x;
        case 3: return (v->lane == 0) ? -v->x : v->x;
        default: return 0.0f;
    }
}

// Get lead vehicle distance along travel axis for simple car-following spacing
static float LeadGap(const Vehicle *self) {
    float best = 1e9f;
    for (int i = 0; i < MAX_VEH; i++) {
        const Vehicle *o = &vehicles[i];
        if (!o->active || o == self) continue;
        if (o->road != self->road || o->lane != self->lane) continue;

        float myS = LaneTravelCoordinate(self);
        float otherS = LaneTravelCoordinate(o);
        if (otherS > myS && (otherS - myS) < best) best = otherS - myS;
    }
    return best;
}



// Spawn vehicle
static void SpawnVehicle(int road, int lane, const char *plateOpt) {
    for(int i=0;i<MAX_VEH;i++){
        if(!vehicles[i].active){
            vehicles[i].active=true;
            vehicles[i].road=road;
            vehicles[i].lane=lane;
            if(plateOpt) strncpy(vehicles[i].plate,plateOpt,sizeof(vehicles[i].plate));
            else GenerateVehicleNumber(vehicles[i].plate);
            vehicles[i].plate[sizeof(vehicles[i].plate)-1]='\0';

            float lateral = LaneLateralOffset(road,lane);
            switch(road){
                case 0: vehicles[i].x=centerX+lateral; vehicles[i].y=-40; break;
                case 1: vehicles[i].x=centerX+lateral; vehicles[i].y=screenH+40; break;
                case 2: vehicles[i].x=screenW+40; vehicles[i].y=centerY+lateral; break;
                case 3: vehicles[i].x=-40; vehicles[i].y=centerY+lateral; break;
            }
            SetLaneSpeed(&vehicles[i], 120.0f);

            // enqueue vehicle in the lane queue
            Enqueue(&laneQueues[road][lane], i);
            break;
        }
    }
}


// Intersection transitions

static Vector2 Lane0SpawnPoint(int road) {
    Vector2 pos={(float)centerX,(float)centerY};
    const float exitOffset=CAR_LEN;
    float lateral=LaneLateralOffset(road,0);
    switch(road){
        case 0: pos.x=centerX+lateral; pos.y=centerY-roadWidth/2-exitOffset; break;
        case 1: pos.x=centerX+lateral; pos.y=centerY+roadWidth/2+exitOffset; break;
        case 2: pos.x=centerX+roadWidth/2+exitOffset; pos.y=centerY+lateral; break;
        case 3: pos.x=centerX-roadWidth/2-exitOffset; pos.y=centerY+lateral; break;
    }
    return pos;
}

static int RoadLeft(int road){ static const int map[4]={3,2,0,1}; return map[road&3]; }
static int RoadRight(int road){ static const int map[4]={2,3,1,0}; return map[road&3]; }
static int RoadOpposite(int road){ static const int map[4]={1,0,3,2}; return map[road&3]; }

static void TransitionVehicleThroughIntersection(Vehicle *v) {
    int originRoad=v->road;
    int originLane=v->lane;

    // remove from queue if L2
    if(originLane==1) Dequeue(&laneQueues[originRoad][originLane]);

    int destRoad;
    if(originLane==2) destRoad=RoadLeft(originRoad);
    else destRoad=(GetRandomValue(0,1)==0)?RoadOpposite(originRoad):RoadRight(originRoad);

    v->road=destRoad;
    v->lane=0;
    Vector2 pos=Lane0SpawnPoint(destRoad);
    v->x=pos.x; v->y=pos.y;
    SetLaneSpeed(v, VEH_SPEED);

    // enqueue in new lane if L2
    if(v->lane==1) Enqueue(&laneQueues[destRoad][v->lane], v-vehicles);
}



static void UpdateVehicles(float dt) {
    float stopOffset = roadWidth / 2.0f + 15.0f; // stop line distance to center
    float boxMinX = centerX - roadWidth / 2.0f;
    float boxMaxX = centerX + roadWidth / 2.0f;
    float boxMinY = centerY - roadWidth / 2.0f;
    float boxMaxY = centerY + roadWidth / 2.0f;
    for (int i = 0; i < MAX_VEH; i++) {
        Vehicle *v = &vehicles[i];
        if (!v->active) continue;

        bool approachLane = (v->lane != 0);
        bool stop = approachLane && ShouldStop(v);

        // car-following headway check (do not run into the vehicle ahead)
        float gap = LeadGap(v);
        bool tooClose = gap < (CAR_LEN + MIN_HEADWAY);

        if (approachLane) {
            float s = 0.0f;
            float stopLineS = 0.0f;
            float desiredS_stop = 0.0f;
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

            desiredS = desiredS_stop;
            if (gap < 1e8f) {
                float leaderS = s + gap;
                float spacingCenter = (CAR_LEN + MIN_HEADWAY);
                float desiredBehindLeader = leaderS - spacingCenter;
                if (desiredBehindLeader < desiredS) desiredS = desiredBehindLeader;
            }

            const float eps = 1.0f;
            if (stop) {
                if (tooClose) {
                    v->vx = 0;
                    v->vy = 0;
                } else if (s < desiredS - eps) {
                    SetLaneSpeed(v, VEH_SPEED);
                } else if (s <= desiredS + eps) {
                    v->vx = 0;
                    v->vy = 0;
                    switch (v->road) {
                        case 0: v->y = desiredS; break;
                        case 1: v->y = -desiredS; break;
                        case 2: v->x = -desiredS; break;
                        case 3: v->x = desiredS; break;
                    }
                } else {
                    SetLaneSpeed(v, VEH_SPEED);
                }
            } else {
                if (tooClose) {
                    v->vx = 0;
                    v->vy = 0;
                } else {
                    SetLaneSpeed(v, VEH_SPEED);
                }
            }
        } else {
            if (tooClose) {
                v->vx = 0;
                v->vy = 0;
            } else {
                SetLaneSpeed(v, VEH_SPEED);
            }
        }

        v->x += v->vx * dt;
        v->y += v->vy * dt;

        if (approachLane && v->x > boxMinX && v->x < boxMaxX && v->y > boxMinY && v->y < boxMaxY) {
            TransitionVehicleThroughIntersection(v);
            continue;
        }

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
    for (int lane = 0; lane < 3; lane++) {
        int lx = centerX + (int)LaneLateralOffset(0, lane) - 10;
        int ly = centerY - roadWidth/2 - 40;
        DrawText(laneNames[lane], lx, ly, textSize, BLACK);
    }

    // Road B (bottom)
    for (int lane = 0; lane < 3; lane++) {
        int lx = centerX + (int)LaneLateralOffset(1, lane) - 10;
        int ly = centerY + roadWidth/2 + 20;
        DrawText(laneNames[lane], lx, ly, textSize, BLACK);
    }

    // Road C (right)
    for (int lane = 0; lane < 3; lane++) {
        int lx = centerX + roadWidth/2 + 20;
        int ly = centerY + (int)LaneLateralOffset(2, lane) - textSize - gap;
        DrawText(laneNames[lane], lx, ly, textSize, BLACK);
    }

    // Road D (left)
    for (int lane = 0; lane < 3; lane++) {
        int lx = centerX - roadWidth/2 - 40;
        int ly = centerY + (int)LaneLateralOffset(3, lane) - textSize;
        DrawText(laneNames[lane], lx, ly, textSize, BLACK);
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

static void DrawPriorityStatus(void) {
    const char *message = al2PriorityActive ? "Priority condition ACTIVE" : "Priority condition inactive";
    int fontSize = 20;
    int textWidth = MeasureText(message, fontSize);
    int x = screenW - textWidth - 20;
    if (x < 20) x = 20;
    Color color = al2PriorityActive ? GREEN : DARKGRAY;
    DrawText(message, x, 20, fontSize, color);
}

static void DrawVehicles(void) {
    for (int i = 0; i < MAX_VEH; i++) {
        if (!vehicles[i].active) continue;
        Color c = (vehicles[i].lane == 1) ? ORANGE : SKYBLUE;
        if (vehicles[i].lane == 2) c = LIME;

        // Draw vehicle as a rounded car shape instead of a square box
        float carW = CAR_WID;
        float carL = CAR_LEN;
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
        if (lane == 0) continue; // lane 0 vehicles now only enter via intersection transitions
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

// Main loop

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_VSYNC_HINT);
    InitWindow(screenW,screenH,"Queue Simulator - Raylib UI");
    SetTargetFPS(60);

    InitVehicles();
    InitQueues(); // initialize lane queues
    SetRandomSeed((unsigned int)GetTime());

    currentGreenDuration=calculateGreenDuration();

    while(!WindowShouldClose()){
        float dt=GetFrameTime();

        // handle window resize
        screenW=GetScreenWidth(); screenH=GetScreenHeight();
        int newCenterX=screenW/2; int newCenterY=screenH/2;
        int dx=newCenterX-centerX, dy=newCenterY-centerY;
        if(dx!=0||dy!=0){
            for(int i=0;i<MAX_VEH;i++)
                if(vehicles[i].active){ vehicles[i].x+=dx; vehicles[i].y+=dy; }
        }
        centerX=newCenterX; centerY=newCenterY;

        // pull new vehicles from file
        PollVehicleFile();

        // update AL2 priority
        UpdateAl2PriorityState();

        // traffic light logic
        if(al2PriorityActive){ currentGreen=0; phaseTimer=0.0f; }
        else{
            phaseTimer+=dt;
            if(phaseTimer>=currentGreenDuration){
                phaseTimer=0.0f;
                currentGreen=(currentGreen+1)%4;
                currentGreenDuration=calculateGreenDuration();
            }
        }

        // decay lane saturation timers
        for(int r=0;r<4;r++) for(int l=0;l<3;l++)
            if(laneSatTimer[r][l]>0) laneSatTimer[r][l]-=dt;

        UpdateVehicles(dt);

        BeginDrawing();
        DrawRoads();
        DrawLights();
        DrawVehicles();
        DrawLaneMarkers();
        DrawLaneLabels();
        DrawLaneAlerts();
        DrawPriorityStatus();
        if(al2PriorityActive)
            DrawText("Green: A (AL2 priority hold)",20,20,22,BLACK);
        else
            DrawText(TextFormat("Green: %c   Phase: %.1f/%.1f",'A'+currentGreen,phaseTimer,currentGreenDuration),20,20,22,BLACK);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
