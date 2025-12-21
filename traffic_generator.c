// gcc traffic_generator.c -o traffic_generator.exe -lraylib -lopengl32 -lgdi32 -lwinmm

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define FILENAME "vehicles.data"
#define MAX_LINES 5000 ///Limits 5000 vehicles
#define MAX_LINE_LENGTH 64
#define TRIM_INTERVAL 1000

// Generate a random vehicle number
static void GenerateVehicleNumber(char *buffer) {
    buffer[0] = 'A' + rand() % 26;
    buffer[1] = 'A' + rand() % 26;
    buffer[2] = '0' + rand() % 10;
    buffer[3] = 'A' + rand() % 26;
    buffer[4] = 'A' + rand() % 26;
    buffer[5] = '0' + rand() % 10;
    buffer[6] = '0' + rand() % 10;
    buffer[7] = '0' + rand() % 10;
    buffer[8] = '\0';
}

// Pick a road (A,B,C,D) and lane (0,1,2)
static void PickRoadLane(char *road, int *lane) {
    const char roads[] = {'A', 'B', 'C', 'D'};
    *road = roads[rand() % 4];
    *lane = rand() % 3;
}

// Keep only the last MAX_LINES entries in the file
static void TrimFile(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return;

    char lines[MAX_LINES][MAX_LINE_LENGTH];
    int count = 0;

    while (fgets(lines[count % MAX_LINES], MAX_LINE_LENGTH, file)) {
        count++;
    }
    fclose(file);

    if (count <= MAX_LINES) return;

    file = fopen(filename, "w");
    if (!file) return;

    int start = count % MAX_LINES;
    for (int i = 0; i < MAX_LINES; i++) {
        fputs(lines[(start + i) % MAX_LINES], file);
    }

    fclose(file);
}

int main(void) {
    FILE *file = fopen(FILENAME, "a");
    if (!file) {
        perror("Error opening vehicles.data");
        return 1;
    }

    srand((unsigned int)time(NULL));

    int vehicleCount = 0;

    while (1) {
        char plate[9];
        char road;
        int lane;

        GenerateVehicleNumber(plate);
        PickRoadLane(&road, &lane);

        fprintf(file, "%s:%c:%d\n", plate, road, lane);
        fflush(file);

        printf("Generated: %s:%c:%d\n", plate, road, lane);

        vehicleCount++;
        if (vehicleCount % TRIM_INTERVAL == 0) {
            fclose(file);
            TrimFile(FILENAME);
            file = fopen(FILENAME, "a");
            if (!file) break;
        }

        sleep(1);
    }

    fclose(file);
    return 0;
}
