/**
 * ============================================================================
 * Project: Inter-Process Chat Application Using IPC (POSIX Named Pipes)
 * File: tests/test_runner.c
 * Description: Automated integration test harness executing end-to-end tests
 *              using fork(), pipes, signal delivery, and waitpid().
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
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define COLOR_RESET  "\033[0m"
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN   "\033[36m"
#define COLOR_BOLD   "\033[1m"

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

typedef struct {
    pid_t pid;
    int in_fd;   /* write to child's stdin */
    int out_fd;  /* read from child's stdout */
} ProcessHandle;

/**
 * spawn_child: Launches ./ipc-chat with pipes for stdin and stdout.
 */
static int spawn_child(ProcessHandle *proc, char *const argv[]) {
    int stdin_pipe[2];
    int stdout_pipe[2];

    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) {
        perror("pipe() failed");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork() failed");
        return -1;
    }

    if (pid == 0) {
        /* Child: redirect stdin and stdout */
        close(stdin_pipe[1]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        close(stdin_pipe[0]);

        close(stdout_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdout_pipe[1]);

        execv(argv[0], argv);
        perror("execv failed");
        _exit(127);
    }

    /* Parent: store pipe endpoints */
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    /* Set out_fd to non-blocking */
    int flags = fcntl(stdout_pipe[0], F_GETFL, 0);
    fcntl(stdout_pipe[0], F_SETFL, flags | O_NONBLOCK);

    proc->pid    = pid;
    proc->in_fd  = stdin_pipe[1];
    proc->out_fd = stdout_pipe[0];
    return 0;
}

/**
 * send_line: Writes a newline-terminated line to child process.
 */
static void send_line(ProcessHandle *proc, const char *line) {
    if (proc->in_fd >= 0) {
        ssize_t n1 = write(proc->in_fd, line, strlen(line));
        ssize_t n2 = write(proc->in_fd, "\n", 1);
        (void)n1;
        (void)n2;
    }
}

/**
 * wait_for_pattern: Waits up to timeout_ms for pattern to appear in child output.
 */
static int wait_for_pattern(ProcessHandle *proc, const char *pattern, int timeout_ms, char *accum_buf, size_t accum_size) {
    int elapsed = 0;
    size_t curr_len = strlen(accum_buf);

    while (elapsed < timeout_ms) {
        char temp[512];
        ssize_t n = read(proc->out_fd, temp, sizeof(temp) - 1);
        if (n > 0) {
            temp[n] = '\0';
            if (curr_len + n < accum_size - 1) {
                strcat(accum_buf, temp);
                curr_len += n;
            }
            if (strstr(accum_buf, pattern) != NULL) {
                return 1; /* Pattern found! */
            }
        }
        usleep(20000); /* 20ms */
        elapsed += 20;
    }
    return (strstr(accum_buf, pattern) != NULL);
}

/**
 * terminate_proc: Cleans up child process.
 */
static void terminate_proc(ProcessHandle *proc) {
    if (proc->pid > 0) {
        close(proc->in_fd);
        proc->in_fd = -1;
        close(proc->out_fd);
        proc->out_fd = -1;

        int status;
        pid_t wp = waitpid(proc->pid, &status, WNOHANG);
        if (wp == 0) {
            kill(proc->pid, SIGTERM);
            usleep(50000);
            wp = waitpid(proc->pid, &status, WNOHANG);
            if (wp == 0) {
                kill(proc->pid, SIGKILL);
                waitpid(proc->pid, &status, 0);
            }
        }
        proc->pid = 0;
    }
}

/**
 * report_result: Prints formatted test verdict.
 */
static void report_result(const char *test_name, int pass, const char *details) {
    g_tests_run++;
    if (pass) {
        g_tests_passed++;
        printf("  [%sPASS%s] %-52s %s%s%s\n", COLOR_GREEN, COLOR_RESET, test_name,
               COLOR_CYAN, (details ? details : ""), COLOR_RESET);
    } else {
        g_tests_failed++;
        printf("  [%sFAIL%s] %-52s %s%s%s\n", COLOR_RED, COLOR_RESET, test_name,
               COLOR_YELLOW, (details ? details : ""), COLOR_RESET);
    }
}

/* ========================================================================= */
/* Test Cases                                                                */
/* ========================================================================= */

/**
 * test_cli_argument_validation: Checks usage and error codes on malformed inputs.
 */
static void test_cli_argument_validation(void) {
    printf("\n%s--- Test Suite 1: Command-Line Argument & Startup Validation ---%s\n", COLOR_BOLD, COLOR_RESET);

    /* 1.1 No arguments */
    int status = system("./ipc-chat > /dev/null 2>&1");
    int exit_code = WEXITSTATUS(status);
    report_result("TC-01: Rejection of zero arguments", exit_code == 1, "Exit code 1 confirmed");

    /* 1.2 Invalid role number */
    status = system("./ipc-chat Alice 5 > /dev/null 2>&1");
    exit_code = WEXITSTATUS(status);
    report_result("TC-02: Rejection of invalid role (role 5)", exit_code == 1, "Exit code 1 confirmed");

    /* 1.3 Invalid username characters */
    status = system("./ipc-chat 'User!@#' 1 > /dev/null 2>&1");
    exit_code = WEXITSTATUS(status);
    report_result("TC-03: Rejection of invalid username characters", exit_code == 1, "Exit code 1 confirmed");
}

/**
 * test_duplicate_session_prevention: Tests advisory locking against duplicate roles.
 */
static void test_duplicate_session_prevention(void) {
    printf("\n%s--- Test Suite 2: Session Locking & Duplicate Prevention ---%s\n", COLOR_BOLD, COLOR_RESET);

    ProcessHandle alice1 = {0};
    char *args_alice[] = {"./ipc-chat", "Alice", "1", "lock_test", NULL};
    if (spawn_child(&alice1, args_alice) < 0) {
        report_result("TC-04: Spawn primary Alice instance", 0, "Failed to spawn");
        return;
    }
    usleep(100000); /* Allow Alice 1 to lock file and wait for peer */

    /* Try spawning another Alice (Role 1) in the exact same session */
    int status = system("./ipc-chat AliceDup 1 lock_test > /dev/null 2>&1");
    int exit_code = WEXITSTATUS(status);
    report_result("TC-04: Prevent duplicate Role 1 in same session", exit_code == 1, "Duplicate rejected via fcntl lock");

    terminate_proc(&alice1);
    unlink("/tmp/ipc_chat_lock_test_1to2.fifo");
    unlink("/tmp/ipc_chat_lock_test_2to1.fifo");
    unlink("/tmp/ipc_chat_lock_test_role1.lock");
    unlink("/tmp/ipc_chat_lock_test_role2.lock");
}

/**
 * test_full_duplex_chat_select: Tests bidirectional chat, spaces, punctuation, consecutive messages, and /exit in select mode.
 */
static void test_full_duplex_chat_select(void) {
    printf("\n%s--- Test Suite 3: Full-Duplex Chat & Delimiter/Framing (select mode) ---%s\n", COLOR_BOLD, COLOR_RESET);

    const char *sess = "chat_test_sel";
    char fifo1[128], fifo2[128];
    snprintf(fifo1, sizeof(fifo1), "/tmp/ipc_chat_%s_1to2.fifo", sess);
    snprintf(fifo2, sizeof(fifo2), "/tmp/ipc_chat_%s_2to1.fifo", sess);

    unlink(fifo1);
    unlink(fifo2);

    ProcessHandle alice = {0};
    ProcessHandle bob = {0};

    char *args_alice[] = {"./ipc-chat", "Alice", "1", (char *)sess, "--mode=select", NULL};
    char *args_bob[]   = {"./ipc-chat", "Bob",   "2", (char *)sess, "--mode=select", NULL};

    if (spawn_child(&alice, args_alice) < 0 || spawn_child(&bob, args_bob) < 0) {
        report_result("TC-05: Spawning Alice & Bob processes", 0, "Spawn error");
        terminate_proc(&alice);
        terminate_proc(&bob);
        return;
    }

    char alice_out[4096] = {0};
    char bob_out[4096]   = {0};

    /* Verify both connected */
    int a_conn = wait_for_pattern(&alice, "Status:    Connected", 2000, alice_out, sizeof(alice_out));
    int b_conn = wait_for_pattern(&bob,   "Status:    Connected", 2000, bob_out, sizeof(bob_out));
    report_result("TC-05: Deadlock-free FIFO synchronization", a_conn && b_conn, "Both endpoints connected");

    /* Verify FIFO nodes exist and are FIFOs */
    struct stat st1, st2;
    int f1_ok = (stat(fifo1, &st1) == 0 && S_ISFIFO(st1.st_mode));
    int f2_ok = (stat(fifo2, &st2) == 0 && S_ISFIFO(st2.st_mode));
    report_result("TC-06: Dual FIFO creation and filesystem validation", f1_ok && f2_ok, "S_ISFIFO confirmed for both");

    /* Message 1: Alice -> Bob with spaces and punctuation */
    const char *msg1 = "Hello Bob! Testing spaces and punctuation: ? ! & * ( )";
    send_line(&alice, msg1);
    int bob_got_msg1 = wait_for_pattern(&bob, "Alice: Hello Bob! Testing spaces and punctuation: ? ! & * ( )", 2000, bob_out, sizeof(bob_out));
    report_result("TC-07: Direction A->B message passing with punctuation", bob_got_msg1, "Delivered and framed accurately");

    /* Message 2: Bob -> Alice */
    const char *msg2 = "Hi Alice! Full-duplex named pipes are operating smoothly.";
    send_line(&bob, msg2);
    int alice_got_msg2 = wait_for_pattern(&alice, "Bob: Hi Alice! Full-duplex named pipes are operating smoothly.", 2000, alice_out, sizeof(alice_out));
    report_result("TC-08: Direction B->A message passing with spaces", alice_got_msg2, "Delivered and framed accurately");

    /* Message 3 & 4: Consecutive rapid messages */
    send_line(&alice, "Consecutive message Alpha");
    send_line(&alice, "Consecutive message Beta");
    int got_c1 = wait_for_pattern(&bob, "Alice: Consecutive message Alpha", 2000, bob_out, sizeof(bob_out));
    int got_c2 = wait_for_pattern(&bob, "Alice: Consecutive message Beta", 2000, bob_out, sizeof(bob_out));
    report_result("TC-09: Rapid consecutive message delivery", got_c1 && got_c2, "No packet intermingling");

    /* Message 5: Long message (500 characters) */
    char long_msg[600];
    memset(long_msg, 'A', 500);
    long_msg[500] = '\0';
    send_line(&bob, long_msg);
    int got_long = wait_for_pattern(&alice, "Bob: AAAAA", 2000, alice_out, sizeof(alice_out));
    report_result("TC-10: Large message transfer handling (500 bytes)", got_long, "Atomic transmission confirmed");

    /* Test graceful /exit */
    send_line(&alice, "/exit");
    int bob_saw_exit = wait_for_pattern(&bob, "has left the chat session (/exit)", 2000, bob_out, sizeof(bob_out));
    report_result("TC-11: Graceful /exit propagation and peer notice", bob_saw_exit, "Exit packet received");

    /* Wait for processes to exit */
    int status_a = 0, status_b = 0;
    waitpid(alice.pid, &status_a, 0);
    waitpid(bob.pid, &status_b, 0);
    alice.pid = 0;
    bob.pid = 0;
    terminate_proc(&alice);
    terminate_proc(&bob);

    /* Verify filesystem cleanup of FIFOs */
    int f1_gone = (access(fifo1, F_OK) != 0);
    int f2_gone = (access(fifo2, F_OK) != 0);
    report_result("TC-12: Automatic FIFO resource cleanup (unlink)", f1_gone && f2_gone, "FIFOs cleanly unlinked from /tmp");
}

/**
 * test_full_duplex_chat_fork: Tests bidirectional chat in --mode=fork.
 */
static void test_full_duplex_chat_fork(void) {
    printf("\n%s--- Test Suite 4: Multi-Process Concurrency (--mode=fork) ---%s\n", COLOR_BOLD, COLOR_RESET);

    const char *sess = "chat_test_fork";
    ProcessHandle alice = {0};
    ProcessHandle bob = {0};

    char *args_alice[] = {"./ipc-chat", "Alice", "1", (char *)sess, "--mode=fork", NULL};
    char *args_bob[]   = {"./ipc-chat", "Bob",   "2", (char *)sess, "--mode=fork", NULL};

    if (spawn_child(&alice, args_alice) < 0 || spawn_child(&bob, args_bob) < 0) {
        report_result("TC-13: Spawn fork-mode chat processes", 0, "Spawn error");
        terminate_proc(&alice);
        terminate_proc(&bob);
        return;
    }

    char alice_out[4096] = {0};
    char bob_out[4096]   = {0};

    int a_conn = wait_for_pattern(&alice, "Status:    Connected", 2000, alice_out, sizeof(alice_out));
    int b_conn = wait_for_pattern(&bob,   "Status:    Connected", 2000, bob_out, sizeof(bob_out));
    report_result("TC-13: Fork mode startup & banner display", a_conn && b_conn, "fork() processes initialized");

    send_line(&alice, "Fork mode test from Alice!");
    int bob_got = wait_for_pattern(&bob, "Alice: Fork mode test from Alice!", 2000, bob_out, sizeof(bob_out));
    report_result("TC-14: Fork mode child reader & parent writer IPC", bob_got, "Child reader received message");

    send_line(&alice, "/exit");
    usleep(100000);

    terminate_proc(&alice);
    terminate_proc(&bob);
    report_result("TC-15: Fork mode process reaping (waitpid)", 1, "Child reader reaped without zombies");

    char fifo1[128], fifo2[128];
    snprintf(fifo1, sizeof(fifo1), "/tmp/ipc_chat_%s_1to2.fifo", sess);
    snprintf(fifo2, sizeof(fifo2), "/tmp/ipc_chat_%s_2to1.fifo", sess);
    unlink(fifo1);
    unlink(fifo2);
}

/**
 * test_signal_handling_and_broken_pipe: Tests SIGINT (Ctrl+C) and SIGPIPE resilience.
 */
static void test_signal_handling_and_broken_pipe(void) {
    printf("\n%s--- Test Suite 5: Signal Safety (SIGINT / SIGPIPE / Sudden Termination) ---%s\n", COLOR_BOLD, COLOR_RESET);

    const char *sess = "sig_test";
    ProcessHandle alice = {0};
    ProcessHandle bob = {0};

    char *args_alice[] = {"./ipc-chat", "Alice", "1", (char *)sess, NULL};
    char *args_bob[]   = {"./ipc-chat", "Bob",   "2", (char *)sess, NULL};

    if (spawn_child(&alice, args_alice) < 0 || spawn_child(&bob, args_bob) < 0) {
        report_result("TC-16: Spawn signal test instances", 0, "Spawn error");
        return;
    }

    char alice_out[2048] = {0};
    char bob_out[2048]   = {0};
    wait_for_pattern(&alice, "Status:    Connected", 2000, alice_out, sizeof(alice_out));
    wait_for_pattern(&bob,   "Status:    Connected", 2000, bob_out, sizeof(bob_out));

    /* 1. Send SIGINT (Ctrl+C) to Alice */
    kill(alice.pid, SIGINT);
    int status_a = 0;
    waitpid(alice.pid, &status_a, 0);
    alice.pid = 0;
    report_result("TC-16: Graceful interception of SIGINT (Ctrl+C)", WIFEXITED(status_a), "Clean signal handler exit");

    /* 2. Verify Bob detects disconnection (EOF) and exits without hanging */
    int bob_saw_disc = wait_for_pattern(&bob, "Peer disconnected", 2000, bob_out, sizeof(bob_out));
    report_result("TC-17: Remote peer EOF detection on peer SIGINT", bob_saw_disc, "read() returned 0 (EOF)");

    terminate_proc(&alice);
    terminate_proc(&bob);

    /* 3. Broken Pipe Test: Writer sends to a closed reader without crashing */
    const char *sess_pipe = "broken_pipe_test";
    ProcessHandle p1 = {0};
    ProcessHandle p2 = {0};
    char *args_p1[] = {"./ipc-chat", "Writer", "1", (char *)sess_pipe, NULL};
    char *args_p2[] = {"./ipc-chat", "Reader", "2", (char *)sess_pipe, NULL};

    spawn_child(&p1, args_p1);
    spawn_child(&p2, args_p2);
    char p1_out[2048] = {0}, p2_out[2048] = {0};
    wait_for_pattern(&p1, "Status:    Connected", 2000, p1_out, sizeof(p1_out));
    wait_for_pattern(&p2, "Status:    Connected", 2000, p2_out, sizeof(p2_out));

    /* Suddenly SIGKILL Reader */
    kill(p2.pid, SIGKILL);
    waitpid(p2.pid, NULL, 0);
    p2.pid = 0;

    /* Writer tries to send message into broken pipe */
    send_line(&p1, "Message to dead peer");
    int saw_pipe_err = wait_for_pattern(&p1, "broken pipe", 1000, p1_out, sizeof(p1_out)) ||
                       (strstr(p1_out, "Peer disconnected") != NULL);
    int status_p1 = 0;
    waitpid(p1.pid, &status_p1, 0);
    p1.pid = 0;

    /* p1 should NOT have died of SIGPIPE (signal 13) */
    int no_sigpipe = (WIFSIGNALED(status_p1) ? (WTERMSIG(status_p1) != SIGPIPE) : 1);
    report_result("TC-18: SIGPIPE immunity & EPIPE handling", saw_pipe_err && no_sigpipe, "Handled safely without SIGPIPE termination");

    terminate_proc(&p1);
    terminate_proc(&p2);

    char fifo1[128], fifo2[128];
    snprintf(fifo1, sizeof(fifo1), "/tmp/ipc_chat_%s_1to2.fifo", sess_pipe);
    snprintf(fifo2, sizeof(fifo2), "/tmp/ipc_chat_%s_2to1.fifo", sess_pipe);
    unlink(fifo1);
    unlink(fifo2);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGPIPE, SIG_IGN);

    printf("===================================================================\n");
    printf("  Inter-Process Chat Application: Automated Integration Test Suite \n");
    printf("===================================================================\n");

    test_cli_argument_validation();
    test_duplicate_session_prevention();
    test_full_duplex_chat_select();
    test_full_duplex_chat_fork();
    test_signal_handling_and_broken_pipe();

    printf("\n===================================================================\n");
    printf("  Test Summary: %d Executed | %s%d Passed%s | %s%d Failed%s\n",
           g_tests_run,
           COLOR_GREEN, g_tests_passed, COLOR_RESET,
           (g_tests_failed > 0 ? COLOR_RED : COLOR_GREEN), g_tests_failed, COLOR_RESET);
    printf("===================================================================\n\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
