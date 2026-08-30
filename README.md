# ⚡ Modular Linux System & Process Monitor (C++17 | `/proc` | Zero-Dependency)

A high-performance terminal system monitor built in modern C++17. Features independent modular monitoring for **CPU**, **Memory**, **Disks & Filesystems**, **Network Interfaces**, and **Processes** with TrueColor theme engine, process tree hierarchy, deep process inspector, multi-line shaded Braille graphs, dynamic responsive layouts, and full mouse interaction.

---

## 📁 Project Structure

```
process_mntr/
├── include/
│   ├── collectors/          # Kernel & hardware metric collector interfaces
│   │   ├── cpu.h            # CPU utilization, per-core stats, load averages
│   │   ├── memory.h         # RAM and swap memory breakdowns
│   │   ├── disk.h           # Mounted filesystems and disk I/O throughput
│   │   ├── net.h            # Network interface bandwidth and packet counters
│   │   ├── process.h        # Process tree snapshots and status
│   │   └── sensors.h        # Thermal and hardware sensor readings
│   ├── core/                # Core engine, application coordinator & state
│   │   └── app.h            # App lifecycle, event loop, dynamic layout engine
│   └── ui/                  # User interface, graphics & terminal abstraction
│       ├── terminal.h       # Cross-platform raw terminal, input & mouse parser
│       ├── theme.h          # TrueColor theme engine (Tokyo Night, Dracula, Nord, etc.)
│       ├── render_buffer.h  # Double-buffered screen cell engine & gradients
│       └── graph.h          # Braille matrix and sparkline chart renderers
├── src/
│   ├── collectors/          # Collector implementations (/proc parser)
│   │   ├── cpu.cpp
│   │   ├── memory.cpp
│   │   ├── disk.cpp
│   │   ├── net.cpp
│   │   ├── process.cpp
│   │   └── sensors.cpp
│   ├── core/                # Core engine implementations
│   │   └── app.cpp
│   ├── ui/                  # UI, graphics & terminal implementations
│   │   ├── terminal.cpp
│   │   ├── theme.cpp
│   │   ├── render_buffer.cpp
│   │   └── graph.cpp
│   └── main.cpp             # CLI argument parser and entry point
├── Makefile                 # Modular build system (Linux & Windows MinGW)
├── build.sh                 # Single-command Linux build script
└── README.md
```

---

## ✨ Features

- **🌲 Interactive Process Tree**: Toggle hierarchical process tree view (`t` or `F5`) with parent-child links (`├─`, `└─`, `│  `).
- **🔍 Deep Process Inspector**: Press `Enter` or `d` on any process to view full command arguments, memory footprints (VIRT, RES, SHR), CPU breakdown (user vs system ticks), threads, and direct signal action buttons.
- **🎨 6 TrueColor Themes**: Live switchable themes (`o` or `F8`) including **Tokyo Night**, **Dracula**, **Nord**, **Cyberpunk / Neon**, **Monokai Pro**, and **Matrix Green**.
- **🖱️ Full Mouse Interaction**: Mouse wheel scrolling on the process table, click to select/inspect rows, and click header badges `[1:cpu]` `[2:mem]` to toggle panels.
- **🎛️ Independent Module Selection**: Toggle panels on-the-fly (`1` for CPU, `2` for Memory, `3` for Disk, `4` for Network, `5` for Processes) or launch targeted views via CLI flags (`--disk`, `--net`, `--proc`).
- **📐 Dynamic Responsive Layout Engine**: Automatically calculates panel arrangements (2-column split, stacked, fullscreen focus) when panels are shown or hidden.
- **📊 Multi-Line Shaded Braille Graphs**: High-density Braille charts with shaded area-fill and vertical TrueColor gradients for CPU, RAM, Network RX/TX throughput, and Disk I/O activity.
- **💾 Filesystem & Disk I/O Inspection**: Real-time mounted partition storage usage (`/`, `/home`, etc.) alongside disk read/write bandwidth and IOPS.
- **🌐 Network RX/TX Monitor**: Live download/upload speed gauges, dual charts, session bandwidth counters, and interface cycling (`i`).
- **🔍 Live Substring Search & Filter**: Press `/` or `F3` to filter processes on-the-fly by command name, arguments, username, or PID.
- **🔄 Dynamic Sorting Modes**: Sort by `CPU%`, `MEM%`, `PID`, `USER`, `TIME+`, `NAME`, `VIRT`, or `RES`. Toggle ascending/descending with `r`.
- **🎯 Process Signal Management**: Send signals (`SIGTERM`, `SIGKILL`, `SIGHUP`, `SIGSTOP`, `SIGCONT`) directly to the selected process via `F9` / `k` or the Inspector modal.
- **📦 Zero External Dependencies**: Written in pure C++17 using standard POSIX system APIs.

---

## ⌨️ Keyboard Shortcuts

| Key | Action |
|---|---|
| `t` / `F5` | Toggle **Process Tree Hierarchy** |
| `Enter` / `d` | Open **Deep Process Inspector** modal |
| `o` / `F8` | Open **Color Theme Switcher** modal |
| `Mouse Wheel` | Scroll process table up / down |
| `Mouse Click` | Select row / toggle header modules / inspect process |
| `1` | Toggle **CPU** panel |
| `2` | Toggle **Memory / Swap** panel |
| `3` | Toggle **Disk & Filesystems** panel |
| `4` | Toggle **Network** panel |
| `5` | Toggle **Process Table** panel |
| `Tab` / `P` | Cycle **Layout Presets** (*Full*, *Resources*, *Processes*, *I/O Focus*, *Minimal*) |
| `m` / `F2` | Open **Module Configuration** menu |
| `i` / `I` | Cycle active **Network Interface** |
| `+` / `-` | Increase / decrease refresh frequency |
| `↑` / `↓` or `k` / `j` | Navigate process rows |
| `PgUp` / `PgDn` | Scroll by 15 processes |
| `Home` / `End` or `g` / `G` | Jump to top / bottom of process list |
| `/` or `F3` | Open live search / filter bar (`Enter`/`Esc` to close) |
| `c` / `e` / `p` | Sort by **CPU%**, **Memory%**, **PID** |
| `r` | Toggle sort order (**Ascending** / **Descending**) |
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
make
```

### Run
```bash
# Launch with full dashboard
./monitor

# Launch with specific theme
./monitor --theme tokyo
./monitor --theme dracula
./monitor --theme cyberpunk

# Launch specific independent modules
./monitor --disk --net
./monitor --proc
./monitor --preset resources

# Custom refresh interval (500ms)
./monitor -i 500
```