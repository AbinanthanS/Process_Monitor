Linux Process Monitor (C++ | Linux/WSL2 | /proc)

A real-time system monitor implemented in C++. It tracks CPU and memory usage by directly parsing the Linux `/proc` filesystem and provides a dynamic, top-like view of running processes with per-process CPU utilization.

---

## Features

- Real-time system CPU usage
- Memory usage tracking
- Process listing using `/proc/[pid]`
- Per-process CPU usage calculation (time-delta based)
- Dynamic sorting by CPU usage
- Safe handling of short-lived processes

---

## Tech Stack

- Language: C++
- OS Interface: Linux `/proc` filesystem

### Concepts Used

- Systems Programming
- File Parsing
- Process Management
- CPU Scheduling Metrics

---

## How It Works

### CPU Usage

- Reads `/proc/stat`
- Computes CPU usage using the difference between consecutive samples `prev` and `curr`

### Memory Usage

- Parses `/proc/meminfo`
- Calculates usage using `MemTotal` and `MemAvailable`

### Process Monitoring

- Iterates through `/proc/[pid]`
- Extracts:
  - Process ID (PID)
  - Process name (from `/cmdline`)
  - CPU time (`utime + stime` from `/stat`)

### Per-Process CPU Usage
CPU% = (process_time_delta / total_cpu_time_delta) * number_of_cores * 100


---

## Build and Run

### Prerequisites

- Linux or WSL2
- g++ (C++17 or later)

### Compile
g++ main.cpp cpu.cpp memory.cpp process.cpp -o monitor

### Run


./monitor