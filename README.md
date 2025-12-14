# Deadlock Detection Library (Bonus Assignment)

## Overview

This project implements a **dynamic deadlock detection runtime** for multi-threaded C programs using **`pthread` mutexes**.  
The library intercepts `pthread_mutex_lock` and `pthread_mutex_unlock` calls via **`LD_PRELOAD`**, tracks lock ownership, builds a **wait-for graph**, and detects cycles (deadlocks) at runtime.

> ⚠️ **Note:** This version only detects deadlocks; it does **not recover or resolve them** yet.

---

## Features

- Intercepts **public** and **internal** pthread mutex functions:
  - `pthread_mutex_lock`
  - `pthread_mutex_unlock`
  - `__pthread_mutex_lock`
  - `__pthread_mutex_unlock`

- Tracks which threads **own** which mutexes and which threads are **waiting**.

- Builds a **wait-for graph** periodically (every 100 ms) and detects cycles.

- Prints:
  - Deadlock cycles (`ThreadID -> ThreadID -> ...`)
  - Tracker state showing mutex ownership and waiting relationships.

- Works on any multi-threaded program using pthread mutexes.

---

## Directory Structure

