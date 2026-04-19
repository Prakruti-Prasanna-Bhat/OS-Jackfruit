#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <pthread.h>
#include <fcntl.h>

#define STACK_SIZE (1024 * 1024)
#define MAX_CONTAINERS 10

// ================= STRUCT =================
typedef struct {
    int id;
    pid_t pid;
    char state[16];
} container_t;

container_t containers[MAX_CONTAINERS];
int container_count = 0;

// ================= LOGGER THREAD =================
void* log_reader(void *arg) {
    int fd = *(int*)arg;

    static int log_id = 0;
    char filename[64];
    sprintf(filename, "logs/container_%d.log", log_id++);

    int log_fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd < 0) {
        perror("log open failed");
        return NULL;
    }

    char buffer[1024];
    int n;

    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        write(log_fd, buffer, n);
    }

    close(log_fd);
    close(fd);
    return NULL;
}

// ================= CHILD FUNCTION =================
int child_func(void *arg) {
    int *pipefd = (int *)arg;

    close(pipefd[0]);

    // redirect stdout + stderr
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);

    close(pipefd[1]);

    // disable buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    // isolation setup
    sethostname("mycontainer", 11);
    mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);

    mkdir("/proc", 0555);
    mount("proc", "/proc", "proc", 0, NULL);

    // run non-interactive commands (stable logging)
    printf("Inside container!\n");
    execlp("/bin/sh", "/bin/sh", "-c", "echo hello; ls; sleep 1", NULL);

    perror("exec failed");
    return 1;
}

// ================= SIGNAL HANDLER =================
void sigchld_handler(int sig) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < container_count; i++) {
            if (containers[i].pid == pid) {
                strcpy(containers[i].state, "STOPPED");
                printf("\nContainer %d exited\n", containers[i].id);
            }
        }
    }
}

// ================= RUN CONTAINER =================
void run_container() {
    if (container_count >= MAX_CONTAINERS) {
        printf("Max containers reached\n");
        return;
    }

    int pipefd[2];
    pipe(pipefd);

    void *stack = malloc(STACK_SIZE);

    pid_t pid = clone(child_func,
                      stack + STACK_SIZE,
                      CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD,
                      pipefd);

    if (pid < 0) {
        perror("clone failed");
        return;
    }

    // parent
    close(pipefd[1]);

    pthread_t tid;
    pthread_create(&tid, NULL, log_reader, &pipefd[0]);

    containers[container_count].id = container_count;
    containers[container_count].pid = pid;
    strcpy(containers[container_count].state, "RUNNING");

    printf("Container %d started (PID %d)\n", container_count, pid);

    container_count++;
}

// ================= LIST =================
void list_containers() {
    printf("\nID\tPID\tSTATE\n");
    for (int i = 0; i < container_count; i++) {
        printf("%d\t%d\t%s\n",
               containers[i].id,
               containers[i].pid,
               containers[i].state);
    }
}

// ================= MAIN =================
int main() {
    signal(SIGCHLD, sigchld_handler);

    mkdir("logs", 0755);

    char cmd[50];

    while (1) {
        printf("\nEnter command (run / ps / exit): ");
        scanf("%s", cmd);

        if (strcmp(cmd, "run") == 0) {
            run_container();
        }
        else if (strcmp(cmd, "ps") == 0) {
            list_containers();
        }
        else if (strcmp(cmd, "exit") == 0) {
            break;
        }
        else {
            printf("Unknown command\n");
        }
    }

    return 0;
}
