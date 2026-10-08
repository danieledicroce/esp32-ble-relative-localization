# Relative Localization with ESP32 and Bluetooth Low Energy

Manual for reproducing the system described in the thesis *Study and Development of Embedded Solutions for Dynamic Position Detection in Indoor Environments Using BLE Technology* (Daniele Di Croce, Università degli Studi della Basilicata, A.Y. 2025/2026).

Three ESP32 boards localize each other by measuring the RSSI of the BLE packets they exchange. An Android smartphone can be localized as a fourth element. The positions are printed in JSON format on the serial port and drawn by a Python script.

## 1. Required Materials

- 3 ESP32-WROOM-32 boards (same model for all three).
- 3 USB cables for programming and powering the boards.
- 1 Android smartphone with the *nRF Connect for Mobile* app (Nordic Semiconductor).
- 1 computer, connected to one of the three boards (in the tests described in the thesis, Node 0).

**The tests were carried out on a computer running the Arch Linux operating system; the information provided for other operating systems was found online but has not been tested.**

## 2. Repository Structure

```text
{root_repo}/
├── firmware/
│   └── esp32_rel_loc/
│       └── esp32_rel_loc.ino    # Arduino sketch (the same for all three boards)
└── python/
    └── visualize_positions.py   # Visualization script
```

The sketch folder must have the same name as the `.ino` file, as required by the Arduino IDE.

## 3. Software Installation

The versions listed are those with which the system was developed and tested. They are not prescriptive: other versions may work, but have not been verified.

| Component | Version used in the tests |
|---|---|
| Arduino IDE | 2.3.10 |
| Board package *esp32 by Espressif Systems* | 3.3.12 |
| Python | 3.14.7 |
| `matplotlib` | 3.11.2 |
| `pyserial` | 3.5 |
| Operating system | Arch Linux |

### 3.1 Arduino IDE and ESP32 Board Support

1. Install the Arduino IDE from the official website: https://www.arduino.cc/en/software
2. Open *Tools > Board > Boards Manager*. There is no need to add any address in the preferences: the package is available directly in the Boards Manager.
3. Search for **esp32** and install **esp32 by Espressif Systems**. In the Boards Manager, the version can be chosen from the drop-down menu in the package card.
4. The `BLEDevice` library is included in the core: no external libraries are required.

### 3.2 Python and Visualization Libraries

1. Install Python 3.
2. Create a virtual environment and install the libraries:
   - **Linux / macOS:**
     ```bash
     cd python
     python -m venv .venv
     source .venv/bin/activate
     pip install pyserial matplotlib
     ```
   - **Windows:**
     ```cmd
     cd python
     python -m venv .venv
     .venv\Scripts\activate
     pip install pyserial matplotlib
     ```

#### Linux Users Only

3. Install the Tkinter graphical support, required for `matplotlib` to open the plot window:
   - Arch Linux: `sudo pacman -S tk`
   - Debian and Ubuntu: `sudo apt install python3-tk` (not tested)
4. Serial port access (`dialout` group):
   - Arch Linux: it is not necessary to add the user to the `dialout` group.
   - Debian and Ubuntu: the system has not been tested on these distributions, so it may be necessary to add the user to the `dialout` group (`sudo usermod -aG dialout $USER`, then restart the session).

## 4. Firmware Preparation and Upload

The three boards run the same sketch. Only the `NODE_ID` constant changes.

1. Open the sketch `firmware/esp32_rel_loc/esp32_rel_loc.ino` in the Arduino IDE.
2. Select the board in *Tools > Board > esp32* and choose **ESP32 Dev Module**. All the other settings in the *Tools* menu are left at their default values.
3. For the **first board**, set `#define NODE_ID 0`, connect it to the computer, select the port in *Tools > Port* and upload the sketch.
4. Repeat with `#define NODE_ID 1` for the second board and with `#define NODE_ID 2` for the third.
5. **Important check**: each board must have a different Node ID. With two identical boards the system does not report errors but mixes their measurements.

The model parameters are located at the beginning of the sketch:

| Constant | Value used | Meaning |
|---|---|---|
| `CALIBRATION_MODE` | false | `true` prints the raw RSSI values of the fixed nodes for calibration and disables the JSON output |
| `RSSI_AT_1M` | -56.0 | RSSI measured at 1 m (to be recalibrated for each environment) |
| `PATH_LOSS_EXPONENT` | 3.0 | path loss exponent $n$ (to be recalibrated) |
| `PROCESS_NOISE` | 0.5 | process noise $Q$ of the Kalman filter |
| `MEASUREMENT_NOISE` | 10.0 | measurement noise $R$ (to be recalibrated) |

## 5. Calibration of RSSI at 1 m and Path Loss Exponent

The values in the table are valid for the thesis environment. When the environment changes, they must be derived again.

1. Set `#define CALIBRATION_MODE true` at the beginning of the sketch and upload it again to the board connected to the computer. In this mode the board prints on the serial port the raw RSSI values of the packets received from the other fixed nodes, and the JSON output of the positions is disabled, so the Python script receives no data as long as the mode remains active.
   Open the Arduino IDE Serial Monitor at **115200 baud**: each packet received from another fixed node produces a line like the following (example values):
   ```
   [Node 1] Raw RSSI: -56 | Filtered RSSI: -56 -> Dist: 1.00
   ```
2. Place two boards 1 m apart, in direct line of sight and without obstacles. To avoid errors and minimize uncontrolled variables, place both boards at the same height from the ground; the mutual orientation is instead secondary, since the PCB antennas of the ESP32-WROOM-32 are quasi-omnidirectional. Finally, collect at least 10 samples.
3. Assuming a Gaussian distribution of the errors, compute the mean of the collected `Raw RSSI` values: this is the value of `RSSI_AT_1M`.
4. Repeat the measurement at a second known distance $d_1$ (in the thesis 5 m, with a mean RSSI of -77 dBm) and derive the exponent with the formula
   $n = \dfrac{RSSI(d_0) - RSSI(d_1)}{10 \log_{10}(d_1 / d_0)}$ where $d_0 = 1$ m.
5. Update the two constants in the sketch, set `CALIBRATION_MODE` back to `false` and upload it to **all three** boards.

## 6. Smartphone Configuration as an External Device (Optional)

With the *nRF Connect for Mobile* app, the smartphone is configured as a pure *Broadcaster*.

1. Open the **nRF Connect for Mobile** application.
2. Open the side drawer menu (hamburger icon at the top left or swipe from the left edge) and select the **Devices** item.
3. In the top navigation bar, select the **ADVERTISER** tab.
4. Press the floating **`+`** button (bottom right) to create a new transmission packet (Advertising packet).
5. Configure the packet (setting the desired device name and the broadcast data) and turn on the switch next to the created profile to start continuous transmission.

Settings that must be in place (from the thesis, Chapter 3):

- Manufacturer Specific Data with Company ID `0xFFFF` and identifier `0x03` (corresponding to `EXTERNAL_ID 3`).
    1. Press **Add record** under the *Advertising data* section.
    2. Select the **Manufacturer Data** item.
    3. In the **Company Identifier** field enter `0xFFFF`.
    4. In the **Data** field enter `03` (corresponding to `EXTERNAL_ID 3`).
    5. Save by pressing *OK*.
- non-connectable and non-scannable packet;
- **"Adv. Extension" option disabled** (primary and secondary PHY set to Legacy): with Extended Advertising the boards do not see the phone;
- advertising interval of about 250 ms.

## 7. Node Placement and Startup

1. Place the three boards at the same height from the ground, in direct line of sight and without obstacles in between. The nodes must form a non-aligned (triangular) geometric arrangement, a requirement for a correct 2D trilateration of the target and to avoid geometric ambiguities. The mutual distance between the boards must fall within the reliable operating range of BLE (recommended within 5–10 m; beyond this threshold, signal attenuation and noise significantly degrade the RSSI estimate).
2. Power the three boards (the power-on order does not matter; the nodes autonomously start the advertising and reception routines).
3. Connect the Node 0 board to the computer via USB cable and identify the assigned serial port:
   - **Linux / macOS:** check the available devices by running:
     ```bash
     ls /dev/ttyUSB* /dev/ttyACM*
     ```
     *(usually identified as `/dev/ttyUSB0` or `/dev/ttyACM0`)*
   - **Windows:** open **Device Manager** and look at the **Ports (COM & LPT)** section to find the assigned identifier (e.g. `COM3` or `COM4`).
4. Open `python/visualize_positions.py` and check that the `SERIAL_PORT` variable matches the port identified in the previous step.
5. Run the script inside the virtual environment:
   - **Linux / macOS:**
     ```bash
     cd python
     source .venv/bin/activate
     python visualize_positions.py
     ```
   - **Windows:**
     ```cmd
     cd python
     .venv\Scripts\activate
     python visualize_positions.py
     ```
6. Wait for the graphical interface to open: the first samples and the estimated position of the nodes appear on screen within a few seconds of the start of the serial reading.

## 8. What to Expect

Every 3 seconds the board prints a JSON line, for example:

```json
{"n":[{"i":0,"x":0.00,"y":0.00},{"i":1,"x":1.20,"y":0.00},{"i":2,"x":0.65,"y":0.50}],"d":{"x":0.30,"y":-0.40,"r":1.25}}
```

- `n`: the three fixed nodes with their coordinates in meters.
- `d`: the external device, with the `x` and `y` coordinates and the distance `r`.

The plot shows:

- nodes 0, 1 and 2 in blue, red and green respectively, with the coordinates at the top left;
- the external device as a magenta X, visible only once it has been localized;
- the gray X labeled "Last position" if the distance from the device remains the same for 5 consecutive readings, that is, if the device has stopped transmitting.

Node 0 is always at the origin, Node 1 on the x axis and Node 2 with a positive y coordinate. The coordinates change slightly from one board to another, because each board computes them with its own measurements.

## 9. Common Problems

| Problem | Cause and solution |
|---|---|
| Meaningless Node IDs on the serial monitor | The Company ID check is missing or the packet does not belong to the system (the final code includes it) |
| Unreadable characters after uploading | Serial port in an abnormal state: unplug and reconnect the USB cable, press the board's reset button, or clear the Arduino IDE Serial Monitor cache with the dedicated software button |
| The phone is never detected | Disable "Adv. Extension" in nRF Connect (Legacy advertising only) |
| The plot does not appear | Install Tkinter support |
| The plot stays empty and the serial monitor shows `Raw RSSI` lines | `CALIBRATION_MODE` is `true`: set it to `false` and upload the sketch again |
| The window opens but is empty | Incomplete JSON lines or port used by another program: close the Arduino IDE Serial Monitor and restart the script |
| The node appears in a meaningless position | Two boards with the same `NODE_ID`, or very unstable RSSI (a limitation of using RSSI alone) |

For the complete analysis of the problems encountered, see Chapter 3 of the thesis.
