#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TREASURE_ID_LEN 20
#define MAX_USERNAME_LEN 50

typedef struct {
    char treasure_id[TREASURE_ID_LEN];
    char username[MAX_USERNAME_LEN];
    float latitude;
    float longitude;
    char clue[256];
    int value;
} Treasure;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Utilizare: %s <hunt_id>\n", argv[0]);
        return 1;
    }

    const char *hunt_id = argv[1];
    char filePath[256];
    snprintf(filePath, sizeof(filePath), "treasure_hunts/%s/treasures.dat", hunt_id);

    FILE *fp = fopen(filePath, "rb");
    if (fp == NULL) {
        perror("Eroare la deschiderea fișierului treasures.dat");
        return 1;
    }

    Treasure treasure;
    float user_scores[50] = {0.0};
    char usernames[50][MAX_USERNAME_LEN] = {""};
    int user_count = 0;
    int found;

    while (fread(&treasure, sizeof(Treasure), 1, fp) == 1) {
        found = 0;
        for (int j = 0; j < user_count; j++) {
            if (strcmp(usernames[j], treasure.username) == 0) {
                user_scores[j] += treasure.value;
                found = 1;
                break;
            }
        }
        if (!found) {
            strcpy(usernames[user_count], treasure.username);
            user_scores[user_count] = treasure.value;
            user_count++;
        }
    }
    fclose(fp);

    for (int j = 0; j < user_count; j++) {
        printf("Scorul utilizatorului %s: %.0f\n", usernames[j], user_scores[j]);
    }

    return 0;
}