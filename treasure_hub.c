#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/wait.h>
#include <dirent.h>

#define PATH_COMMAND_TXT "command.txt"
#define PATH_COMMAND "command"
#define TREASURE_MANAGER_EXEC "./treasure_manager"

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

volatile int monitor_working = 1;
volatile int monitor_shutting_down = 0;
volatile int shutdown = 0;
pid_t monitor_pid = -1;

void error_exit(char* message) {
    perror(message);
    exit(-1);
}

int open_file_read(char* filename) {
    int f_out = 0;
    if ((f_out = open(PATH_COMMAND_TXT, O_RDONLY)) == -1) {
        if (errno != ENOENT && errno != 0) {
            error_exit("Eroare la deschidere");
        }
    }
    return f_out;
}

void close_file(int f_out) {
    if (close(f_out) != 0) {
        error_exit("Eroare la închidere");
    }
}

int open_file_write(char filename[]) {
    errno = 0;
    int f_out = 0;
    if ((f_out = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
        error_exit("Eroare la deschidere");
    }
    return f_out;
}

int get_file_size(char* filename) {
    struct stat statbuf;
    int status = stat(filename, &statbuf);

    if (status != 0) {
        error_exit("Eroare la stat");
    }

    return statbuf.st_size;
}

void write_in_command(char* text) {
    int fd = open_file_write(PATH_COMMAND);
    if (write(fd, text, strlen(text)) == -1) {
        error_exit("Eroare la scriere");
    }
    close_file(fd);
    rename(PATH_COMMAND, PATH_COMMAND_TXT);
}

void get_content(char* f_content) {
    int fd = open_file_read(PATH_COMMAND_TXT);
    if (fd != -1) {
        int file_size = get_file_size(PATH_COMMAND_TXT);
        if (read(fd, f_content, file_size) != file_size) {
            error_exit("Eroare la citirea fisierului");
        }
        f_content[file_size] = '\0';
        close_file(fd);
    }
}

void handler(int signum) {
    if (signum == SIGTERM) {
        usleep(5000000);
        shutdown = 1;
    }
    if (signum == SIGUSR1) {
        char f_content[100] = {'\0'};
        char word[5][50] = {'\0'};

        get_content(f_content);

        int i = 0;
        char *token = strtok(f_content, " ");
        while(token != NULL && i < 5) {
            strcpy(word[i], token);
            i++;
            token = strtok(NULL , " ");
        }

        pid_t exec_pid = -1;
        int status = 0;
        int pipe_fd[2];

        if (pipe(pipe_fd) == -1) {
            perror("Eroare la crearea pipe-ului");
            return;
        }

        if ((exec_pid = fork()) < 0) {
            perror("Eroare la fork");
            exit(1);
        } else if (exec_pid == 0) {
            close(pipe_fd[0]);

            dup2(pipe_fd[1], STDOUT_FILENO);
            close(pipe_fd[1]);

            if (strcmp(word[0], "list") == 0) {
                execl(TREASURE_MANAGER_EXEC, TREASURE_MANAGER_EXEC, word[0], word[1], NULL);
                perror("Eroare la execl");
                exit(1);
            } else if (strcmp(word[0], "view") == 0) {
                execl(TREASURE_MANAGER_EXEC, TREASURE_MANAGER_EXEC, word[0], word[1], word[2], NULL);
                perror("Eroare la execl");
                exit(1);
            }
            exit(0);
        }

        close(pipe_fd[1]);

        char buffer[2048];
        ssize_t bytes_read;
        printf("--- Rezultatul comenzii: ---\n");
        while ((bytes_read = read(pipe_fd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            printf("%s", buffer);
        }
        close(pipe_fd[0]);

        if (waitpid(exec_pid, &status, 0) == -1) {
            error_exit("Eroare la waitpid");
        }

        if (!WIFEXITED(status)) {
            error_exit("Procesul copil a iesit anormal");
        }

        pid_t p_pid = getppid();
        /*if (kill(p_pid, SIGUSR2) < 0) {
            error_exit("Eroare la trimiterea SIGUSR2 catre parinte");
        }*/
    }
}

int monitor() {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        error_exit("Eroare la sigaction in monitor");
    }

    while(1) {
        if (!shutdown) {
            sleep(1);
        } else {
            printf("\nMonitorul a fost oprit\n");
            return 0;
        }
    }
}

void create_command_and_send_sig(pid_t pid, char* text) {
    write_in_command(text);
    if (kill(pid, SIGUSR1) < 0) {
        error_exit("Eroare la trimiterea SIGUSR1 catre monitor");
    }
}

void list_hunts() {
    printf("--- Lista hunt-urilor disponibile: ---\n");
    DIR *d;
    struct dirent *dir;
    d = opendir("treasure_hunts");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                printf("- %s\n", dir->d_name);
            }
        }
        closedir(d);
    } else if (errno == ENOENT) {
        printf("Nu exista inca hunt-uri create.\n");
    } else {
        perror("Eroare la deschiderea directorului treasure_hunts");
    }
}

void read_line(char* str, size_t size) {
    fgets(str, size, stdin);
    str[strcspn(str, "\n")] = '\0';
}

void list_treasures(pid_t pid) {
    char hunt_id[50] = {'\0'};
    printf("Introduceti hunt_id pentru a lista comorile: ");
    read_line(hunt_id, sizeof(hunt_id));
    char comm[60] = {'\0'};
    strcpy(comm, "list ");
    strcat(comm, hunt_id);
    create_command_and_send_sig(pid, comm);
}

void view_treasure(pid_t pid) {
    char hunt_id[50] = {'\0'};
    printf("Introduceti hunt_id pentru a vizualiza comoara: ");
    read_line(hunt_id, sizeof(hunt_id));
    char comm[120] = {'\0'};
    strcpy(comm, "view ");
    strcat(comm, hunt_id);
    char tr_id[30] = {'\0'};
    printf("Introduceti treasure_id: ");
    read_line(tr_id, sizeof(tr_id));
    strcat(comm, " ");
    strcat(comm, tr_id);
    create_command_and_send_sig(pid, comm);
}

void p_handler(int signum) {
    if (signum == SIGUSR2) {
        monitor_working = 0;
    }
}

void wait_for_monitor() {
    struct sigaction sa2;
    sa2.sa_handler = p_handler;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = 0;

    if (sigaction(SIGUSR2, &sa2, NULL) == -1) {
        perror("Eroare la sigaction in parinte");
    }

    monitor_working = 1;
    while (monitor_working) {
        sleep(1);
    }
}

int get_command() {
    char command[50] = {'\0'};
    read_line(command, sizeof(command));
    if (strcmp(command, "start_monitor") == 0) {
        return 1;
    } else if (strcmp(command, "list_hunts") == 0) {
        return 2;
    } else if (strcmp(command, "list_treasures") == 0) {
        return 3;
    } else if (strcmp(command, "view_treasure") == 0) {
        return 4;
    } else if (strcmp(command, "stop_monitor") == 0) {
        return 5;
    } else if (strcmp(command, "exit") == 0) {
        return 6;
    } else if (strcmp(command, "calculate_score") == 0) {
        return 7;
    } else {
        return 8;
    }
}

int main() {
    while (1) {
        int monitor_status = 0, waitpid_return = -1;

        printf("\n==MENIU MONITOR==\n");
        printf("| 1. start_monitor     |\n");
        printf("| 2. list_hunts        |\n");
        printf("| 3. list_treasures    |\n");
        printf("| 4. view_treasure     |\n");
        printf("| 5. stop_monitor      |\n");
        printf("| 6. exit              |\n");
        printf("| 7. calculate_score   |\n");
        printf("=====================\n");
        printf("Introduceti comanda: ");

        int option = get_command();

        if (monitor_pid == -1 && option != 1 && option != 6) {
            printf("Eroare: monitor oprit\n");
            continue;
        }

        if (monitor_pid > 0) {
            waitpid_return = waitpid(monitor_pid, &monitor_status, WNOHANG);
            if (waitpid_return == -1) {
                perror("Eroare la waitpid");
                exit(1);
            }

            if (waitpid_return == 0) {
                if (option == 5) {
                    kill(monitor_pid, SIGTERM);
                    wait_for_monitor();
                    monitor_pid = -1;
                }
            } else if (waitpid_return > 0) {
                printf("Monitorul s-a incheiat cu succes!\n");
                monitor_pid = -1;
            }
        }

        switch (option) {
            case 1:
                if (monitor_pid == -1) {
                    monitor_pid = fork();
                    if (monitor_pid == 0) {
                        if (monitor() == 0) {
                            return 0;
                        }
                    } else if (monitor_pid > 0) {
                        printf("Monitor pornit cu PID: %d\n", monitor_pid);
                    } else {
                        perror("Eroare la pornirea monitorului");
                    }
                } else {
                    printf("\nMonitorul este deja pornit.\n");
                }
                break;
            case 2:
                list_hunts();
                break;
            case 3:
                list_treasures(monitor_pid);
                break;
            case 4:
                view_treasure(monitor_pid);
                break;
            case 5:
                kill(monitor_pid, SIGTERM);
                wait_for_monitor();
                waitpid(monitor_pid, NULL, 0);
                monitor_pid = -1;
                printf("Monitorul a fost oprit cu succes.\n");
                break;
            case 6:
                if (monitor_pid > 0) {
                    printf("Opreste monitorul!\n");
                } else {
                    exit(0);
                }
                break;
            case 7: {
                char hunt_id_calc[50];
                printf("Introduceti hunt_id: ");
                read_line(hunt_id_calc, sizeof(hunt_id_calc));

                pid_t pid;
                int pipe_fd[2];

                if (pipe(pipe_fd) == -1) {
                    perror("Eroare la crearea pipe-ului");
                    break;
                }

                pid = fork();
                if (pid == -1) {
                    perror("Eroare la fork");
                    close(pipe_fd[0]);
                    close(pipe_fd[1]);
                    break;
                } else if (pid == 0) {
                    close(pipe_fd[0]);
                    dup2(pipe_fd[1], STDOUT_FILENO);
                    close(pipe_fd[1]);

                    execl("./calculate_score", "./calculate_score", hunt_id_calc, NULL);
                    perror("Eroare la execl pentru calculate_score");
                    exit(EXIT_FAILURE);
                } else {
                    close(pipe_fd[1]);
                    char buffer[2048];
                    ssize_t bytes_read;
                    printf("--- Scorul hunt-ului '%s': ---\n", hunt_id_calc);
                    while ((bytes_read = read(pipe_fd[0], buffer, sizeof(buffer) - 1)) > 0) {
                        buffer[bytes_read] = '\0';
                        printf("%s", buffer);
                    }
                    close(pipe_fd[0]);

                    int status;
                    waitpid(pid, &status, 0);
                    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                        fprintf(stderr, "Eroare la calcularea scorului.\n");
                    }
                }
                break;
            }
            default:
                printf("Comanda invalida.\n");
                break;
        }
    }

    return 0;
}