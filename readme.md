# DSA Queue Simulator

A practical implementation demonstrating the application of Queue data structure to solve real-world traffic management problems.

##  Overview

This project showcases how the Queue data structure (FIFO - First In First Out) can be effectively applied to simulate and manage traffic flow scenarios, such as vehicle queuing at intersections.


## Web-based simulator (C + browser)

I converted the simulator frontend to a browser-based UI and added a small C HTTP server that serves the web UI and streams vehicle updates using Server-Sent Events (SSE). This approach keeps the project in C (no Python) and provides a cross-platform UI that runs in any modern browser.

Files added:
- `server.c` — a minimal HTTP server (C, pthreads) that serves `web/` and exposes `/events` (SSE) which tails `vehicles.data` and streams new lines to connected browsers.
- `web/index.html`, `web/app.js` — browser UI that renders a simple intersection on a canvas and listens to `/events` for vehicle updates.

How it works:
- The traffic generator programs (e.g., `traffic_generator.c`) append lines to `vehicles.data` in the format `VEHICLENO:LANE` (like `AB12C345:A`).
- `server.c` serves the web UI and streams new lines from `vehicles.data` to connected browsers via SSE.
- The browser receives events and renders vehicles into simple visual queues on a canvas.

Build & run (Linux / WSL / MSYS2):
1. Compile server:
```bash
gcc server.c -o server -pthread
```
2. Ensure `vehicles.data` exists (the project contains an empty file):
```bash
touch vehicles.data
```
3. Start the server:
```bash
./server
```
4. Open a browser to `http://localhost:8000/`.
5. In another terminal, run the traffic generator to populate `vehicles.data`:
```bash
gcc traffic_generator.c -o traffic_gen
./traffic_gen
```

Notes:
- The SSE endpoint tails `vehicles.data` per-connection and sends new lines as `data:` events.
- This is intentionally minimal (no external C libraries). It uses pthreads and POSIX sockets; run it on Linux/WSL/macOS. On native Windows you will need a POSIX-like environment (MSYS2) or adapt to Winsock.

If you'd like, I can:
- Add graceful shutdown handling to `server.c`.
- Make the server configurable (port, web directory, file path) via command-line arguments.
- Add a small web UI improvement or animation for vehicles.
