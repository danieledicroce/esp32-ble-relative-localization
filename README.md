# ESP32 BLE Autonomous Relative Localization & Tracking

An autonomous, anchor-free indoor relative positioning and tracking system developed on **ESP32-WROOM-32** microcontrollers. The firmware enables a cluster of three symmetric nodes to calculate a mutual 2D coordinate frame without manual distance measurements or pre-calibrated spatial anchors, while concurrently tracking an external BLE broadcast target (e.g., an Android smartphone).

---

## Key Highlights

- **Concurrent Dual-Role Operation:** Each ESP32 runs simultaneously as a GAP **Broadcaster** and **Observer**, continuously transmitting custom advertisement payloads while scanning surrounding peers.
- **Connectionless Architecture:** All peer-to-peer distance telemetry travels inside the `Manufacturer Specific Data` field (`0xFFFF`), avoiding GATT connection overhead or external Wi-Fi/ESP-NOW networks.
- **Signal Conditioning:** Independent scalar **Kalman filtering** on raw RSSI measurements for each peer to suppress environmental multipath and channel fading.
- **Autonomous Coordinate Frame:** Resolves relative 2D positions by fixing Node 0 at $(0,0)$, aligning Node 1 along the positive $X$-axis, and placing Node 2 in the positive $Y$ half-plane.
- **External Target Tracking:** Disambiguates mirror symmetry using three non-collinear anchors to track an external BLE beacon in real time.

---

## Firmware Development & Commit History

The repository reflects the incremental software design discussed in Chapter 2 of the thesis:

1. `feat(ble): implementation of broadcaster and observer roles`
   - Concurrent setup of GAP Broadcaster and Observer roles via ESP32 `BLEDevice`.
   - Initial 11-byte broadcast payload structure (`Company ID`, `Node ID`, initial coordinates).
2. `feat(rssi): implementation of log-distance path loss distance estimation`
   - Integration of the Log-Distance Path Loss model.
   - Empirical calibration parameters ($d_0 = 1\text{ m}$, $\text{RSSI}(d_0)$, and path loss exponent $n$).
3. `feat(filter): implementation of scalar kalman filter on rssi readings`
   - Per-peer scalar Kalman filter operating directly on the logarithmic RSSI domain (dBm) to prevent non-linear error amplification.
4. `feat(localization): implementation of relative trilateration and distance exchange`
   - Extended 15-byte payload exchanging mutual distance arrays (`float[3]`).
   - Relative 2D trilateration algorithm resolving the reference frame across the three nodes.
5. `feat(tracking): implementation of external device localization and disambiguation`
   - State refactoring (`FixedNodesState`, `ExternalDeviceState`).
   - Extended 19-byte advertising payload including target distance telemetry.
   - Non-collinear geometric disambiguation resolving the $\pm y$ mirror ambiguity.

---

## Advertising Payload Structure

The firmware uses legacy BLE advertising packets carrying a compact 19-byte custom payload:

| Byte Offset | Field | Type | Description |
|---|---|---|---|
| `[0..1]` | Company ID | `uint16_t` | `0xFFFF` (Reserved by Bluetooth SIG for test/prototyping) |
| `[2]` | Node ID | `uint8_t` | Identifier of transmitting anchor (`0`, `1`, or `2`) |
| `[3..14]` | Peer Distances | `float[3]` | 3 $\times$ 4-byte IEEE 754 floats: mutual distances measured by this node |
| `[15..18]` | Device Distance | `float` | 4-byte IEEE 754 float: distance from this node to the external target |

---

## Mathematical Model

### 1. Distance Estimation (Log-Distance Path Loss)
Raw RSSI values are converted to geometric distances via the log-distance path loss model:

$$d = d_0 \cdot 10^{\frac{\text{RSSI}(d_0) - \text{RSSI}}{10 \cdot n}}$$

where $d_0 = 1.0\text{ m}$, $\text{RSSI}(d_0)$ is the calibrated signal strength at 1 meter, and $n$ is the path loss exponent.

### 2. Scalar Kalman Filter

To prevent exponential error amplification, the Kalman filter is applied directly to the logarithmic RSSI domain (dBm) rather than to computed distances.

**Prediction:**

$$\hat{x}_{k\vert{}k-1} = \hat{x}_{k-1\vert{}k-1}$$

$$P_{k\vert{}k-1} = P_{k-1\vert{}k-1} + Q$$

**Update:**

$$K_k = \frac{P_{k\vert{}k-1}}{P_{k\vert{}k-1} + R}$$

$$\hat{x}_{k\vert{}k} = \hat{x}_{k\vert{}k-1} + K_k (z_k - \hat{x}_{k\vert{}k-1})$$

$$P_{k\vert{}k} = (1 - K_k) P_{k\vert{}k-1}$$

### 3. Baseline Trilateration & Disambiguation
For baseline length $d_{01}$ along the $X$-axis:
$$x = \frac{d_0^2 - d_1^2 + d_{01}^2}{2 \cdot d_{01}}, \quad y = \pm \sqrt{\max\left(0, \, d_0^2 - x^2\right)}$$

For the external target, the mirror ambiguity ($\pm y$) is resolved by choosing the candidate $(x, y)$ that minimizes:
$$\vert{}\sqrt{(x - x_2)^2 + (y - y_2)^2} - d_2\vert{}$$

---

## Configuration & Tuning

Configure the firmware parameters at the top of the sketch according to your test environment:

```cpp
#define NODE_ID 0              // Assign 0, 1, or 2 uniquely per physical board
#define EXTERNAL_ID 3          // Identifier for the external device (e.g., smartphone)
#define RSSI_AT_1M -56.0       // Calibrated RSSI at 1 meter (dBm)
#define PATH_LOSS_EXPONENT 3.0 // Path loss exponent n (typically 2.0 to 4.0 indoor)
#define PROCESS_NOISE 0.5      // Kalman Q: lower for static nodes, higher for mobility
#define MEASUREMENT_NOISE 10.0 // Kalman R: sample variance of RSSI at a fixed distance
