# ⚡ Interactive Linux Process Monitor (C++17 | `/proc` | Zero-Dependency TUI)

A high-performance, real-time, interactive Terminal User Interface (TUI) system monitor built in modern C++17. Directly parses the Linux `/proc` filesystem and features a flicker-free double-buffered rendering engine with full keyboard navigation, dynamic sorting, live substring filtering, and process signal management (`SIGTERM`, `SIGKILL`, etc.).

---

## ✨ Features

- **🚀 Flicker-Free TUI Engine**: Custom double-buffered ANSI rendering utilizing the terminal alternate screen buffer (`\033[?1049h`). Clean exit restoration with zero artifacts.
- **📊 Real-Time Multi-Core CPU Gauges**: Visual progress meters per CPU core with dynamic color thresholds (Green $\to$ Yellow $\to$ Red).
- **💾 Memory & Swap Breakdown**: Live tracking of used, available, buffers, cached memory, and swap utilization.
- **⚡ System Dashboard**: Real-time task counts (*Total*, *Running*, *Sleeping*, *Zombie*), load averages (*1m*, *5m*, *15m*), and formatted uptime.
- **🔍 Live Substring Search & Filter**: Press `/` or `F3` to filter processes on-the-fly by command name, arguments, username, or PID.
- **🔄 Dynamic Sorting Modes**: Sort by `CPU%`, `MEM%`, `PID`, `USER`, `TIME+`, `NAME`, `VIRT`, or `RES`. Toggle ascending/descending with `r`.
- **🎯 Process Signal Management**: Send signals (`SIGTERM`, `SIGKILL`, `SIGHUP`, `SIGSTOP`, `SIGCONT`) directly to the selected process via `F9` / `k`.
- **🧵 Multi-Threaded Architecture**: Dedicated background collector thread coupled with a responsive, low-latency UI loop.
- **📦 Zero External Dependencies**: Written in pure C++17 and native POSIX system APIs (`termios`, `sys/ioctl.h`).

---

## ⌨️ Keyboard Shortcuts

| Key | Action |
|---|---|
| `↑` / `↓` or `k` / `j` | Navigate process rows |
| `PgUp` / `PgDn` | Scroll by 15 processes |
| `Home` / `End` or `g` / `G` | Jump to top / bottom of process list |
| `/` or `F3` | Open live search / filter bar (`Enter`/`Esc` to close) |
| `c` | Sort by **CPU%** |
| `m` | Sort by **Memory%** |
| `p` | Sort by **PID** |
| `t` | Sort by **Total CPU Time** (`TIME+`) |
| `u` | Sort by **Username** |
| `n` | Sort by **Command Name** |
| `r` or `I` | Toggle sort order (**Ascending** / **Descending**) |
| `F6` | Open interactive **Sort Selection** popup |
| `F9` or `k` | Open **Send Signal** popup (`SIGTERM`, `SIGKILL`, etc.) |
| `Space` | **Pause / Resume** live monitoring |
| `F1` or `?` | Open **Help** modal |
| `q` or `F10` | Quit application |

---

## 🛠️ Build and Run

### Prerequisites
- Linux or WSL2 (Windows Subsystem for Linux)
- `g++` (C++17 or later)

### Compile
```bash
./build.sh
```
*Or manually:*
```bash
g++ -std=c++17 -Wall -Wextra -O2 -pthread terminal.cpp render_buffer.cpp cpu.cpp memory.cpp process.cpp app.cpp main.cpp -o monitor
```

### Run
```bash
./monitor
```

---

## 🏗️ Architecture

```
┌────────────────────────────────────────────────────────┐
│                   Main Event Loop (UI)                 │
│  - POSIX Raw Terminal Input & Escape Sequence Parsing   │
│  - Double-Buffered Screen Cell Assembly & ANSI Styling │
│  - Interactive State (Selection, Scroll, Filter, Sort) │
└───────────────────────────▲────────────────────────────┘
                            │ Thread-Safe Mutex Lock
┌───────────────────────────┴────────────────────────────┐
│              Background Metric Collector Thread        │
│  - /proc/stat       (Total & Per-Core CPU Times)       │
│  - /proc/meminfo    (Total, Avail, Buffers, Cached)    │
│  - /proc/loadavg    (1m, 5m, 15m Load Averages)        │
│  - /proc/uptime     (System Uptime)                    │
│  - /proc/[pid]/stat (State, Priority, Nice, UTime, RSS)│
│  - /proc/[pid]/statm(VIRT, RES, SHR Memory Pages)      │
│  - /proc/[pid]/status & /etc/passwd (UID -> Username)  │
│  - /proc/[pid]/cmdline (Full Command Arguments)        │
└────────────────────────────────────────────────────────┘
```