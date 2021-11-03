#define RUN_BINARY "peerpidfd"
#define MOUNT_PROCFS 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <sys/mount.h>
#include <sys/stat.h>

int main(void) {
        pid_t pid = 0;
        int status;

        printf("Hello world!\n");
        printf("Setting up environment…\n");

#if MOUNT_PROCFS
	if (mkdir("/proc", 0755) != 0) {
		perror("mkdir /proc");
		goto failure;
	}
	if (mount("none", "/proc", "proc", 0, "") != 0) {
		perror("mount procfs");
		goto failure;
	}
#endif

        printf("Executing " RUN_BINARY "\n");

        pid = fork();
        if (pid == 0) {
                execl("/" RUN_BINARY, NULL);
                perror("In exec(): ");
		goto failure;
        }
        if (pid > 0) {
                pid = wait(&status);
                printf(RUN_BINARY " terminated\n");
                if (WIFEXITED(status)) {
                        printf("ended with exit(%d).\n", WEXITSTATUS(status));
                }
                if (WIFSIGNALED(status)) {
                        printf("ended with kill -%d.\n", WTERMSIG(status));
                }
        }
        if (pid < 0) {
                perror("In fork():");
		goto failure;
        }

	goto success;

failure:
	printf("aborting!\n!!!!!!!!!!!!!!!!!!!\n");

success:
        reboot(RB_POWER_OFF);
}
