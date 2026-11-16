# system-monitor

A **real-time Linux system resource monitor** written in C++ with **SDL2** and **Dear ImGui**.

## Features

- **CPU:** overall usage and per-core load with a live graph.
- **Thermal sensors:** temperatures read from the kernel's thermal interface.
- **Memory:** RAM and SWAP usage with progress bars.
- **Processes:** sortable and filterable process table.
- **Network:** per-interface RX/TX counters and throughput graphs.
- Immediate-mode GUI that refreshes at a configurable rate.

## Source layout

| File | Responsibility |
| --- | --- |
| `main.cpp` | SDL2 window, OpenGL context, Dear ImGui setup, main loop and widget layout |
| `system.cpp` | CPU statistics, thermal sensors, process list and general system information (via `/proc` and `/sys`) |
| `mem.cpp` | RAM and SWAP usage, and the process memory table |
| `network.cpp` | Network interface discovery and RX/TX statistics |

All data comes from `/proc` and `/sys`, so no extra daemons or root privileges are required.

## Dependencies

- A C++17 compiler (`g++` or `clang++`)
- SDL2 development files
- OpenGL development files
- Dear ImGui (SDL2 + OpenGL backends)
- `make`

Debian/Ubuntu:

```bash
sudo apt update
sudo apt install build-essential libsdl2-dev libgl1-mesa-dev
```

## Build and run

```bash
make
./monitor
```

Clean build artifacts:

```bash
make clean
```
