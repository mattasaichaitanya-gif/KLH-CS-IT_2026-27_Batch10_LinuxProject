/**
 * ============================================================================
 * Project: Inter-Process Chat Application Using IPC (POSIX Named Pipes)
 * File: chat.c
 * Description: Terminal-based, full-duplex peer-to-peer chat application
 *              demonstrating Linux IPC using FIFOs, I/O multiplexing (select),
 *              and process management (fork/waitpid).
 * Standard: POSIX.1-2008 / C11
 * ============================================================================
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>
#include <ctype.h>

/* Constants and Configuration */
#define PROTOCOL_MAGIC      0x49504331U /* ASCII "IPC1" */
#define MAX_USERNAME_LEN    32
#define MAX_MESSAGE_LEN     1024
#define MAX_PATH_LEN        256
#define MAX_SESSION_LEN     32
#define DEFAULT_SESSION_ID  "default"

/* Packet Types */
typedef enum {
    MSG_TYPE_CHAT  = 1,
    MSG_TYPE_EXIT  = 2,
    MSG_TYPE_JOIN  = 3
} MessageType;

/* Protocol Packet (Packed, fits well within PIPE_BUF 4096 bytes) */
typedef struct {
    uint32_t magic;
    uint32_t type;
    uint32_t payload_len;
    char     sender[MAX_USERNAME_LEN];
    char     text[MAX_MESSAGE_LEN];
} ChatPacket;

/* Execution Modes */
typedef enum {
    MODE_SELECT = 0,
    MODE_FORK   = 1
} ConcurrencyMode;

/* Global State for Signal Management and Resource Cleanup */
static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_child_pid = 0;

static char g_fifo_1[MAX_PATH_LEN] = {0};
static char g_fifo_2[MAX_PATH_LEN] = {0};
static char g_lock_path[MAX_PATH_LEN] = {0};

static int g_read_fd  = -1;
static int g_write_fd = -1;
static int g_lock_fd  = -1;
static int g_role     = 0;

/* Signal Handler */
static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
    if (g_child_pid > 0) {
        /* Forward signal to child process if running in fork mode */
        kill(g_child_pid, SIGTERM);
    }
}

/* Child process termination handler */
static void handle_sigchld(int sig) {
    (void)sig;
    int saved_errno = errno;
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == g_child_pid) {
            g_child_pid = 0;
            g_running = 0; /* Reader exited -> trigger session end */
        }
    }
    errno = saved_errno;
}

/**
 * write_all: Writes exact count bytes to fd, handling partial writes and EINTR.
 */
static ssize_t write_all(int fd, const void *buf, size_t count) {
    size_t total_written = 0;
    const char *ptr = (const char *)buf;

    while (total_written < count) {
        ssize_t n = write(fd, ptr + total_written, count - total_written);
        if (n < 0) {
            if (errno == EINTR) {
                if (!g_running) return -1;
                continue;
            }
            return -1; /* EPIPE or other socket/pipe failure */
        }
        if (n == 0) {
            break;
        }
        total_written += (size_t)n;
    }
    return (ssize_t)total_written;
}

/**
 * read_all: Reads exact count bytes from fd, handling partial reads and EINTR.
 * Returns count on success, 0 on EOF, or -1 on error.
 */
static ssize_t read_all(int fd, void *buf, size_t count) {
    size_t total_read = 0;
    char *ptr = (char *)buf;

    while (total_read < count) {
        ssize_t n = read(fd, ptr + total_read, count - total_read);
        if (n < 0) {
            if (errno == EINTR) {
                if (!g_running) return -1;
                continue;
            }
            return -1;
        }
        if (n == 0) {
            /* EOF: peer closed their end */
            return (ssize_t)total_read;
        }
        total_read += (size_t)n;
    }
    return (ssize_t)total_read;
}

/**
 * build_packet: Safely initializes a ChatPacket with bounds checking and null termination.
 */
static void build_packet(ChatPacket *pkt, MessageType type, const char *sender, const char *text) {
    memset(pkt, 0, sizeof(*pkt));
    pkt->magic = PROTOCOL_MAGIC;
    pkt->type  = (uint32_t)type;

    if (sender) {
        size_t ulen = strlen(sender);
        if (ulen >= MAX_USERNAME_LEN) ulen = MAX_USERNAME_LEN - 1;
        memcpy(pkt->sender, sender, ulen);
        pkt->sender[ulen] = '\0';
    }

    if (text) {
        size_t tlen = strlen(text);
        if (tlen >= MAX_MESSAGE_LEN) tlen = MAX_MESSAGE_LEN - 1;
        memcpy(pkt->text, text, tlen);
        pkt->text[tlen] = '\0';
        pkt->payload_len = (uint32_t)tlen;
    }
}

/**
 * cleanup_resources: Closes file descriptors and unlinks FIFOs and lock files.
 */
static void cleanup_resources(void) {
    if (g_write_fd >= 0) {
        close(g_write_fd);
        g_write_fd = -1;
    }
    if (g_read_fd >= 0) {
        close(g_read_fd);
        g_read_fd = -1;
    }
    if (g_lock_fd >= 0) {
        /* Release lock and close */
        struct flock fl;
        memset(&fl, 0, sizeof(fl));
        fl.l_type = F_UNLCK;
        fl.l_whence = SEEK_SET;
        fcntl(g_lock_fd, F_SETLK, &fl);
        close(g_lock_fd);
        g_lock_fd = -1;
    }
    if (g_lock_path[0] != '\0') {
        unlink(g_lock_path);
        g_lock_path[0] = '\0';
    }

    /* Unlink FIFOs when role 1 exits or upon final termination */
    if (g_role == 1) {
        if (g_fifo_1[0] != '\0') {
            unlink(g_fifo_1);
        }
        if (g_fifo_2[0] != '\0') {
            unlink(g_fifo_2);
        }
    }
}

/**
 * print_usage: Explains CLI arguments and options.
 */
static void print_usage(const char *prog_name) {
    fprintf(stderr, "\n===================================================================\n");
    fprintf(stderr, " Inter-Process Chat Application (POSIX Named Pipes / IPC)\n");
    fprintf(stderr, "===================================================================\n");
    fprintf(stderr, "Usage: %s <username> <role> [session_id] [--mode=select|fork]\n\n", prog_name);
    fprintf(stderr, "Arguments:\n");
    fprintf(stderr, "  <username>    Display name of user (1-%d chars, alphanumeric/underscore)\n", MAX_USERNAME_LEN - 1);
    fprintf(stderr, "  <role>        1 = Initiator (Alice) / 2 = Joiner (Bob)\n");
    fprintf(stderr, "  [session_id]  Optional room/session name (default: '%s')\n", DEFAULT_SESSION_ID);
    fprintf(stderr, "  --mode=select Use synchronous I/O multiplexing via select() [default]\n");
    fprintf(stderr, "  --mode=fork   Use concurrent processes via fork() and waitpid()\n\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  Terminal 1:  %s Alice 1\n", prog_name);
    fprintf(stderr, "  Terminal 2:  %s Bob 2\n", prog_name);
    fprintf(stderr, "  Custom Room: %s Alice 1 roomA\n", prog_name);
    fprintf(stderr, "               %s Bob 2 roomA\n", prog_name);
    fprintf(stderr, "===================================================================\n\n");
}

/**
 * validate_string: Ensures string contains only safe alphanumeric/underscore characters.
 */
static int validate_string(const char *str, size_t max_len) {
    if (!str || str[0] == '\0') return 0;
    size_t len = strlen(str);
    if (len >= max_len) return 0;
    for (size_t i = 0; i < len; ++i) {
        if (!isalnum((unsigned char)str[i]) && str[i] != '_' && str[i] != '-') {
            return 0;
        }
    }
    return 1;
}

/**
 * acquire_session_lock: Prevents duplicate sessions from corrupting the channel.
 * Uses POSIX fcntl advisory record locking.
 */
static int acquire_session_lock(const char *session_id, int role, const char *username) {
    snprintf(g_lock_path, sizeof(g_lock_path), "/tmp/ipc_chat_%s_role%d.lock", session_id, role);

    g_lock_fd = open(g_lock_path, O_CREAT | O_RDWR, 0600);
    if (g_lock_fd < 0) {
        perror("[Error] Failed to open lock file");
        return -1;
    }

    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type   = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0; /* Lock entire file */

    if (fcntl(g_lock_fd, F_SETLK, &fl) < 0) {
        if (errno == EACCES || errno == EAGAIN) {
            fprintf(stderr, "\n[Error] Duplicate session detected!\n");
            fprintf(stderr, "An active instance for role %d in session '%s' is already running.\n", role, session_id);
            fprintf(stderr, "Cannot start user '%s' because that role is already in use.\n\n", username);
            close(g_lock_fd);
            g_lock_fd = -1;
            g_lock_path[0] = '\0';
            return -1;
        }
        perror("[Error] fcntl record lock failed");
        close(g_lock_fd);
        g_lock_fd = -1;
        g_lock_path[0] = '\0';
        return -1;
    }

    return 0;
}

/**
 * print_banner: Formats and displays startup UI banner.
 */
static void print_banner(const char *username, int role, const char *session_id, ConcurrencyMode mode) {
    printf("\n====================================\n");
    printf("       INTER-PROCESS CHAT\n");
    printf("====================================\n");
    printf("User:      %s\n", username);
    printf("Role:      %s (Role %d)\n", (role == 1) ? "Initiator" : "Joiner", role);
    printf("Session:   %s\n", session_id);
    printf("Mechanism: POSIX Named Pipes (FIFOs)\n");
    printf("Multiplex: %s\n", (mode == MODE_SELECT) ? "select() I/O Multiplexing" : "fork() & waitpid() Process Model");
    printf("Status:    Connected\n");
    printf("Type /exit to leave\n");
    printf("====================================\n\n");
    fflush(stdout);
}

/**
 * run_chat_select: Event loop using synchronous select() I/O multiplexing.
 */
static int run_chat_select(const char *username) {
    char stdin_buf[MAX_MESSAGE_LEN * 2];
    size_t stdin_buf_len = 0;
    ChatPacket in_packet;
    ChatPacket out_packet;

    while (g_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(g_read_fd, &read_fds);

        int max_fd = (STDIN_FILENO > g_read_fd) ? STDIN_FILENO : g_read_fd;

        /* Short timeout (200ms) allows checking g_running signal flag */
        struct timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 200000;

        int ready = select(max_fd + 1, &read_fds, NULL, NULL, &tv);

        if (ready < 0) {
            if (errno == EINTR) {
                if (!g_running) break;
                continue;
            }
            perror("[Error] select() system call failed");
            break;
        }

        if (ready == 0) {
            /* Timeout: continue loop to verify g_running status */
            continue;
        }

        /* 1. Incoming Message from Peer over FIFO */
        if (FD_ISSET(g_read_fd, &read_fds)) {
            memset(&in_packet, 0, sizeof(in_packet));
            ssize_t n = read_all(g_read_fd, &in_packet, sizeof(in_packet));

            if (n <= 0) {
                /* Peer closed pipe (EOF) or error occurred */
                printf("\n[Notice] Peer disconnected. Exiting chat...\n");
                break;
            }

            if (n < (ssize_t)sizeof(in_packet)) {
                /* Partial/corrupted packet on disconnect */
                printf("\n[Notice] Incomplete packet received. Peer disconnected.\n");
                break;
            }

            if (in_packet.magic != PROTOCOL_MAGIC) {
                fprintf(stderr, "\n[Warning] Received malformed packet (invalid magic number).\n");
                continue;
            }

            if (in_packet.type == MSG_TYPE_EXIT) {
                printf("\n[Notice] %s has left the chat session (/exit).\n", in_packet.sender);
                break;
            } else if (in_packet.type == MSG_TYPE_CHAT) {
                /* Ensure null termination for safe printing */
                in_packet.sender[MAX_USERNAME_LEN - 1] = '\0';
                in_packet.text[MAX_MESSAGE_LEN - 1] = '\0';

                printf("%s: %s\n", in_packet.sender, in_packet.text);
                fflush(stdout);
            }
        }

        /* 2. User Input from Keyboard (stdin) */
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            ssize_t bytes = read(STDIN_FILENO, stdin_buf + stdin_buf_len, sizeof(stdin_buf) - 1 - stdin_buf_len);
            if (bytes <= 0) {
                if (bytes == 0) {
                    /* EOF from stdin (Ctrl+D) -> exit gracefully */
                    printf("\n[Notice] EOF detected on input. Sending exit notification...\n");
                    build_packet(&out_packet, MSG_TYPE_EXIT, username, NULL);
                    write_all(g_write_fd, &out_packet, sizeof(out_packet));
                    break;
                }
                if (errno == EINTR) {
                    if (!g_running) break;
                    continue;
                }
                perror("[Error] read(stdin) failed");
                break;
            }
            stdin_buf_len += (size_t)bytes;
            stdin_buf[stdin_buf_len] = '\0';

            /* Process all complete lines in stdin_buf */
            char *line_start = stdin_buf;
            char *newline_pos = NULL;

            while ((newline_pos = strchr(line_start, '\n')) != NULL) {
                *newline_pos = '\0';
                char *msg = line_start;

                /* Strip carriage return if present */
                size_t mlen = strlen(msg);
                if (mlen > 0 && msg[mlen - 1] == '\r') {
                    msg[mlen - 1] = '\0';
                    mlen--;
                }

                if (mlen > 0) {
                    if (strcmp(msg, "/exit") == 0) {
                        printf("[Notice] Leaving chat session...\n");
                        build_packet(&out_packet, MSG_TYPE_EXIT, username, NULL);
                        write_all(g_write_fd, &out_packet, sizeof(out_packet));
                        g_running = 0;
                        break;
                    }

                    build_packet(&out_packet, MSG_TYPE_CHAT, username, msg);
                    ssize_t written = write_all(g_write_fd, &out_packet, sizeof(out_packet));
                    if (written < 0) {
                        if (errno == EPIPE) {
                            printf("\n[Notice] Peer disconnected (broken pipe). Cannot send message.\n");
                        } else {
                            perror("[Error] write() to FIFO failed");
                        }
                        g_running = 0;
                        break;
                    }

                    printf("%s: %s\n", username, msg);
                    fflush(stdout);
                }

                line_start = newline_pos + 1;
            }

            /* Shift remaining incomplete line to start of buffer */
            if (line_start > stdin_buf) {
                size_t remaining = strlen(line_start);
                memmove(stdin_buf, line_start, remaining + 1);
                stdin_buf_len = remaining;
            }
        }
    }

    return 0;
}

/**
 * run_chat_fork: Concurrency model using fork() and waitpid().
 * Spawns a dedicated child process to read from FIFO while parent reads stdin.
 */
static int run_chat_fork(const char *username) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("[Error] fork() failed");
        return -1;
    }

    if (pid == 0) {
        /* ================= CHILD PROCESS: Reader ================= */
        /* Set child signal handling */
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);

        ChatPacket in_packet;
        while (1) {
            memset(&in_packet, 0, sizeof(in_packet));
            ssize_t n = read_all(g_read_fd, &in_packet, sizeof(in_packet));

            if (n <= 0) {
                printf("\n[Notice] Peer disconnected. Exiting...\n");
                fflush(stdout);
                break;
            }

            if (n < (ssize_t)sizeof(in_packet)) {
                break;
            }

            if (in_packet.magic != PROTOCOL_MAGIC) {
                continue;
            }

            if (in_packet.type == MSG_TYPE_EXIT) {
                printf("\n[Notice] %s has left the chat session (/exit).\n", in_packet.sender);
                fflush(stdout);
                break;
            } else if (in_packet.type == MSG_TYPE_CHAT) {
                in_packet.sender[MAX_USERNAME_LEN - 1] = '\0';
                in_packet.text[MAX_MESSAGE_LEN - 1] = '\0';
                printf("%s: %s\n", in_packet.sender, in_packet.text);
                fflush(stdout);
            }
        }
        _exit(0);
    } else {
        /* ================= PARENT PROCESS: Writer ================= */
        g_child_pid = pid;

        /* Register SIGCHLD to catch child reader termination */
        struct sigaction sa_chld;
        memset(&sa_chld, 0, sizeof(sa_chld));
        sa_chld.sa_handler = handle_sigchld;
        sigaction(SIGCHLD, &sa_chld, NULL);

        char input_line[MAX_MESSAGE_LEN];
        ChatPacket out_packet;

        while (g_running) {
            /* Check if child reader is still alive */
            int status;
            pid_t wp = waitpid(pid, &status, WNOHANG);
            if (wp != 0) {
                /* Child exited (e.g. peer closed pipe) */
                break;
            }

            if (!fgets(input_line, sizeof(input_line), stdin)) {
                /* EOF or interrupted */
                if (errno == EINTR && !g_running) break;
                printf("\n[Notice] Input ended. Sending exit packet...\n");
                build_packet(&out_packet, MSG_TYPE_EXIT, username, NULL);
                write_all(g_write_fd, &out_packet, sizeof(out_packet));
                break;
            }

            size_t len = strlen(input_line);
            while (len > 0 && (input_line[len - 1] == '\n' || input_line[len - 1] == '\r')) {
                input_line[len - 1] = '\0';
                len--;
            }

            if (len == 0) continue;

            if (strcmp(input_line, "/exit") == 0) {
                printf("[Notice] Leaving chat session...\n");
                build_packet(&out_packet, MSG_TYPE_EXIT, username, NULL);
                write_all(g_write_fd, &out_packet, sizeof(out_packet));
                break;
            }

            build_packet(&out_packet, MSG_TYPE_CHAT, username, input_line);

            ssize_t written = write_all(g_write_fd, &out_packet, sizeof(out_packet));
            if (written < 0) {
                if (errno == EPIPE) {
                    printf("\n[Notice] Peer disconnected (broken pipe).\n");
                } else {
                    perror("[Error] write() to FIFO failed");
                }
                break;
            }

            printf("%s: %s\n", username, input_line);
            fflush(stdout);
        }

        /* Reap child process */
        if (g_child_pid > 0) {
            kill(g_child_pid, SIGTERM);
            int status;
            waitpid(g_child_pid, &status, 0);
            g_child_pid = 0;
        }
    }

    return 0;
}

/**
 * main: Entry point for the Inter-Process Chat Application.
 */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *username = argv[1];
    if (!validate_string(username, MAX_USERNAME_LEN)) {
        fprintf(stderr, "[Error] Invalid username '%s'. Must be 1-%d alphanumeric/underscore characters.\n",
                username, MAX_USERNAME_LEN - 1);
        return 1;
    }

    char *endptr = NULL;
    long role_val = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || (role_val != 1 && role_val != 2)) {
        fprintf(stderr, "[Error] Invalid role '%s'. Must be 1 (Initiator) or 2 (Joiner).\n", argv[2]);
        print_usage(argv[0]);
        return 1;
    }
    g_role = (int)role_val;

    const char *session_id = DEFAULT_SESSION_ID;
    ConcurrencyMode mode = MODE_SELECT;

    for (int i = 3; i < argc; ++i) {
        if (strncmp(argv[i], "--mode=", 7) == 0) {
            const char *m = argv[i] + 7;
            if (strcmp(m, "select") == 0) {
                mode = MODE_SELECT;
            } else if (strcmp(m, "fork") == 0) {
                mode = MODE_FORK;
            } else {
                fprintf(stderr, "[Error] Unknown mode '%s'. Use --mode=select or --mode=fork.\n", m);
                return 1;
            }
        } else {
            session_id = argv[i];
            if (!validate_string(session_id, MAX_SESSION_LEN)) {
                fprintf(stderr, "[Error] Invalid session_id '%s'. Must be 1-%d alphanumeric characters.\n",
                        session_id, MAX_SESSION_LEN - 1);
                return 1;
            }
        }
    }

    /* 1. Prevent Broken-Pipe Signal Termination */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("[Error] Failed to set SIGPIPE to SIG_IGN");
        return 1;
    }

    /* 2. Configure Signal Handlers for Clean Termination */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /* Do not use SA_RESTART so blocking syscalls wake up */

    if (sigaction(SIGINT, &sa, NULL) < 0 ||
        sigaction(SIGTERM, &sa, NULL) < 0 ||
        sigaction(SIGHUP, &sa, NULL) < 0) {
        perror("[Error] Failed to install signal handlers");
        return 1;
    }

    /* 3. Acquire Advisory Session Lock (Prevents Duplicate Instances) */
    if (acquire_session_lock(session_id, g_role, username) < 0) {
        return 1;
    }

    /* 4. Construct FIFO Paths */
    snprintf(g_fifo_1, sizeof(g_fifo_1), "/tmp/ipc_chat_%s_1to2.fifo", session_id);
    snprintf(g_fifo_2, sizeof(g_fifo_2), "/tmp/ipc_chat_%s_2to1.fifo", session_id);

    /* 5. Create FIFOs (Ignore EEXIST if already present) */
    if (mkfifo(g_fifo_1, 0666) < 0 && errno != EEXIST) {
        perror("[Error] mkfifo(fifo_1) failed");
        cleanup_resources();
        return 1;
    }
    if (mkfifo(g_fifo_2, 0666) < 0 && errno != EEXIST) {
        perror("[Error] mkfifo(fifo_2) failed");
        cleanup_resources();
        return 1;
    }

    /* 6. Deadlock-Free Complementary FIFO Opening Sequence */
    if (g_role == 1) {
        printf("[Info] User '%s' started as Role 1 (Initiator).\n", username);
        printf("[Info] Session ID: '%s'\n", session_id);
        printf("[Info] Waiting for peer to connect...\n");
        fflush(stdout);

        /* Role 1 opens FIFO 1 (1to2) for writing (blocks until Role 2 opens FIFO 1 for reading) */
        g_write_fd = open(g_fifo_1, O_WRONLY);
        if (g_write_fd < 0) {
            if (errno == EINTR) {
                printf("\n[Info] Interrupted before peer connected.\n");
            } else {
                perror("[Error] open(fifo_1, O_WRONLY) failed");
            }
            cleanup_resources();
            return 1;
        }

        if (!g_running) {
            cleanup_resources();
            return 0;
        }

        /* Role 1 opens FIFO 2 (2to1) for reading (blocks until Role 2 opens FIFO 2 for writing) */
        g_read_fd = open(g_fifo_2, O_RDONLY);
        if (g_read_fd < 0) {
            if (errno == EINTR) {
                printf("\n[Info] Interrupted before peer connected.\n");
            } else {
                perror("[Error] open(fifo_2, O_RDONLY) failed");
            }
            cleanup_resources();
            return 1;
        }
    } else {
        printf("[Info] User '%s' started as Role 2 (Joiner).\n", username);
        printf("[Info] Session ID: '%s'\n", session_id);
        printf("[Info] Connecting to peer...\n");
        fflush(stdout);

        /* Role 2 opens FIFO 1 (1to2) for reading (unblocks Role 1's write open) */
        g_read_fd = open(g_fifo_1, O_RDONLY);
        if (g_read_fd < 0) {
            if (errno == EINTR) {
                printf("\n[Info] Interrupted before peer connected.\n");
            } else {
                perror("[Error] open(fifo_1, O_RDONLY) failed");
            }
            cleanup_resources();
            return 1;
        }

        if (!g_running) {
            cleanup_resources();
            return 0;
        }

        /* Role 2 opens FIFO 2 (2to1) for writing (unblocks Role 1's read open) */
        g_write_fd = open(g_fifo_2, O_WRONLY);
        if (g_write_fd < 0) {
            if (errno == EINTR) {
                printf("\n[Info] Interrupted before peer connected.\n");
            } else {
                perror("[Error] open(fifo_2, O_WRONLY) failed");
            }
            cleanup_resources();
            return 1;
        }
    }

    if (!g_running) {
        cleanup_resources();
        return 0;
    }

    /* 7. Display Welcome Banner */
    print_banner(username, g_role, session_id, mode);

    /* 8. Execute Event Loop based on Selected Concurrency Mode */
    int ret = 0;
    if (mode == MODE_SELECT) {
        ret = run_chat_select(username);
    } else {
        ret = run_chat_fork(username);
    }

    /* 9. Final Cleanup and Exit */
    printf("\n[Info] Cleaning up session resources...\n");
    cleanup_resources();
    printf("[Info] Session terminated successfully.\n");

    return ret;
}
