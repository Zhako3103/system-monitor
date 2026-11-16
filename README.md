# System Monitor (C++ / Dear ImGui)

1. `sudo apt install libsdl2-dev build-essential`
2. Put the provided `imgui/` folder (with `lib/`, `backend/`, `gl3w/`) next to these files.
3. `make && ./monitor`

Files: main.cpp (loop/layout), system.cpp (OS/CPU/Fan/Thermal + graphs),
mem.cpp (RAM/SWAP/Disk + process table), network.cpp (RX/TX tables + bars).
