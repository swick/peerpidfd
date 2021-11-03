#define _GNU_SOURCE 1
#include <unistd.h>
#include <stdio.h>
#include <wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/syscall.h>
#include <limits.h>
#include <string.h>

#define SO_PEERPIDFD   22

static inline int sys_pidfd_send_signal(int pidfd, int sig, siginfo_t *info,
					unsigned int flags)
{
	return syscall(__NR_pidfd_send_signal, pidfd, sig, info, flags);
}

/* Returns the number of chars needed to format variables of the
 * specified type as a decimal string. Adds in extra space for a
 * negative '-' prefix (hence works correctly on signed
 * types). Includes space for the trailing NUL. */
#define DECIMAL_STR_MAX(type)                                           \
        (2U+(sizeof(type) <= 1 ? 3U :                                   \
             sizeof(type) <= 2 ? 5U :                                   \
             sizeof(type) <= 4 ? 10U :                                  \
             sizeof(type) <= 8 ? 20U : sizeof(int[-2*(sizeof(type) > 8)])))

#define STRLEN(x) (sizeof(""x"") - sizeof(typeof(x[0])))


#define SOCKET_NAME "/my-socket"
#define EVIL_EXE "/evil"
#define GOOD_EXE "/peerpidfd"


static int pidfd_valid(int pidfd)
{
	int res;

	res = sys_pidfd_send_signal(pidfd, 0, NULL, 0);
	if (res >= 0)
		return 1;

	switch (errno) {
	case EPERM:
		return 1;
		break;
	default:
		return 0;
	}
}

static void get_exe_path(int pid, char *buffer, size_t buf_size)
{
	int res;
        char path[STRLEN("/proc/") + DECIMAL_STR_MAX(int) + STRLEN("/exe")];
	sprintf(path, "/proc/%i/exe", pid);

	res = readlink(path, buffer, buf_size - 1);
	if (res < 0) {
		perror("readlink");
		exit(EXIT_FAILURE);
	}

	buffer[res] = '\0';
}

int run_reuse_attack(int target_pid)
{
	int pid = -1;
	do {
		pid = fork();
		if (pid < 0) {
			perror("fork");
			exit(EXIT_FAILURE);
		} else if (pid == 0) {
			_exit(0);
		}
		waitpid(pid, NULL, 0);
	} while (pid > target_pid || pid < target_pid - 1);

	pid = fork();
        if (pid == 0) {
                execl(EVIL_EXE, NULL);
                perror("exec evil");
		exit(EXIT_FAILURE);
	}
        if (pid < 0) {
                perror("fork");
		exit(EXIT_FAILURE);
        }

	if (pid != target_pid) {
		printf("reuse attack failed!\n");
	}

	return pid;
}

void run_client() {
	struct sockaddr_un addr;
	int sock;
	int res;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCKET_NAME, sizeof(addr.sun_path) - 1);

	res = connect(sock, (const struct sockaddr *) &addr, sizeof(addr));
	if (res == -1) {
		fprintf(stderr, "server not available\n");
		exit(EXIT_FAILURE);
	}

	close(sock);
	
	sleep(1);

	exit(EXIT_SUCCESS);

}

int main() {
	struct sockaddr_un addr;
	int res;
	int sock;
	int client;
	struct ucred cred;
	socklen_t solen;
        pid_t pid;
        int pidfd;
        int pidfrompidfd;
        int evilpid;
        char exe_path[PATH_MAX];

	printf("setting up a unix socket and figuring out if the client "
	       "executable path we read on the server is actually from the "
	       "correct process or from a process with a reused PID\n");

	/* for some reason low pids dont get reused? */
	evilpid = run_reuse_attack(1050);
	waitpid(evilpid, NULL, 0);

	/* setting up the socket */
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("socket: ");
		exit(EXIT_FAILURE);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCKET_NAME, sizeof(addr.sun_path) - 1);

	res = bind(sock, (const struct sockaddr *)&addr, sizeof(addr));
	if (res == -1) {
		perror("bind");
		exit(EXIT_FAILURE);
	}

	res = listen(sock, 8);
	if (res == -1) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	/* our real client */
        pid = fork();
        if (pid == 0) {
        	run_client();
        	exit(EXIT_SUCCESS);
	}

        if (pid < 0) {
                perror("fork");
		exit(EXIT_FAILURE);
        }

	/* accept the connection from the client */
	client = accept(sock, NULL, NULL);
	if (client == -1) {
		perror("accept");
		exit(EXIT_FAILURE);
	}

	/* get the pid */
	solen = sizeof(cred);
	res = getsockopt(client, SOL_SOCKET, SO_PEERCRED, &cred, &solen);
	if (res == -1) {
		perror("getsockopt SO_PEERCRED");
		exit(EXIT_FAILURE);
	}

	/* get the pidfd */
	solen = sizeof(pidfd);
	res = getsockopt(client, SOL_SOCKET, SO_PEERPIDFD, &pidfd, &solen);
	if (res == -1) {
		perror("getsockopt SO_PEERPIDFD");
		exit(EXIT_FAILURE);
	}

	printf("client PID %d from getsockopt, %d from fork\n", cred.pid, pid);
	if (cred.pid != pid) {
		fprintf(stderr, "SO_PEERCRED pid and fork pid are different. Should not happen!\n");
		exit(EXIT_FAILURE);
	}

	get_exe_path(cred.pid, exe_path, sizeof(exe_path));
	printf("got executable path %s and pidfd says the path is%s valid\n", exe_path, pidfd_valid(pidfd) == 0 ? " NOT" : "");
	if (strcmp(exe_path, GOOD_EXE) != 0) {
		fprintf(stderr, "executable path should be good but is not\n");
		exit(EXIT_FAILURE);
	}

	close(client);
	waitpid(pid, NULL, 0);

	printf("running a PID reuse attack!\n");
	evilpid = run_reuse_attack(pid);
	printf("PID %d is evil now!\n", pid);
	sleep(1);

	get_exe_path(cred.pid, exe_path, sizeof(exe_path));
	printf("got executable path %s and pidfd says the path is%s valid\n", exe_path, pidfd_valid(pidfd) == 0 ? " NOT" : "");
	if (strcmp(exe_path, EVIL_EXE) != 0) {
		fprintf(stderr, "executable path should be evil but is not\n");
		exit(EXIT_FAILURE);
	}

	waitpid(evilpid, NULL, 0);

	close(sock);
	unlink(SOCKET_NAME);

	close(pidfd);

	printf("everything as expected!\n");
	exit(EXIT_SUCCESS);
}
