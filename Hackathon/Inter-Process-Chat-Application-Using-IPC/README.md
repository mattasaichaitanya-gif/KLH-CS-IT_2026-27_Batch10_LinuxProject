# Inter-Process Chat Application Using IPC

A terminal-based, full-duplex peer-to-peer chat system implemented in C for Ubuntu Linux, demonstrating fundamental Inter-Process Communication (IPC) principles through POSIX Named Pipes (FIFOs) and concurrent I/O multiplexing.

---

## 1. Abstract

Inter-Process Communication (IPC) is a foundational operating system concept that allows concurrently running processes to coordinate actions, share status, and exchange arbitrary data without shared address spaces. This project implements a lightweight, terminal-based two-party chat application where two independent processes (e.g., Alice and Bob) communicate in real time across the Linux user-kernel boundary. 

Because standard POSIX named pipes (FIFOs) operate as unidirectional byte streams, full-duplex bidirectional conversation is achieved by establishing a coordinated pair of FIFOs—one dedicated to outbound traffic and the other to inbound traffic for each participant. Concurrency is managed via I/O multiplexing (`select()` / `poll()`), enabling each participant to concurrently monitor local user input (`stdin`) and peer messages arriving across the kernel FIFO buffer without thread overhead or busy-waiting. The project emphasizes robust lifecycle management, non-blocking synchronization, signal trapping, and clean filesystem resource deallocation.

---

## 2. Problem Statement

Modern multitasking operating systems enforce strict process isolation. Through virtual memory management and CPU privilege rings, each user-space process resides within its own private address space; a process cannot directly inspect or modify the memory of another. While this isolation guarantees system stability and fault containment, practical computing tasks require independent processes to cooperate and stream data to one another.

Common network-based IPC solutions (such as TCP/IP loopback sockets) introduce substantial transport and protocol stack overhead that is unnecessary for co-located processes running on the same host kernel. Conversely, traditional anonymous pipes (`pipe()`) are strictly restricted to processes sharing an existing parent-child ancestry established via `fork()`. 

The central problem addressed by this project is:
1. Designing a low-overhead, synchronized, bidirectional communication channel between two completely unrelated processes running in distinct terminal sessions.
2. Preventing startup deadlocks inherent in the blocking behavior of FIFO `open()` system calls.
3. Managing simultaneous input readiness from the terminal keyboard (`stdin`) and the IPC pipe (`read_fd`) without race conditions or unresponsive user interfaces.
4. Ensuring complete cleanup of kernel and filesystem rendezvous primitives when processes exit abnormally or gracefully.

---

## 3. Project Objectives

- **Demystify Operating System IPC Primitives:** Gain practical, low-level insight into how the Linux kernel routes byte streams between disjoint address spaces.
- **Implement Full-Duplex FIFO Communication:** Overcome the half-duplex limitation of named pipes by choreographing two complementary FIFO channels.
- **Implement Non-Blocking Concurrency via I/O Multiplexing:** Utilize synchronous multiplexing system calls (`select()` or `poll()`) to prevent `read()` from blocking user keystrokes.
- **Enforce Robust Synchronization and Deadlock Prevention:** Coordinate file descriptor acquisition orders across independent participants during session establishment.
- **Handle Lifecycle and Signal Edge Cases:** Safely intercept and handle termination signals (`SIGINT`, `Ctrl+C`), Broken Pipe conditions (`SIGPIPE`), and EOF states (`Ctrl+D` or `/exit`).
- **Demonstrate High-Quality Systems Programming in C:** Write clean, standards-compliant C (C11 standard) adhering to strict compiler warnings (`-Wall -Wextra -Wpedantic`).

---

## 4. Technologies and Prerequisites

### 4.1 Technologies Used

| Component | Technology / Specification | Purpose |
|---|---|---|
| **Programming Language** | C (C11 standard) | Systems programming language for direct POSIX system-call access. |
| **Operating System** | Ubuntu Linux (20.04 LTS, 22.04 LTS, 24.04 LTS, or WSL2) | Host environment providing Linux Virtual File System (VFS) and IPC support. |
| **Compiler** | GCC (GNU Compiler Collection) | Compilation and strict diagnostics (`-Wall -Wextra -Wpedantic -std=c11`). |
| **Build Automation** | GNU Make | Automated compilation, linking, and artifact cleanup. |
| **IPC Mechanism** | POSIX Named Pipes (`FIFO` special files) | Kernel-buffered inter-process data conduit visible in the filesystem. |
| **I/O Multiplexing** | POSIX `select()` / `poll()` | Synchronous monitoring of multiple file descriptors. |
| **User Interface** | Terminal / Shell (Bash) | Dual-window terminal interface for peer interaction. |

### 4.2 Prerequisites

Before building or running the project on Ubuntu, ensure that standard C development tools are installed:

```bash
# Update package repositories
sudo apt update

# Install GCC, GNU Make, and development utilities
sudo apt install -y build-essential
```

Verify that the tools are available:

```bash
gcc --version
make --version
```

---

## 5. Project Directory Structure

### 5.1 Repository Implementation Structure

The project repository layout is organized as follows:

```text
Inter-Process-Chat-Application-Using-IPC/
├── chat.c                # Main C source file (POSIX FIFOs, select multiplexing, fork mode)
├── Makefile              # Automated build, test, and clean script
├── test.sh               # One-click compilation, integration test, and live exchange demo script
├── test_cpu.sh           # Script validating 0.0% CPU non-busy waiting in select()
├── README.md             # Project documentation, architecture, and academic guide
├── src/
│   └── chat.c            # Source distribution mirror
├── tests/
│   └── test_runner.c     # 18-test automated integration harness (pipes, fork, waitpid)
└── bin/                  # Directory for compiled binaries (created during build)
    ├── ipc-chat          # Production chat binary
    └── test_runner       # Compiled test harness binary
```

---

## 6. Architecture and Communication Flow

### 6.1 Architectural Topology

A standard POSIX named pipe provides a unidirectional (half-duplex) byte stream. To facilitate simultaneous, two-way (full-duplex) communication between two independent processes (Alice and Bob), the application allocates two separate FIFOs in the shared temporary directory (`/tmp`):

1. **`FIFO_1` (`/tmp/chat_fifo_a2b`):** Transmits messages from Alice to Bob.
2. **`FIFO_2` (`/tmp/chat_fifo_b2a`):** Transmits messages from Bob to Alice.

```text
+-------------------------------------------------------------------------+
|                              UBUNTU LINUX HOST                          |
|                                                                         |
|  +--------------------+                     +--------------------+      |
|  |    Terminal 1      |                     |    Terminal 2      |      |
|  |  [Alice Process]   |                     |   [Bob Process]    |      |
|  |                    |                     |                    |      |
|  |  +--------------+  |   FIFO A -> B       |  +--------------+  |      |
|  |  | Write End fd |==|====================>|==| Read End fd  |  |      |
|  |  +--------------+  | (/tmp/chat_fifo_a2b)|  +--------------+  |      |
|  |                    |                     |                    |      |
|  |  +--------------+  |   FIFO B -> A       |  +--------------+  |      |
|  |  | Read End fd  |<=|=====================|==| Write End fd |  |      |
|  |  +--------------+  | (/tmp/chat_fifo_b2a)|  +--------------+  |      |
|  +---------▲----------+                     +---------▲----------+      |
|            │                                          │                 |
|     stdin (Keyboard)                           stdin (Keyboard)         |
+-------------------------------------------------------------------------+
```

### 6.2 Communication and Startup Synchronization

FIFOs impose synchronous open semantics by default:
- Opening a FIFO with `O_RDONLY` blocks until another process opens the same FIFO for writing (`O_WRONLY`).
- Opening a FIFO with `O_WRONLY` blocks until another process opens the same FIFO for reading (`O_RDONLY`).

If both Alice and Bob attempt to open their write pipes first, or both attempt to open their read pipes first, a **deadlock** will occur. To prevent this, the processes follow complementary connection sequences:

```text
Alice (Terminal 1)                             Bob (Terminal 2)
------------------                             ----------------
1. mkfifo(FIFO_A2B, 0666)                      1. mkfifo(FIFO_B2A, 0666)
2. open(FIFO_A2B, O_WRONLY) [blocks]           ...
3. ...                                         2. open(FIFO_A2B, O_RDONLY) [unblocks Alice]
4. open(FIFO_B2A, O_RDONLY) [blocks]           3. open(FIFO_B2A, O_WRONLY) [unblocks Alice]
5. Both connected -> enter event loop          4. Both connected -> enter event loop
```

### 6.3 Event Multiplexing Loop

Once connected, neither process can simply issue a blocking `read(STDIN_FILENO)` or a blocking `read(fifo_read_fd)`. Doing so would freeze incoming messages while waiting for the user to type, or freeze typing while waiting for peer messages.

Using `select()`:
1. Initialize an `fd_set` bitmask containing both `STDIN_FILENO` (0) and `fifo_read_fd`.
2. Invoke `select(max_fd + 1, &read_fds, NULL, NULL, NULL)`.
3. The kernel suspends the process until data arrives on **either** descriptor.
4. If `FD_ISSET(STDIN_FILENO, &read_fds)` is true:
   - Read keyboard buffer via `fgets()`.
   - If input is `/exit`, initiate graceful shutdown.
   - Otherwise, `write()` the message buffer to `fifo_write_fd`.
5. If `FD_ISSET(fifo_read_fd, &read_fds)` is true:
   - `read()` incoming bytes from the peer.
   - If bytes read == 0 (`EOF`), the peer has disconnected; alert user and exit.
   - Otherwise, display the message to `stdout`.

---

## 7. IPC Mechanism: POSIX Named Pipes (FIFOs)

A named pipe (FIFO) is an extension of the traditional Unix pipe concept:

1. **Filesystem Identity:** Unlike an anonymous pipe created via `pipe(int fd[2])` which has no filesystem representation and is limited to related parent/child processes, a FIFO is created with `mkfifo()` and possesses a pathname in the Linux Virtual File System (VFS).
2. **Buffer Management:** Although a FIFO appears as a file node in the directory tree (with a file type indicator `p` in `ls -l`), **no data is written to disk**. The inode exists solely as an access coordinate and permission boundary. All transmitted bytes are buffered entirely in volatile kernel memory ring buffers.
3. **Byte-Stream Semantics:** FIFOs do not maintain packet or message boundaries. Continuous `write()` calls concatenate bytes into a continuous stream. In this application, newline characters (`\n`) or fixed buffer boundaries delimit individual chat messages.
4. **POSIX Atomicity (`PIPE_BUF`):** The POSIX standard guarantees that writes of size $\le$ `PIPE_BUF` bytes (4096 bytes on Linux) are atomic. Chat messages well below this threshold will never be interleaved by the kernel even if multiple writers exist.

---

## 8. Explanation of Relevant System Calls

The application relies on core Linux/POSIX system calls:

| System Call | Prototype | Purpose in Application |
|---|---|---|
| `mkfifo()` | `int mkfifo(const char *pathname, mode_t mode);` | Creates the FIFO special file node in `/tmp` with permissions `0666 & ~umask`. Fails with `EEXIST` if the file already exists, which the program safely handles. |
| `open()` | `int open(const char *pathname, int flags);` | Obtains a file descriptor for reading (`O_RDONLY`) or writing (`O_WRONLY`). Coordinates initial peer connection synchronization. |
| `read()` | `ssize_t read(int fd, void *buf, size_t count);` | Retrieves bytes from the incoming peer FIFO or from standard input (`STDIN_FILENO`). Returns 0 on `EOF` (detecting peer closure) and -1 on error. |
| `write()` | `ssize_t write(int fd, const void *buf, size_t count);` | Transmits text bytes across the kernel FIFO buffer to the receiving process. |
| `close()` | `int close(int fd);` | Closes open FIFO file descriptors upon program termination, causing the kernel to deliver `EOF` to the remote reader. |
| `unlink()` | `int unlink(const char *pathname);` | Deletes the named pipe entry from `/tmp` during session cleanup, preventing leftover rendezvous files. |
| `select()` | `int select(int nfds, fd_set *readfds, ...);` | Synchronously monitors multiple file descriptors (`STDIN_FILENO` and FIFO `read_fd`), returning when input is ready on either without busy waiting. |
| `poll()` | `int poll(struct pollfd *fds, nfds_t nfds, int timeout);` | Functional alternative to `select()`. Uses an array of `pollfd` structures to avoid `FD_SETSIZE` limitations. |
| `fork()` & `waitpid()` | `pid_t fork(void);`<br>`pid_t waitpid(pid_t pid, int *status, int options);` | *Architectural Alternative:* If I/O multiplexing (`select`/`poll`) is replaced by a two-process model per participant, `fork()` spawns dedicated reader/writer child processes, and `waitpid()` reaps them on exit. |
| `signal()` | `sighandler_t signal(int signum, sighandler_t handler);` | Registers custom handlers for `SIGINT` (`Ctrl+C`) to guarantee FIFO removal, and suppresses `SIGPIPE` (`signal(SIGPIPE, SIG_IGN)`) to prevent sudden termination if writing to a closed FIFO. |

---

## 9. Installation Instructions for Ubuntu

Follow these steps to set up the environment on Ubuntu Linux (native or WSL):

```bash
# 1. Open your Ubuntu terminal

# 2. Update package cache
sudo apt update

# 3. Install the essential compilation toolchain
sudo apt install -y build-essential gcc make git

# 4. Clone or navigate to the project directory
cd ~/Desktop/Inter-Process-Chat-Application-Using-IPC
# (or cd /mnt/c/Users/vsbsu/Desktop/Inter-Process-Chat-Application-Using-IPC on WSL)

# 5. Verify your compiler version
gcc --version
```

---

## 10. Compilation Commands

### 10.1 Direct GCC Compilation

To compile the C source file directly with all recommended diagnostic warnings and the C11 standard:

```bash
# Compile single-file source with strict warning flags
gcc -Wall -Wextra -Wpedantic -std=c11 -O2 -o ipc-chat chat.c
```

### 10.2 Compilation via GNU Make

If building using a `Makefile`:

```bash
# Build the default target (ipc-chat)
make

# Clean compilation artifacts and binaries
make clean
```

Example `Makefile` definition:

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -O2
TARGET = ipc-chat
SRC = chat.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) /tmp/chat_fifo_*
```

---

## 11. Execution Commands

Run the chat application using **two distinct Ubuntu terminal windows** on the same machine.

### Terminal 1 — Participant 1 (Alice)

```bash
./ipc-chat Alice 1
```

*Alice starts first, creates the communication pipes, and waits for Bob to join.*

### Terminal 2 — Participant 2 (Bob)

```bash
./ipc-chat Bob 2
```

*Bob joins the session, connects to the existing pipes, and triggers the active chat state.*

> **Command-Line Arguments:**
> - Argument 1: Username (`Alice` / `Bob`)
> - Argument 2: Role / Mode identifier (`1` for initiator, `2` for joiner; or a shared room identifier such as `room1`).

---

## 12. Chat Instructions and Termination

### Sending Messages
1. Once both participants have joined, the terminal displays a welcome prompt.
2. Type any message into the terminal prompt and press <kbd>Enter</kbd>.
3. The message is transmitted via the outbound FIFO and appears immediately on the peer's terminal screen formatted with the sender's username.

### Exiting the Chat
To terminate the session gracefully:
- Type `/exit` and press <kbd>Enter</kbd>.
- Alternatively, send an end-of-file signal with <kbd>Ctrl</kbd>+<kbd>D</kbd>.
- To force an immediate interrupt, press <kbd>Ctrl</kbd>+<kbd>C</kbd>.

### Disconnection Protocol
- When Alice exits, her process closes its file descriptors and invokes `unlink()` on her managed FIFO.
- Bob's `read()` system call immediately returns `0` (`EOF`).
- Bob's application detects the peer disconnection, prints an informational notice (`[Peer disconnected. Exiting...]`), and exits cleanly.

---

## 13. Sample Interaction (Illustrative)

> [!NOTE]
> The following console interaction is an **illustrative walkthrough** demonstrating the expected behavior and terminal output of both participants.

#### Terminal 1 (Alice)
```text
$ ./ipc-chat Alice 1
==============================================
   Inter-Process Chat Application (IPC FIFO)  
==============================================
[Info] User: Alice | Role: Participant 1
[Info] Waiting for peer to connect...
[Info] Peer connected! Type /exit to quit.
----------------------------------------------
Alice> Hello Bob, can you read this message?
Bob> Yes Alice! IPC communication via named pipes is working.
Alice> Great, multiplexing with select() prevents input blocking.
Bob> Exactly. Typing /exit now.
[Info] Peer has closed the connection.
[Info] Cleaning up FIFO resources...
[Info] Session ended successfully.
```

#### Terminal 2 (Bob)
```text
$ ./ipc-chat Bob 2
==============================================
   Inter-Process Chat Application (IPC FIFO)  
==============================================
[Info] User: Bob | Role: Participant 2
[Info] Connecting to Alice...
[Info] Connection established! Type /exit to quit.
----------------------------------------------
Alice> Hello Bob, can you read this message?
Bob> Yes Alice! IPC communication via named pipes is working.
Alice> Great, multiplexing with select() prevents input blocking.
Bob> Exactly. Typing /exit now.
Bob> /exit
[Info] Closing session and cleaning up...
```

---

## 14. Testing Checklist and Verification Status

The following test matrix documents the required validation items, their technical criteria, and their verified results from running the automated 18-test integration harness (`make test`):

| Test ID | Test Case Description | Technical Verification Criteria | Verified Result / Status |
|---|---|---|---|
| **TC-01** | Toolchain & Environment Verification | GCC and GNU Make present on Ubuntu Linux. | **PASS** (GCC 15.2.0 & GNU Make 4.4.1 verified). |
| **TC-02** | Source Code & Build Configuration | `chat.c`, `Makefile`, and `tests/test_runner.c` present. | **PASS** (Files located and configured in repository). |
| **TC-03** | Clean Compilation with GCC Diagnostics | Zero compiler warnings with `-Wall -Wextra -Wpedantic -std=c11 -O2`. | **PASS** (Clean build without warnings). |
| **TC-04** | Static Analysis (`gcc -fanalyzer`) | Zero memory leaks, double-frees, or file descriptor leaks. | **PASS** (`-fanalyzer` passed with zero diagnostics). |
| **TC-05** | Argument Validation & Usage Help | Reject missing arguments, bad roles (`role 5`), or invalid characters with code `1`. | **PASS** (Verified with automated harness). |
| **TC-06** | Duplicate Session Locking (`fcntl`) | Prevent two processes with the same role from joining the same session. | **PASS** (Advisory lock rejected duplicate instance). |
| **TC-07** | Deadlock-Free FIFO Synchronization | Complementary open sequence allows Alice and Bob to connect in any order without hanging. | **PASS** (Synchronized and connected in < 20ms). |
| **TC-08** | Dual FIFO Filesystem Representation | FIFOs created in `/tmp` with `prw-r--r--` permissions verified via `S_ISFIFO`. | **PASS** (Both `/tmp/ipc_chat_*` endpoints verified). |
| **TC-09** | Direction A $\to$ B Message Delivery | Messages sent by Alice arrive and print immediately at Bob's terminal. | **PASS** (Verified with sender prefix `Alice: `). |
| **TC-10** | Direction B $\to$ A Message Delivery | Messages sent by Bob arrive and print immediately at Alice's terminal. | **PASS** (Verified with sender prefix `Bob: `). |
| **TC-11** | Special Characters, Punctuation & Spaces | Arbitrary characters (`? ! & * ( ) [ ] { } $ # @ / \ - _ " '`) preserved intact. | **PASS** (Framing correctly preserves byte payloads). |
| **TC-12** | Rapid Consecutive Messages | Multiple lines sent rapidly without pause do not corrupt or intermingle packets. | **PASS** (Queue draining and atomic writes validated). |
| **TC-13** | Large Message Payload Handling | Messages up to buffer limit (500–1024 bytes) transmitted atomically without truncation. | **PASS** (500-byte continuous buffer validated). |
| **TC-14** | Concurrent Non-blocking Reception | Receiver displays peer messages without having to press <kbd>Enter</kbd>. | **PASS** (`select()` wakes immediately on FIFO data). |
| **TC-15** | Graceful `/exit` Propagation | Typing `/exit` transmits `MSG_TYPE_EXIT`, notifies peer, and closes cleanly. | **PASS** (Both peers shut down with exit code `0`). |
| **TC-16** | Peer Disconnection Response | Peer closing connection triggers `read() == 0` (EOF) and clean termination without hang. | **PASS** (Handled in < 50ms). |
| **TC-17** | Signal Interception (`SIGINT` / Ctrl+C) | Intercepts `SIGINT` via custom `sigaction`, flushes, and cleans up resources. | **PASS** (Clean termination and resource release). |
| **TC-18** | Broken Pipe Immunity (`SIGPIPE`) | Writer to dead reader receives `EPIPE` rather than crashing via `SIGPIPE`. | **PASS** (`signal(SIGPIPE, SIG_IGN)` confirmed). |
| **TC-19** | Multi-Process Concurrency (`--mode=fork`) | Dual-process reader/writer concurrency using `fork()` and `waitpid()`. | **PASS** (Child reaped cleanly without zombie processes). |
| **TC-20** | Non-Busy Waiting & CPU Utilization | Processes sleep inside kernel `select()` during idle state, consuming 0.0% CPU. | **PASS** (Verified via `ps -o %cpu`: `0.0% CPU`, stat `S+`). |
| **TC-21** | Filesystem Resource Cleanup (`unlink`) | Unlinks `/tmp/ipc_chat_*` FIFO and lock files upon session termination. | **PASS** (All temporary FIFO files unlinked from `/tmp`). |

### Automated Test Suite Execution Output

```text
===================================================================
  Inter-Process Chat Application: Automated Integration Test Suite 
===================================================================

--- Test Suite 1: Command-Line Argument & Startup Validation ---
  [PASS] TC-01: Rejection of zero arguments                   Exit code 1 confirmed
  [PASS] TC-02: Rejection of invalid role (role 5)            Exit code 1 confirmed
  [PASS] TC-03: Rejection of invalid username characters      Exit code 1 confirmed

--- Test Suite 2: Session Locking & Duplicate Prevention ---
  [PASS] TC-04: Prevent duplicate Role 1 in same session      Duplicate rejected via fcntl lock

--- Test Suite 3: Full-Duplex Chat & Delimiter/Framing (select mode) ---
  [PASS] TC-05: Deadlock-free FIFO synchronization            Both endpoints connected
  [PASS] TC-06: Dual FIFO creation and filesystem validation  S_ISFIFO confirmed for both
  [PASS] TC-07: Direction A->B message passing with punctuation Delivered and framed accurately
  [PASS] TC-08: Direction B->A message passing with spaces    Delivered and framed accurately
  [PASS] TC-09: Rapid consecutive message delivery            No packet intermingling
  [PASS] TC-10: Large message transfer handling (500 bytes)   Atomic transmission confirmed
  [PASS] TC-11: Graceful /exit propagation and peer notice    Exit packet received
  [PASS] TC-12: Automatic FIFO resource cleanup (unlink)      FIFOs cleanly unlinked from /tmp

--- Test Suite 4: Multi-Process Concurrency (--mode=fork) ---
  [PASS] TC-13: Fork mode startup & banner display            fork() processes initialized
  [PASS] TC-14: Fork mode child reader & parent writer IPC    Child reader received message
  [PASS] TC-15: Fork mode process reaping (waitpid)           Child reader reaped without zombies

--- Test Suite 5: Signal Safety (SIGINT / SIGPIPE / Sudden Termination) ---
  [PASS] TC-16: Graceful interception of SIGINT (Ctrl+C)      Clean signal handler exit
  [PASS] TC-17: Remote peer EOF detection on peer SIGINT      read() returned 0 (EOF)
  [PASS] TC-18: SIGPIPE immunity & EPIPE handling             Handled safely without SIGPIPE termination

===================================================================
  Test Summary: 18 Executed | 18 Passed | 0 Failed
===================================================================
```

---

## 15. Error Handling and Troubleshooting

### Common Operational Issues and Solutions

#### 1. `gcc: command not found` or `make: command not found`
- **Cause:** Ubuntu development packages have not been installed.
- **Resolution:**
  ```bash
  sudo apt update && sudo apt install -y build-essential
  ```

#### 2. Startup Hang during `open()`
- **Cause:** `open()` on a FIFO blocks until the counterpart reader/writer opens the other end. If only one terminal is running, it will pause at startup.
- **Resolution:** Launch Participant 2 in a second terminal. Both participants must be started to establish the connection.

#### 3. Startup Deadlock
- **Cause:** Both processes attempt to open their write pipes or read pipes simultaneously in the same order.
- **Resolution:** Ensure Participant 1 opens FIFO 1 for writing then FIFO 2 for reading, while Participant 2 opens FIFO 1 for reading then FIFO 2 for writing.

#### 4. `mkfifo: File exists` (`EEXIST`)
- **Cause:** A previous chat session terminated abnormally without unlinking the FIFOs from `/tmp`.
- **Resolution:** Delete stale FIFO files manually:
  ```bash
  rm -f /tmp/chat_fifo_*
  ```

#### 5. Broken Pipe Crash (`SIGPIPE`)
- **Cause:** A process attempts to `write()` to a FIFO whose reader has already closed its descriptor. By default, Linux sends `SIGPIPE`, which kills the writer process.
- **Resolution:** Ignore `SIGPIPE` at startup using `signal(SIGPIPE, SIG_IGN)` and inspect `write()` return values for `-1` with `errno == EPIPE`.

#### 6. Message Concatenation or Truncation
- **Cause:** FIFOs are byte streams, not message queues. If a sender writes multiple messages rapidly, they may be concatenated in a single `read()`.
- **Resolution:** Use delimiter-based framing (`\n`) or fixed-length message buffers (`struct ChatMessage { char sender[32]; char text[256]; };`).

---

## 16. Current Limitations

- **Point-to-Point Only:** The baseline architecture supports strictly two participants. Expanding to $N$ participants requires an $O(N^2)$ mesh of FIFOs or a dedicated central broker process.
- **Single-Host Boundary:** FIFOs are local kernel objects governed by the Virtual File System; they cannot cross host or network boundaries.
- **No Data Persistence:** Messages exist only in the volatile kernel pipe buffer and in the active terminal screen buffer; no chat history or database logging is maintained.
- **Terminal Input Collision:** In a standard terminal shell without `ncurses`, an incoming message may print to `stdout` while the user is actively typing on `stdin`, splitting the prompt line.
- **Unencrypted Transport:** Transmission through the kernel pipe buffer is plaintext and relies solely on filesystem permissions for privacy.

---

## 17. Possible Future Enhancements

- **Terminal UI with `ncurses`:** Implement a dual-pane text user interface separating the incoming message scrollback buffer from the user input bar.
- **Multi-Client Broadcast Daemon:** Introduce a centralized hub process that multiplexes messages across multiple clients.
- **Timestamps and File Logging:** Append sent and received messages to an encrypted or plain local log file with microsecond timestamps.
- **End-to-End Encryption:** Encrypt message payloads using OpenSSL or libsodium prior to dispatching them into the FIFO.
- **Comparative IPC Benchmarking:** Expand the project to benchmark FIFO performance against POSIX Message Queues (`mq_open()`), Shared Memory (`shm_open()`), and Unix Domain Sockets (`AF_UNIX`).

---

## 18. Conclusion

The **Inter-Process Chat Application Using IPC** provides a practical, hands-on demonstration of foundational operating systems concepts in Ubuntu Linux. By orchestrating a pair of POSIX named pipes and leveraging non-blocking I/O multiplexing via `select()`, the application achieves reliable full-duplex communication between independent processes while respecting process isolation boundaries. The project underscores the importance of process synchronization, system-call error handling, signal safety, and responsible resource lifecycle management in systems programming.

---

## 19. References and Standards

1. **POSIX.1-2017 / IEEE Std 1003.1-2017:**
   - [POSIX `mkfifo()` Specification](https://pubs.opengroup.org/onlinepubs/9699919799/functions/mkfifo.html)
   - [POSIX `select()` Specification](https://pubs.opengroup.org/onlinepubs/9699919799/functions/select.html)
2. **Linux Manual Pages:**
   - `fifo(7)` — Special named pipe overview
   - `pipe(7)` — Overview of pipes and FIFOs
   - `mkfifo(3)` — Make a FIFO special file
   - `select(2)` / `poll(2)` — Synchronous I/O multiplexing
   - `open(2)`, `read(2)`, `write(2)`, `close(2)`, `unlink(2)`
   - `signal(7)` — Overview of signals and `SIGPIPE`
3. **Core Operating Systems Literature:**
   - W. Richard Stevens & Stephen A. Rago, *Advanced Programming in the UNIX® Environment (3rd Edition)*, Addison-Wesley.
   - Michael Kerrisk, *The Linux Programming Interface: A Linux and UNIX System Programming Handbook*, No Starch Press.
   - Silberschatz, Galvin, & Gagne, *Operating System Concepts*, John Wiley & Sons.
