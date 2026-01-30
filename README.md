# LibDeadlock: Runtime POSIX Deadlock Detector

![Language](https://img.shields.io/badge/language-C%2FC%2B%2B-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux-green.svg)
![License](https://img.shields.io/badge/license-MIT-lightgrey.svg)

**LibDeadlock** is a zero-configuration, runtime analysis tool for C/C++ applications. It detects deadlocks in multi-threaded environments by intercepting `pthread` mutex calls via **`LD_PRELOAD`**, constructing a real-time **Resource Allocation Graph (RAG)**, and identifying cycles using Depth-First Search (DFS).

Unlike static analyzers, LibDeadlock runs alongside your application, providing **stack traces** and **mutex ownership chains** the moment a deadlock occurs.

---

## Key Features

* **Zero-Code Integration:** Uses dynamic linker interposition (`LD_PRELOAD`); no recompilation or source code modification required.
* **Deep Symbol Hooking:** Intercepts both public (`pthread_mutex_lock`) and internal glibc symbols (`__pthread_mutex_lock`) to catch optimized internal locking mechanisms.
* **Cycle Detection Engine:** Runs a dedicated monitor thread that periodically builds a **Wait-For Graph** to detect circular dependencies (Deadlocks).
* **Diagnostic Stack Traces:** Captures and resolves stack frames (`execinfo.h`) to pinpoint exactly *where* in the source code the deadlock occurred.
* **Async-Signal-Safe Logging:** Uses raw `write()` syscalls instead of buffered `printf` to ensure output integrity during frozen states.

---

## Usage

### 1. Build the Shared Library
```bash
make
# Generates libdeadlock.so
```

### 2. Run with Injection
Inject the library into any existing C/C++ executable:

```bash
LD_PRELOAD=./libdeadlock.so ./your_program
```

### 3. Example Output
When a deadlock is detected, the tool interrupts execution and prints the dependency cycle:

```text
[DEADLOCK DETECTED] Cycle Found:
Thread [14023] is waiting for Mutex [0x55a...00] (Owned by Thread [14024])
Thread [14024] is waiting for Mutex [0x55a...20] (Owned by Thread [14023])

[STACK TRACE] Thread 14023 blocked at:
  ./test_program(func_A+0x14) [0x55...]
  ./test_program(main+0x2a) [0x55...]
```

---

## Technical Architecture

### 1. Symbol Interposition (The "Hook")
The library utilizes the Linux dynamic linker's `LD_PRELOAD` feature to load before `libc`. Using `dlsym(RTLD_NEXT, ...)` inside a constructor (`__attribute__((constructor))`), we preserve the original function pointers while injecting our tracking logic.

**Intercepted Symbols:**
* `pthread_mutex_lock` / `unlock` / `trylock`
* `__pthread_mutex_lock` / `__pthread_mutex_unlock` (Internal glibc variants)

### 2. The Monitor Thread
A detached, non-hooked thread is spawned at startup. It sleeps for a definable interval (default: 100ms) and performs the following:
1.  **Snapshot:** Safely pauses hooks using a global spinlock.
2.  **Graph Build:** Constructs a directed graph where Nodes = Threads and Edges = "Waiting For Mutex".
3.  **Cycle Check:** Runs a generic DFS algorithm. If a back-edge is detected during traversal, a deadlock is confirmed.

### 3. Data Structures & Performance
* **O(N) Lookups:** Thread and Mutex tracking utilizes linear arrays rather than hash maps. For typical concurrency loads (<500 threads), this offers better cache locality and avoids `malloc` overhead during critical sections.
* **Spinlocks:** Uses `atomic_flag` for low-overhead busy waiting, preventing the detector itself from causing a deadlock or context switch.

---

## ⚠️ Platform & Limitations

* **OS:** Linux only (Requires ELF binary format and `/proc` filesystem access).
* **Architecture:** x86_64 recommended.
* **Stack Depth:** Captures the top 10 stack frames to balance debug context with runtime performance.
* **Thread Safety:** The detector uses a "reentrancy guard" to ensure that the monitor thread's own internal locking does not trigger the hooks.

---

## 📄 License
MIT License. Free for educational and research use.
