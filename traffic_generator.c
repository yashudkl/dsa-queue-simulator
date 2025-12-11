#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define FILENAME "vehicles.data"

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

// Pick a road (A,B,C,D) and lane (0=L1 incoming, 1=L2 priority, 2=L3 left-turn)
static void PickRoadLane(char *road, int *lane) {
    const char roads[] = {'A', 'B', 'C', 'D'};
    *road = roads[rand() % 4];
    *lane = rand() % 3;
}

int main(void) {
    FILE *file = fopen(FILENAME, "a");
    if (!file) {
        perror("Error opening vehicles.data");
        return 1;
    }

    srand((unsigned int)time(NULL));

    while (1) {
        char plate[9];
        char road;
        int lane;
        GenerateVehicleNumber(plate);
        PickRoadLane(&road, &lane);

        // Format: PLATE:ROAD:LANE
        fprintf(file, "%s:%c:%d\n", plate, road, lane);
        fflush(file);

        printf("Generated: %s:%c:%d\n", plate, road, lane);
        sleep(1); // adjust rate as needed
    }

    fclose(file);
    return 0;
}
