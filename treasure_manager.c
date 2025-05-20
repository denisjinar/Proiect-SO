#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> //control fisiere(ex:open)
#include <unistd.h> //unlink
#include <sys/stat.h> //stare fisiere (stat)
#include <sys/types.h> //size_t
#include <errno.h> //pt coduri de eroare
#include <time.h>
#include <dirent.h> //acces la directoare

#define TREASURE_ID_LEN 20
#define MAX_USERNAME_LEN 50
#define MAX_CLUE_LEN 256

typedef struct {
    char treasure_id[TREASURE_ID_LEN];
    char username[MAX_USERNAME_LEN];
    float latitude;
    float longitude;
    char clue[MAX_CLUE_LEN];
    int value;
} Treasure;

int createHuntDirectory(const char *hunt_id);
int createSymlinkForLog(const char *hunt_id);
int logOperation(const char *hunt_id, const char *operation, const char *details);
int addTreasure(const char *hunt_id, const char *treasure_id, const char *username,
                float latitude, float longitude, const char *clue, int value);
int listTreasures(const char *hunt_id);
int viewTreasure(const char *hunt_id, const char *treasure_id);
int removeTreasure(const char *hunt_id, const char *treasure_id);
int removeHunt(const char *hunt_id);

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Eroare1");
        exit(-1);
    }

    const char *command = argv[1];
    const char *hunt_id = argv[2];
    if (strcmp(command, "add") == 0) {
        if (argc != 9) {
            fprintf(stderr, "Eroare la argumente");
            exit(-1);
        }
        const char *treasure_id = argv[3];
        const char *username = argv[4];
        float latitude = atof(argv[5]);
        float longitude = atof(argv[6]);
        const char *clue = argv[7];
        int value = atoi(argv[8]);

        if (addTreasure(hunt_id, treasure_id, username, latitude, longitude, clue, value) < 0) {
            fprintf(stderr, "Eroare la adaugarea comorii.\n");
            exit(-1);
        }
    } else if (strcmp(command, "list") == 0) {
        if (listTreasures(hunt_id) < 0) {
            fprintf(stderr, "Eroare la listarea comorilor.\n");
            exit(-1);
        }
    } else if (strcmp(command, "view") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Eroare la argumente");
            exit(EXIT_FAILURE);
        }
        const char *treasure_id = argv[3];
        if (viewTreasure(hunt_id, treasure_id) < 0) {
            fprintf(stderr, "Eroare la vizualizarea comorii.\n");
            exit(-1);
        }
    } else if (strcmp(command, "remove_treasure") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Eroare la argumente");
            exit(EXIT_FAILURE);
        }
        const char *treasure_id = argv[3];
        if (removeTreasure(hunt_id, treasure_id) < 0) {
            fprintf(stderr, "Eroare la eliminarea comorii.\n");
            exit(-1);
        }
    } else if (strcmp(command, "remove_hunt") == 0) {
        if (removeHunt(hunt_id) < 0) {
            fprintf(stderr, "Eroare la eliminarea hunt-ului.\n");
            exit(-1);
        }
    } else {
        fprintf(stderr, "Comanda necunoscuta: %s\n", command);
        exit(-1);
    }

    return 0;
}

int createHuntDirectory(const char *hunt_id) { //se creeaza un director pentru hunt-uri daca nu e deja creat

    const char *root = "treasure_hunts";
    if (mkdir(root, 0755) == -1 && errno != EEXIST) {
        perror("mkdir treasure_hunts");
        return -1;
    }
    char dirPath[256];
    snprintf(dirPath, sizeof(dirPath), "%s/%s", root, hunt_id);
    if (mkdir(dirPath, 0755) == -1 && errno != EEXIST) {
        perror("mkdir hunt_id");
        return -1;
    }
    return 0;
}

int createSymlinkForLog(const char *hunt_id) { //pt a avea o cale scurta catre fisierul log
    char huntDir[256], logPath[256], symlinkName[256];
    if (snprintf(huntDir, sizeof(huntDir), "treasure_hunts/%s", hunt_id) >= sizeof(huntDir)) {
        fprintf(stderr, "Eroare2\n");
        return -1;
    }
    if (snprintf(logPath, sizeof(logPath), "%s/logged_hunt", huntDir) >= sizeof(logPath)) {
        fprintf(stderr, "Eroare3\n");
        return -1;
    }

    if (snprintf(symlinkName, sizeof(symlinkName), "logged_hunt-%s", hunt_id) >= sizeof(symlinkName)) {
        fprintf(stderr, "Eroare4\n");
        return -1;
    }
    unlink(symlinkName);
    if (symlink(logPath, symlinkName) == -1) {
        perror("symlink");
        return -1;
    }
    return 0;
}

int logOperation(const char *hunt_id, const char *operation, const char *details) { //pt inregistrarea detaliilor actiunilor in fisierul log 
    char huntDir[256], logPath[256];
    if (snprintf(huntDir, sizeof(huntDir), "treasure_hunts/%s", hunt_id) >= sizeof(huntDir)) {
        fprintf(stderr, "Eroare5\n");
        return -1;
    }
    if (snprintf(logPath, sizeof(logPath), "%s/logged_hunt", huntDir) >= sizeof(logPath)) {
        fprintf(stderr, "Eroare6\n");
        return -1;
    }
    int fd = open(logPath, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open log file");
        return -1;
    }
    time_t now = time(NULL);
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    char logEntry[512];
    int len = snprintf(logEntry, sizeof(logEntry), "[%s] %s: %s\n", timeStr, operation, details);
    if (write(fd, logEntry, len) != len) {
        perror("write log");
        close(fd);
        return -1;
    }
    close(fd);
    if (createSymlinkForLog(hunt_id) < 0)
        return -1;
    
    return 0;
}

int addTreasure(const char *hunt_id, const char *treasure_id, const char *username,
                float latitude, float longitude, const char *clue, int value) {
    if (createHuntDirectory(hunt_id) < 0)
        return -1;
    
    char filePath[256];
    snprintf(filePath, sizeof(filePath), "treasure_hunts/%s/treasures.dat", hunt_id);
    
    int fd = open(filePath, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open treasures.dat");
        return -1;
    }
    
    Treasure t;
    strncpy(t.treasure_id, treasure_id, TREASURE_ID_LEN - 1);
    t.treasure_id[TREASURE_ID_LEN - 1] = '\0';
    
    strncpy(t.username, username, MAX_USERNAME_LEN - 1);
    t.username[MAX_USERNAME_LEN - 1] = '\0';
    
    t.latitude = latitude;
    t.longitude = longitude;
    
    strncpy(t.clue, clue, MAX_CLUE_LEN - 1);
    t.clue[MAX_CLUE_LEN - 1] = '\0';
    
    t.value = value;
    ssize_t written = write(fd, &t, sizeof(Treasure));
    if (written != sizeof(Treasure)) {
        perror("write treasure");
        close(fd);
        return -1;
    }
    close(fd);
    if (logOperation(hunt_id, "add", treasure_id) < 0)
        return -1;
    
    printf("Comoara '%s' a fost adaugata cu succes in hunt-ul '%s'\n", treasure_id, hunt_id);
    return 0;
}

int listTreasures(const char *hunt_id) {
    char filePath[256];
    snprintf(filePath, sizeof(filePath), "treasure_hunts/%s/treasures.dat", hunt_id);
    
    struct stat st;
    if (stat(filePath, &st) == -1) {
        perror("stat treasures.dat");
        return -1;
    }
    printf("Hunt: %s\n", hunt_id);
    printf("Dimensiune fisier: %ld bytes\n", st.st_size);
    printf("Ultima modificare: %s", ctime(&st.st_mtime));
    
    int fd = open(filePath, O_RDONLY);
    if (fd == -1) {
        perror("open treasures.dat");
        return -1;
    }
    
    Treasure t;
    printf("Comorile:\n");
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        printf("ID: %s | User: %s | Coordonate: (%.6f, %.6f) | Value: %d\nClue: %s\n\n",
               t.treasure_id, t.username, t.latitude, t.longitude, t.value, t.clue);
    }
    close(fd);
    return 0;
}

int viewTreasure(const char *hunt_id, const char *treasure_id) {
    char filePath[256];
    snprintf(filePath, sizeof(filePath), "treasure_hunts/%s/treasures.dat", hunt_id);
    
    int fd = open(filePath, O_RDONLY);
    if (fd == -1) {
        perror("open treasures.dat");
        return -1;
    }
    
    Treasure t;
    int found = 0;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (strncmp(t.treasure_id, treasure_id, TREASURE_ID_LEN) == 0) {
            printf("ID: %s\nUser: %s\nLatitude: %.6f\nLongitude: %.6f\nClue: %s\nValue: %d\n",
                   t.treasure_id, t.username, t.latitude, t.longitude, t.clue, t.value);
            found = 1;
            break;
        }
    }
    close(fd);
    if (!found) {
        fprintf(stderr, "Comoara cu ID-ul '%s' nu a fost gasita.\n", treasure_id);
        return -1;
    }
    return 0;
}

int removeTreasure(const char *hunt_id, const char *treasure_id) {
    char filePath[256];
    snprintf(filePath, sizeof(filePath), "treasure_hunts/%s/treasures.dat", hunt_id);
    
    int fd = open(filePath, O_RDONLY);
    if (fd == -1) {
        perror("open treasures.dat");
        return -1;
    }
    
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        close(fd);
        return -1;
    }
    size_t totalRecords = st.st_size / sizeof(Treasure);
    Treasure *records = malloc(st.st_size);
    if (!records) {
        perror("malloc");
        close(fd);
        return -1;
    }
    
    ssize_t bytesRead = read(fd, records, st.st_size);
    if (bytesRead != st.st_size) {
        perror("read");
        free(records);
        close(fd);
        return -1;
    }
    close(fd);
    
    fd = open(filePath, O_WRONLY | O_TRUNC);
    if (fd == -1) {
        perror("open treasures.dat for writing");
        free(records);
        return -1;
    }
    
    int found = 0;
    for (size_t i = 0; i < totalRecords; i++) {
        if (strncmp(records[i].treasure_id, treasure_id, TREASURE_ID_LEN) == 0) {
            found = 1;
            continue;
        }
        if (write(fd, &records[i], sizeof(Treasure)) != sizeof(Treasure)) {
            perror("write record");
            free(records);
            close(fd);
            return -1;
        }
    }
    free(records);
    close(fd);
    
    if (!found) {
        fprintf(stderr, "Comoara cu ID-ul '%s' nu a fost gasita pentru eliminare.\n", treasure_id);
        return -1;
    }
    
    if (logOperation(hunt_id, "remove_treasure", treasure_id) < 0)
        return -1;
    
    printf("Comoara '%s' a fost eliminata din hunt-ul '%s'.\n", treasure_id, hunt_id);
    return 0;
}
int removeHunt(const char *hunt_id) {
    char huntDir[256];
    snprintf(huntDir, sizeof(huntDir), "treasure_hunts/%s", hunt_id);
    
    char symlinkName[256];
    snprintf(symlinkName, sizeof(symlinkName), "logged_hunt-%s", hunt_id);
    
    char command[1024];
    snprintf(command, sizeof(command), "rm -rf %s %s", huntDir, symlinkName);
    if (system(command) != 0) {
        fprintf(stderr, "Eroare la stergerea hunt-ului '%s'.\n", hunt_id);
        return -1;
    }
    
    printf("Hunt-ul '%s' a fost eliminat.\n", hunt_id);
    return 0;
}
