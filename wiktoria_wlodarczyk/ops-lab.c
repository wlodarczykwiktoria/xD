#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MIN_STUDENTS 4
#define MAX_STUDENTS 20

#define BUFFER_SIZE 256

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)             \
    (__extension__({                               \
        long int __result;                         \
        do                                         \
            __result = (long int)(expression);     \
        while (__result == -1L && errno == EINTR); \
        __result;                                  \
    }))
#endif

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void student_work(int pipe_fd)
{
    pid_t pid = getpid();
    printf("Student: %d\n", pid);
    srand(pid);

    char buffer[BUFFER_SIZE];

    if (read(pipe_fd, buffer, BUFFER_SIZE) == -1)
        ERR("student pipe - read");

    if (atoi(buffer) == pid)
    {
        snprintf(buffer, BUFFER_SIZE, "Student %d: HERE", pid);
        if (write(pipe_fd, buffer, strlen(buffer) + 1) == -1)
            ERR("student pipe - write");
        printf("%s\n", buffer);
    }
}

void teacher_work(int *student_pipes, int n)
{
    pid_t pid = getpid();
    printf("Teacher: %d\n", pid);
    srand(pid);

    char buffer[BUFFER_SIZE];

    for (int i = 0; i < n; ++i)
    {
        snprintf(buffer, BUFFER_SIZE, "%d", student_pipes[i]);
        if (write(student_pipes[i], buffer, strlen(buffer) + 1) == -1)
            ERR("teacher pipe - write");
        printf("Teacher: Is %d here?\n", pid);
    }

    for (int i = 0; i < n; ++i)
    {
        if (read(student_pipes[i], buffer, BUFFER_SIZE) == -1)
            ERR("teacher pipe -read");
        printf("%s\n", buffer);
    }
}

void create_students(const int n)
{
    int student_pipes[n][2];

    for (int i = 0; i < n; ++i)
    {
        if (pipe(student_pipes[i]) == -1)
            ERR("pipe");
    }

    for (int i = 0; i < n; ++i)
    {
        pid_t student_pid = fork();

        if (student_pid == -1)
        {
            ERR("fork");
            exit(EXIT_FAILURE);
        }
        else if (student_pid == 0)
        {
            for (int j = 0; j < n; ++j)
            {
                if (j != i)
                    close(student_pipes[j][1]);
            }
            student_work(student_pipes[i][0]);
            exit(EXIT_SUCCESS);
        }
        else
        {
            close(student_pipes[i][0]);
        }
    }
    teacher_work((int *)student_pipes, n);
}

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s N M\n", name);
    fprintf(stderr, "N: 4 <= N <= 20 - number of students\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int n;
    if (argc != 2)
        usage(argv[0]);
    n = atoi(argv[1]);
    if (n < MIN_STUDENTS || n > MAX_STUDENTS)
        usage(argv[0]);

    create_students(n);

    int status;
    while (wait(&status) > 0);

    teacher_work();
}
