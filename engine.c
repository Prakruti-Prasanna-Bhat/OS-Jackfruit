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
    char log_file[64];
    int exit_code;
    int stop_requested;
} container_t;

container_t containers[MAX_CONTAINERS];
int container_count = 0;

typedef struct {
    int fd;
    int container_id;
} log_arg_t;
// ================= LOGGER THREAD =================
void* log_reader(void *arg) {
    log_arg_t *logarg = (log_arg_t *)arg;
    int fd = logarg->fd;
    int cid = logarg->container_id;

    char filename[64];
    sprintf(filename, "logs/container_%d.log", cid);

    int log_fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd < 0) {
        perror("log open failed");
        free(logarg);
        return NULL;
    }

    char buffer[1024];
    int n;

    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        write(log_fd, buffer, n);
    }

    close(log_fd);
    close(fd);
    free(logarg);
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

    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    // ================= ISOLATION =================
    sethostname("mycontainer", 11);

    // mount namespace
    mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);

    // ROOTFS ISOLATION
    if (chroot("rootfs-alpha") != 0) {
        perror("chroot failed");
        return 1;
    }

    if (chdir("/") != 0) {
        perror("chdir failed");
        return 1;
    }

    // mount proc inside container
    mkdir("/proc", 0555);
    mount("proc", "/proc", "proc", 0, NULL);

    // ================= WORKLOAD =================
    printf("Inside container!\n");

    execl("/bin/sh", "/bin/sh", "-c", "echo hello; ls /; sleep 5", NULL);

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
                if (WIFEXITED(status)) {
                  strcpy(containers[i].state, "EXITED");
                  containers[i].exit_code = WEXITSTATUS(status);
                } 
                else if (WIFSIGNALED(status)) {
                  strcpy(containers[i].state, "KILLED");
                  containers[i].exit_code = 128 + WTERMSIG(status);
                } 
                else {
                strcpy(containers[i].state, "STOPPED");
                containers[i].exit_code = -1;
                }
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
    log_arg_t *logarg = malloc(sizeof(log_arg_t));
    logarg->fd = pipefd[0];
    logarg->container_id = container_count;
    pthread_create(&tid, NULL, log_reader, logarg);

    containers[container_count].id = container_count;
    containers[container_count].pid = pid;
    strcpy(containers[container_count].state, "RUNNING");
    sprintf(containers[container_count].log_file, "logs/container_%d.log", container_count);
    containers[container_count].exit_code = -1;
    containers[container_count].stop_requested = 0;
    printf("Container %d started (PID %d)\n", container_count, pid);

    container_count++;
}

// ================= LIST =================
void list_containers() {
    printf("\nID\tPID\tSTATE\tEXIT\tLOG FILE\n");
    for (int i = 0; i < container_count; i++) {
        printf("%d\t%d\t%s\t%d\t%s\n",
               containers[i].id,
               containers[i].pid,
               containers[i].state,
               containers[i].exit_code,
               containers[i].log_file);
    }
}
void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s run\n"
        "  %s ps\n",
        prog, prog);
}
// ================= MAIN =================
int main(int argc, char *argv[]) {
    signal(SIGCHLD, sigchld_handler);

    mkdir("logs", 0755);

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "run") == 0) {
        if (argc != 2) {
            print_usage(argv[0]);
            return 1;
        }

        run_container();

        // temporary wait so child can finish and logs can be written
        sleep(6);
        return 0;
    }
    else if (strcmp(argv[1], "ps") == 0) {
        if (argc != 2) {
            print_usage(argv[0]);
            return 1;
        }

        list_containers();
        return 0;
    }
    else {
        print_usage(argv[0]);
        return 1;
    }
}
