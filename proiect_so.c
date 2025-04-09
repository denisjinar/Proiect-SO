#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>


typedef struct treasure{
    char treasure_id[20];
    char user_name[25];
    float lat;
    float lg;
    char clue_text[50];
    int value;
}treasure;

void create_directory(const char *dir)
{
    struct stat st;
    if(stat(dir,&st)==-1)
    {
        
        mkdir(dir,0);
    }
}


void add_treasure(const char *hunt_id, const char *id, const char *user_name, double latitude, double longitude, const char *clue, int value) {
    char hunt_dir[256];
    snprintf(hunt_dir, sizeof(hunt_dir), "treasure_hunts/%s", hunt_id);

    create_directory("treasure_hunts");
    create_directory(hunt_dir);

    char treasure_file[256];
    snprintf(treasure_file, sizeof(treasure_file), "%s/%s.txt", hunt_dir, id);

    FILE *file = fopen(treasure_file, "r");
    if (file) {
        fclose(file);
        printf("Error");
        return;
    }

    file = fopen(treasure_file, "w");
    if (!file) {
        perror("Error");
        return;
    }

    fprintf(file, "ID: %s\n", id);
    fprintf(file, "User: %s\n", user_name);
    fprintf(file, "Latitude: %.6f\n", latitude);
    fprintf(file, "Longitude: %.6f\n", longitude);
    fprintf(file, "Clue: %s\n", clue);
    fprintf(file, "Value: %d\n", value);
    fclose(file);

    printf("Treasure %s added to hunt %s.\n", id, hunt_id);
}

void list_treasures(const char *hunt_id) {
    char hunt_dir[256];
    snprintf(hunt_dir, sizeof(hunt_dir), "treasure_hunts/%s", hunt_id);

    DIR *dir = opendir(hunt_dir);
    if (!dir) {
        printf("Error");
        return;
    }

    printf("Treasures in hunt %s:\n", hunt_id);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".txt")) {
            char treasure_file[256];
            snprintf(treasure_file, sizeof(treasure_file), "%s/%s", hunt_dir, entry->d_name);

            FILE *file = fopen(treasure_file, "r");
            if (!file) {
                perror("Error");
                continue;
            }

            char line[256];
            while (fgets(line, sizeof(line), file)) {
                printf("%s", line);
            }
            printf("\n");
            fclose(file);
        }
    }

    closedir(dir);
}


int main(int argc,char *argv[]) {
 
if (argc < 2) {
        return 1;
    }
        const char *hunt_id = argv[2];
        const char *id = argv[3];
        const char *user_name = argv[4];
        double latitude = atof(argv[5]);
        double longitude = atof(argv[6]);
        const char *clue = argv[7];
        int value = atoi(argv[8]);

        add_treasure(hunt_id, id, user_name, latitude, longitude, clue, value);
    return 0;
}