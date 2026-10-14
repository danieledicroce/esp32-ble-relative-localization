# Extension to Multiple Mobile Devices (Robots)

This extension is not part of the thesis, which stops at the three-board prototype described in Chapters 2 and 3. It documents how the same system, with the same ESP32-WROOM-32 boards, can be used to localize about 10 mobile devices (robots) relative to fixed points, with a more compact payload format. The code is in the three folders of this directory.

## 1. Idea

The thesis prototype exchanges distances as 4-byte `float` numbers, and a legacy advertising packet (31 bytes) holds at most 6 of them. To handle more devices, distances are transmitted as `uint16` integers in centimeters (2 bytes, value `0xFFFF` = distance not available). With this format, up to 13 distances fit in a packet.

Three firmwares are needed:

| Folder                           | Role                      | Description                                                                                                                                                                                                                                                                                  |
| -------------------------------- | ------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `esp32_anchor`                   | Fixed nodes (3 boards)    | They measure the RSSI of the robots, exchange the measured distances with each other and compute the position of every robot. The coordinates of the fixed nodes are measured by hand and entered in the code.                                                                               |
| `esp32_robot_broadcast`          | Robot (transmit only)     | It only transmits its own ID. It does not scan and computes nothing.                                                                                                                                                                                                                         |
| `esp32_robot_broadcast_observer` | Robot (transmit and scan) | It transmits its own ID, like the broadcast-only robot, and also listens to the anchors: from the distances they transmit, together with the known coordinates of the anchors, it works out where all the robots are. Meanwhile, the anchors keep localizing the robots and displaying them. |

The anchors (`esp32_anchor`) work with only one type of robot sketch at a time. Robots are interchangeable: the same board can be loaded with `esp32_robot_broadcast` or with `esp32_robot_broadcast_observer`, depending on the function it is meant to have. IDs are always those of the robots, starting from 16:

| ID            | Who                             |
| ------------- | ------------------------------- |
| 0, 1, 2       | anchors                         |
| 16, 17, 18, … | robots (with the chosen sketch) |

- With `esp32_robot_broadcast`, the robot just makes itself heard: the anchors localize it and display it.
- With `esp32_robot_broadcast_observer`, the same holds, and the robot also listens to the anchors: the reference points of the trilateration are always the anchors (fixed, with known coordinates), never the robots, which move. The robot thus knows the position of all the robots, its own included, while the anchors keep displaying them (same JSON).

## 2. Maximum Number of Devices

- **Legacy packet:** 31 bytes, of which 26 are usable for distances (the calculation is in Section 3.1). The prototype payload was 19 bytes.
- **Compact format:** with 2-byte distances, the payload of an anchor is `3 + 2·M` bytes for `M` robots, so the limit is **13 robots** per packet. The sketch checks this with a `static_assert`. With 1-byte distances in steps of 0.1 m one would reach 26, but resolution would be lost.
- **Extended Advertising (Bluetooth 5.0):** it allows packets of up to 255 bytes (1650 chained), but the ESP32-WROOM-32 only supports Bluetooth 4.2 and cannot transmit it. Other boards support it (ESP32-C3, ESP32-S3). It is not needed for this scenario.
- **Scanning:** it does not require Bluetooth 5.0 (the prototype already scans in legacy mode). The memory needed to hold the state of 10 robots is negligible (about 500 bytes out of about 520 KB of RAM).

## 3. Packet Format

### 3.1 How Many Bytes We Have

The legacy advertising packet is 31 bytes long. Of these, 2 are taken by the header of the Manufacturer Specific Data (MSD) field (length and type), added automatically by the library; 29 bytes remain for the payload proper. From the payload, another 3 bytes of our own header (Company ID and Node ID) are subtracted, and what is left is available for distances:

|                                      | Bytes         |
| ------------------------------------ | ------------- |
| Legacy advertising packet            | 31            |
| minus field header (length and type) | −2            |
| **Available payload**                | **29**        |
| minus Company ID (2) and Node ID (1) | −3            |
| **Space for distances**              | **26**        |
| 2-byte distances: 26 ÷ 2             | **13 robots** |

In the thesis prototype, distances were 4-byte `float` values: 26 ÷ 4 = 6 distances at most.

### 3.2 Anchor Packet

The anchor payload is a sequence of bytes, in this order:

| Position | Bytes | Content                                                      |
| -------- | ----- | ------------------------------------------------------------ |
| 0 and 1  | 2     | Company ID, always `FF FF` (test value, as in the prototype) |
| 2        | 1     | ID of the transmitting anchor (0, 1 or 2)                    |
| 3 and 4  | 2     | distance from robot 0 (ID 16)                                |
| 5 and 6  | 2     | distance from robot 1 (ID 17)                                |
| …        | 2     | one pair of bytes for each following robot                   |

With `M` robots the payload is `3 + 2·M` bytes long: 5 bytes with 1 robot, 23 bytes with 10 robots, 29 bytes with 13 robots. One more robot costs 2 bytes in the anchor packet.

Each distance is a 16-bit unsigned integer (`uint16`), in centimeters and rounded. The two bytes are written in little-endian order, that is, the least significant byte first and then the most significant one. The value `FF FF` (65535) means "distance not available", so the maximum representable distance is 65534 cm = 655.34 m.

**Example.** Anchor 1 with two robots (16 and 17) has measured robot 16 at 1.25 m and has not heard robot 17. The distances become:

- robot 16: 1.25 m = 125 cm = `0x007D`, written `7D 00` (low byte first);
- robot 17: not available, hence `FF FF`.

The complete payload, of 3 + 2·2 = 7 bytes, is:

```
FF FF  01  7D 00  FF FF
 │      │    │      └─ robot 17: not available
 │      │    └──────── robot 16: 125 cm
 │      └───────────── anchor 1
 └──────────────────── Company ID
```

The receiver does the reverse: it reads `7D 00` as `0x7D | (0x00 << 8)` = 125 cm = 1.25 m, and `FF FF` as "not available". The robot a pair of bytes refers to is obtained from its position: pair `j` (starting from 0) belongs to the robot with ID `16 + j`.

### 3.3 Robot Packets

Robots do not transmit distances: the payload is just `FF FF <ID>`, 3 bytes long, with `ID ≥ 16`. This holds for both `esp32_robot_broadcast` and `esp32_robot_broadcast_observer`; the latter also listens to the anchor packets.

### 3.4 Summary

| Source                                            | Payload                                        | Size            |
| ------------------------------------------------- | ---------------------------------------------- | --------------- |
| Anchor (`esp32_anchor`)                           | `FF FF <ID> <dist robot 0> … <dist robot M-1>` | `3 + 2·M` bytes |
| Robot (`esp32_robot_broadcast`)                   | `FF FF <ID>`                                   | 3 bytes         |
| Observer robot (`esp32_robot_broadcast_observer`) | `FF FF <ID>`                                   | 3 bytes         |

For comparison, the payload of the thesis prototype was 19 bytes, excluding the 2 bytes of the MSD field header: 3 bytes of our own header and 16 for four 4-byte `float` distances (the three measured by the node and the one towards the external device).

## 4. Position Computation (Anchors)

Each anchor measures the RSSI of every robot, filters it with the prototype's Kalman filter, converts it to distance with the path loss model and transmits the measured distances in advertising. By listening to the other two anchors, each one obtains the three distances of every robot.

The position is computed with the thesis trilateration (`trilaterateFromBaseline()`), the same one that localizes the external device in the prototype:

1. with nodes 0 and 1 as the baseline, `x` and the magnitude of `y` are obtained;
2. the distance from node 2 chooses the sign of `y`.

The three anchors must be measured by hand in the same reference system as the thesis: node 0 at the origin, node 1 on the `x` axis, node 2 with `y > 0` and not aligned with the first two (for example `(0,0)`, `(6,0)`, `(0,4)` for a 6×4 m area). A robot is localized only if all three anchors have received it.

**Alternative not adopted.** The position can be derived directly from the three circles, without a square root and without choosing the sign. In the thesis (Section 3.3.2) this method turned out to be worse with nearly aligned nodes. A numerical test on the model (RSSI noise of 3 dB, anchors at `(0,0)`, `(6,0)`, `(0,4)`) gives a median error of 1.23 m for the baseline method and 1.33 m for the three circles, with worse tails for the latter (90th percentile 2.86 m versus 3.94 m). This is a result of the model, not a measurement on the boards.

## 5. Configuration

Main constants of `esp32_anchor`:

| Constant                                                | Meaning                                                                                                                                                                              |
| ------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `NODE_ID`                                               | 0, 1 or 2, different on each anchor                                                                                                                                                  |
| `NUM_DEVICES`                                           | number of robots (at most 13)                                                                                                                                                        |
| `DEVICE_ID_BASE`                                        | ID of the first robot (16); the others follow in sequence                                                                                                                            |
| `NODE_X`, `NODE_Y`                                      | coordinates of the anchors in meters, measured by hand                                                                                                                               |
| `RSSI_AT_1M`, `PATH_LOSS_EXPONENT`, `MEASUREMENT_NOISE` | model parameters, to be recalibrated for each environment (as in the thesis)                                                                                                         |
| `PROCESS_NOISE`                                         | Q of the Kalman filter. Here it is 2.0 instead of 0.5 because it is the value expected to suit moving robots better: Q is an assumption about the system dynamics, not a measurement |
| `CALIBRATION_MODE`                                      | prints the RSSI values for calibration                                                                                                                                               |

Constant of `esp32_robot_broadcast`: `NODE_ID`, a different value for each robot (from 16 onwards, in sequence, within `NUM_DEVICES`).

Constants of `esp32_robot_broadcast_observer`: `NODE_ID` (different on each board, from 16 onwards), `NUM_DEVICES` and `DEVICE_ID_BASE` (the same as those of the anchors) and `NODE_X`, `NODE_Y` (the same coordinates as the anchors). The RSSI model parameters are not needed, because the robot does not measure RSSI.

## 6. Usage

1. Measure the positions of the three anchors in the reference system described above and enter them in `NODE_X` and `NODE_Y`.
2. Load `esp32_anchor` on three boards, changing `NODE_ID` (0, 1, 2).
3. Load `esp32_robot_broadcast` on each robot, changing `NODE_ID` (16, 17, …).
4. Alternatively, load `esp32_robot_broadcast_observer` on the robots, changing `NODE_ID` (16, 17, …), with `NUM_DEVICES`, `NODE_X` and `NODE_Y` equal to those of the anchors.
5. Connect one of the anchors to the computer: every 3 seconds it prints on the serial port a JSON with the anchors (`n`) and the localized robots (`d`):

```json
{"n":[{"i":0,"x":0.00,"y":0.00}, ...],"d":[{"i":16,"x":1.20,"y":2.10}, ...]}
```

An observer robot also prints on the serial port, every 3 seconds, a JSON in the same format, with the anchors and the localized robots in the same reference system. The same `visualize_robots.py` reads both.

## 7. Visualization with Background

`visualize_robots.py` shows anchors and robots in real time (JSON from the anchors or from an observer robot) and lets you place a scaled background image under the map, to see the robots' movement on a map, a floor plan or a street. For each robot it also draws the latest positions (`TRAIL_LENGTH`). It is launched with `python visualize_robots.py` after setting `SERIAL_PORT` (or `SOURCE = "simulator"`, see the section on simulation).

The background parameters are at the top of the script:

| Parameter                                   | Meaning                                                                                                                                                                                        |
| ------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `BACKGROUND_IMAGE`                          | path of the image (`None` = no background)                                                                                                                                                     |
| `METERS_PER_PIXEL`                          | scale of the image: with `0.01` one pixel is 1 cm, so a 5000×5000 pixel image covers 50×50 m                                                                                                   |
| `ORIGIN_PIXEL`                              | pixel of the image (x, y, from the top-left corner) that corresponds to the point (0, 0) of the anchors' reference system                                                                      |
| `VIEW_LIMITS`                               | area to show in meters `(xmin, xmax, ymin, ymax)`; if `None`, the whole image is shown                                                                                                         |
| `SHOW_GRID`, `GRID_STEP`, `GRID_LABEL_STEP` | grid over the background: thin lines every `GRID_STEP` meters (0.1 = decimeters) and bold lines, with the label on the axes, every `GRID_LABEL_STEP` meters (`SHOW_GRID = False` to remove it) |
| `MAX_IMAGE_SIDE`                            | maximum side in pixels: a larger image is reduced at startup and the scale adjusts automatically                                                                                               |

The scale should be chosen according to the real area: for a 6×4 m area at 1 cm per pixel, 600×400 pixels are enough. The anchor coordinates (`NODE_X`, `NODE_Y`) are measured in meters starting from the corner corresponding to `ORIGIN_PIXEL`, with $x$ pointing right and $y$ pointing up, as in the map; otherwise the points do not coincide with the floor plan.

## 8. Simulation on PC

The `sim` folder runs **the real sketches** (the very same `.ino` files) on a PC, without boards, and sends the result to the visualizer.

- `arduino_stub.h`: fake BLE and fake serial, only what the sketches need.
- `build.py`: compiles the sketches as PC libraries (a C++ compiler is required, for example `g++`; it can be specified with the `CXX` variable). It starts by itself at the first simulation and repeats whenever the sketches change.
- `sim.py`: creates a board for each anchor and each robot (each with its own state) and simulates one 3-second cycle at a time: the robots move, each scanning board receives the packets of the others (at most one per transmitter per cycle, as in the prototype) with the RSSI computed by the path loss model plus Gaussian noise, then runs `loop()`. The anchor coordinates, `NUM_DEVICES` and `DEVICE_ID_BASE` are read from the anchor sketch, so they do not need to be repeated.
- `run_sim.py`: runs the simulation without graphics and prints the position error. Options: `--robots`, `--cycles`, `--sketch broadcast_observer|broadcast`, `--motion waypoints|circle|line`, `--noise` (dB), `--loss` (probability of losing a packet), `--seed`, `--viewer`.

To see it with the map, set `SOURCE = "simulator"` in `visualize_robots.py` and run the script: it advances one cycle (3 simulated seconds) every 3 seconds, that is, in real time; `SIM_SPEED` speeds it up or slows it down (2 = double speed). The options are in `SIM_OPTIONS`; with `SHOW_TRUTH = True`, a cross with the true position appears next to the estimated point of each robot. With `SOURCE = "serial"` the script reads the real boards as before.

The JSON is the one printed by the board chosen in `viewer` (by default anchor 0).

**What it says and what it does not say.** The simulator checks that the code does what it claims: packet parsing, ID filtering, trilateration, robots not heard, exchange between anchors and robots. The RSSI, however, comes from the path loss model with Gaussian noise, not from a measurement: packet collisions, obstacles and the real behavior of the scanner are not simulated, and the code is compiled with a stub, not for the ESP32. With anchors at `(0,0)`, `(6,0)`, `(0,4)`, 10 robots, 3 dB noise and all parameters equal to those of the sketches, the median position error is about 0.6 m with stationary robots and about 1.1 m with robots moving at 0.3 m/s (90th percentile about 1.9 m): with moving robots, the Kalman filter lag and the update every 3 s weigh in. These are results of the model, not of the boards.

### 8.1 How the Simulator Works

**Folder structure.** The simulator looks for the sketches in `SKETCH_DIR` (configurable in `sim/build.py`, also relative to the `sim` folder) or in `firmware` next to the project folder. Each sketch sits in a folder with the same name as its `.ino` (`esp32_anchor`, `esp32_robot_broadcast`, `esp32_robot_broadcast_observer`). While running, `build.py` creates `sim/build` (it can be deleted at any time):

```
sim/build/
├── anchor.cpp, broadcast_observer.cpp, broadcast.cpp   support files generated from the sketches
├── anchor.so, broadcast_observer.so, broadcast.so      compiled libraries (.dll on Windows)
└── instances/                                          one copy of the library for each simulated board
```

**The problem.** The sketches use classes that exist only on the ESP32 (`BLEDevice`, `Serial`, `String`, `delay`). The simulator replaces them with fake versions, so the `.ino` files run on a PC without modifications, and a Python program acts as the "world" around the boards.

**`arduino_stub.h`.**

- `String` inherits from `std::string`, so `data[2]`, `data.length()` and `&data[3]` work as in the sketches.
- `Serial` does not write to a port: it accumulates the text in a string that Python reads after each `loop()`.
- `BLEAdvertising` stores the last payload set (`setAdvertisementData`): this is what the board "transmits".
- `BLEScan` keeps the sketch's `onResult` function: the simulator calls it to deliver a packet, as the ESP32 would.
- `delay()` does nothing, because time is managed by the simulator (one cycle = 3 s).

**`build.py`: from sketch to library.** For each sketch type (`anchor`, `broadcast_observer`, `broadcast`):

1. it reads the `.ino` and removes the `#include <BLE...>` lines (replaced by the stub);
2. it replaces `#define NODE_ID <number>` with `#define NODE_ID g_node_id`, a variable: this way a single library serves all the boards, each with its own ID;
3. it appends at the end five functions exported to Python: `sim_start(id)` (sets the ID and runs `setup()`), `sim_loop()` (runs `loop()`), `sim_rx(data, length, rssi)` (delivers a packet to the `onResult` function), `sim_payload()` (reads the transmitted payload), `sim_serial()` (reads and empties the printed text);
4. it writes the `.cpp` in `sim/build` and compiles it with `g++ -shared` (compiler configurable with the `CXX` variable). It recompiles only if the library is missing or if the sketch, the stub or `build.py` are newer.

**One copy of the library per board.** The state of the sketch (`nodeState`, the Kalman filter, the distances) consists of global variables, and two loads of the same file would share the same state. For this reason `instance_library()` copies the library into `sim/build/instances` with an always-new name (process, counter, type, index), and each board loads its own copy. The new name also avoids overwriting a file that is already loaded, which would terminate the program. Old copies are deleted at startup.

**`sim.py`: the pieces.**

- `read_defines()` reads `NUM_FIXED_NODES`, `NUM_DEVICES`, `DEVICE_ID_BASE`, `NODE_X` and `NODE_Y` from the anchor sketch (with regular expressions): the values do not need to be repeated in the simulator. For this reason, in the anchor sketch the constants must keep their current form (`#define NUM_DEVICES 10`, `const float NODE_X[...] = {...};`).
- `Board` is a board: it loads its library with `ctypes` and offers `receive(payload, rssi)`, `payload()` and `run_loop()`, which runs `loop()` and keeps the last JSON line printed.
- `Robot` has a position and a motion. `Robot.move(dt)` implements three modes: `waypoints` (goes in a straight line towards a random point and then picks another), `circle` (turns in a circle around the center of the area) and `line` (goes back and forth along x). It moves within the rectangle of the anchors reduced by 0.3 m per side, at speed `speed`.
- `Simulation` creates the three anchors and one board per robot. The anchors scan and, only with the `broadcast_observer` sketch, so do the robots. A single random generator, initialized with `seed`, decides initial positions, points to reach, noise and losses: same `seed`, same simulation.

**`Simulation.step()`: one 3-second cycle.**

1. Each robot moves by `speed · 3` meters.
2. The payloads that each board is currently transmitting are read.
3. Each scanning board receives, from every other board, a packet with the model's RSSI: `RSSI = RSSI_1m − 10 · n · log10(distance) + noise`, with `RSSI_1m = −56` and `n = 3.0` (`WORLD_*` constants in `sim.py`, equal to those of the sketches, but modifiable to simulate an environment different from the calibration one), rounded to an integer. One packet per transmitter per cycle, like the scanner's duplicate filter; with `packet_loss` each packet can be lost.
4. Each board runs `loop()`: it updates the filter and the distances, computes the positions, updates the payload and prints the JSON.
5. The last JSON of the board chosen in `viewer` is returned, with an additional key `"t"`: the true positions of the robots.

Since the payloads are read before the `loop()` calls, each board receives the data of the previous cycle: this is a real delay, and it explains why the positions read from a robot are slightly less accurate than those read from an anchor.

**The link with the visualizer.** `visualize_robots.py` does not know whether the JSON comes from the boards or from the simulator: with `SOURCE = "simulator"` the `read_positions()` function returns `simulation.step()` instead of reading the serial port. If present, the `"t"` key is drawn as crosses (`SHOW_TRUTH`). `import serial` is inside the real-boards branch, so the simulator does not require `pyserial`.

**Number of robots.** The maximum is `NUM_DEVICES`, which must be changed in `esp32_anchor` and `esp32_robot_broadcast_observer` (same value, at most 13 because of the legacy payload). With `--robots` or `n_robots`, fewer are simulated: the robots that do not exist do not transmit and the anchors mark them as not available.

**How to read the code (suggested order).** 1) `arduino_stub.h`; 2) `build.py`, starting from `wrapper_source()` and the tail with the five functions; 3) `sim.py`: first `Board`, then `Robot`, then `Simulation.step()`; 4) `run_sim.py`; 5) the changes in `visualize_robots.py` (`SOURCE`, `SIM_OPTIONS`, `draw_truth`).

## 9. Limits and Status

- **Not tested on boards.** The position computation was verified on PC with exact distances, and the two-byte conversion with a round-trip test. The three sketches have not been compiled for the ESP32 nor tried on the boards. The observer robot sketch was tried on PC with the anchor packets built from exact distances (positions reconstructed with an error below 1 cm; a robot not heard by an anchor is not localized; the packets of the other robots are ignored).
- **Scanner capacity.** The behavior of the BLE scanner with 10 robots transmitting together has not been verified. As in the prototype, the duplicate filter is reset every cycle (3 s), so at most one sample per robot per cycle is obtained.
- **Observer robots.** They depend on the anchors: if a robot is not heard by one of the three anchors, it is not localized by the robot either. The robot does not measure distances towards the other robots, so there is no relative computation between robots.
- **Three anchors.** If a robot is not heard by one of the three anchors, it is not localized. Accuracy depends strongly on geometry and RSSI noise, as in the prototype.
- **No expiry of measurements.** As in the prototype, a distance is not invalidated if a robot stops transmitting.
- **Legacy packets.** Beyond 13 robots, the distances would have to be split over several packets or a board with Bluetooth 5.0 would be needed.
