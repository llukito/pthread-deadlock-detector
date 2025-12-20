# Deadlock Detection Library

## Overview

This project implements a **dynamic deadlock detection runtime** for multi-threaded C programs using **`pthread` mutexes**.  
The library intercepts `pthread_mutex_lock`, `pthread_mutex_unlock` and `pthread_mutex_trylock`calls via **`LD_PRELOAD`**, tracks lock ownership, builds a **wait-for graph**, and detects cycles (deadlocks) at runtime.

---

## Features

- Intercepts **public** and **internal** pthread mutex functions:
  - `pthread_mutex_lock`
  - `pthread_mutex_unlock`
  - `pthread_mutex_trylock`
  - `__pthread_mutex_lock`
  - `__pthread_mutex_unlock`
  - `__pthread_mutex_trylock`

- Tracks which threads **own** which mutexes and which threads are **waiting**.

- Builds a **wait-for graph** periodically (every 100 ms) and detects cycles.

- Prints:
  - Deadlock cycles (`ThreadID -> ThreadID -> ...`)
  - Tracker state showing mutex ownership and waiting relationships.

- Works on any multi-threaded program using pthread mutexes.

---

## Technical Overview

- To run the program, first use `make`,  
  and then run the command:

      LD_PRELOAD=./libdeadlock.so ./test

  > Make sure to edit the file that you want to test for deadlock in the Makefile.

LD_PRELOAD basically puts my library first before the program runs, so we set up the API first before these well-known libraries (such as `pthread.h`).

Once the library is linked, functions with `__attribute__((constructor))` at the head get executed first. That's where we track and remember real functions (`pthread_mutex_lock`, `pthread_mutex_unlock` and `pthread_mutex_trylock`).

We also initialize our thread, which is a special one and is not tracked, because we use it for parallel monitoring execution. That's why, in the future, we won't use `pthread_mutex_lock` and `pthread_mutex_unlock` but another one, which is `atomic_flag`. Notice that we also use attributes for that pthread: we use a detached type and not joinable, because there is no other thread that will be waiting for it, so it cleans up automatically.

Then we define functions that already exist in `pthread.h`, but because we linked our library first, the program sets this library in the first container and `pthread.h` in the following one. So once a function tries to search for the implementation of these two functions, it will come across ours first, which will run additional code before calling the actual functions. This additional code is what remembers information about threads and mutexes. Notice that we have both `pthread_mutex_lock` and `__pthread_mutex_lock`, and the same for unlock and trylock, because sometimes what is really called is the one with `__`. This is done by glibc for optimization, so sometimes the real one is called, sometimes the `__` version, so we have both ready in case either of them is called.

We also have `in_hook` to make sure another thread does not access our functions while another one is inside them. In that case, we just call the main lock and do not let it go further. But in general, we remember which thread is waiting for which mutex and which thread acquires which mutex, and oppositely in unlock, where we track which pthread unlocked a mutex.

Then we take this information to create a wait-for graph. But before that, we basically had two mappings:

1. Which mutex is locked by which thread  
2. Which thread is waiting for which mutex

We could, of course, use a generic map for this, but it would only make it more complex, so I decided to make two arrays, where access will be O(n), which is still practical for ~300 or more threads. If you want to make it scalable, it is advised to switch to a map.

Also, when we used pthread for our monitoring function, we used busy waiting in the `spinlock_acq` function, which waits until it takes the lock. This is still fine for our implementation, but again, if you want to make it scalable, switch to condition wait and then signal (unless you plan to add those functions into monitoring).

We also use `write` instead of `printf`, because it is unbuffered and makes log prints more accurate.

In the graph, we connect threads using this logic (simplified): if T1 is waiting for M1, and M1 is acquired by T2, then `T1 -> T2`. So it is directed. But if, in any case, T2 connects back to T1, we have a deadlock.

Our monitor function runs tests every 100 ms, builds the graph, checks for cycles with DFS, and then tracks the cycle so we can output it. Of course, the time period at which our monitor runs is customizable as well.

In the process, we print which thread has which mutex, these simple logs, and in the end, if a cycle is detected, we output which threads are involved. You can remove additional comments if you want to leave only the final one.

# Technical Requirements

## Platform Support
Linux Only: Uses LD_PRELOAD, ELF-specific headers (<execinfo.h>), and the /proc filesystem. Not compatible with macOS (Mach-O) or Windows (PE).

## Stack Tracing
Depth Limit: 
To search where exactly that deadlock happened and where are these
threads stored, we have to access stack frames stored in RAM. We
capture a maximum of 10 stack frames (this provides a balance between deep debugging context and low runtime memory overhead), which is quite descent, cause many programs don't require more, but again if you want to make it scalable, increase its size. 

Symbol Resolution: Uses the addr2line utility to map addresses to source code.