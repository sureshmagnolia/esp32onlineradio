# 3D Printable Stereo Desktop Cabinet for Waveshare ESP32-S3-Touch-LCD-2.8

A retro-modern desktop internet radio cabinet custom-engineered for the **Waveshare ESP32-S3-Touch-LCD-2.8** development board and its bundled **dual 2030 stereo cavity speakers**.

---

## Aesthetic Architecture & Board Placement (Board Next to Bottom Wall)

* **Continuous, Sleek Flat Bottom Wall (ZERO Scallop Cutout)**:
  * The board is positioned directly adjacent to the inside bottom wall ($0.5\text{ mm}$ clearance).
  * The **MicroSD socket mouth sits only $2.5\text{ mm}$ from the outer bottom wall**.
  * The slot through the bottom wall is a clean, flush $13.5\text{ mm} \times 2.8\text{ mm}$ card slot with a subtle $0.5\text{ mm}$ lead-in chamfer.
  * When pushed to eject, the card springs **$1.3\text{ mm}$ to $1.5\text{ mm}$ directly outside the flat bottom wall**, allowing effortless thumb/index pinch removal.
  * **Zero aesthetic damage**: The cabinet maintains a 100% continuous, uninterrupted, sleek rectangular silhouette!
* **Ultra-Short, Crisp Button Plungers (Sealed RESET Architecture)**:
  * Two active tactile switch plungers are provided: **BOOT** ($X = -26.23\text{ mm}$, single touch Next, double touch Prev, hold Volume Up, wake) and **BAT_PWR** ($X = -10.23\text{ mm}$, battery power switch).
  * The middle **RESET** hole ($X = -18.23\text{ mm}$) is **permanently closed and solid in the front case**, preventing accidental hard reboots while handling the device.
  * Plungers are short ($4.5\text{ mm}$), rigid, and wobble-free with internal retaining flanges.
* **Balanced Desktop Proportions**:
  * Enclosure dimensions: $142.0\text{ mm}$ Width $\times 62.0\text{ mm}$ Height $\times 32.0\text{ mm}$ Depth.
  * Active screen viewing aperture: $58.4 \times 44.0\text{ mm}$, centered horizontally at $X = 0$, with balanced top and bottom bezels.
  * Flanked symmetrically by dual vertical slotted acoustic grilles for the Left and Right 2030 cavity speakers.
* **Right Wall USB Type-C Port**:
  * Centered at $Y = -3.23\text{ mm}$ with a wide cable collar for standard USB-C cables.

---

## Generated STL Files

Located in **[`d:\ESP32Radio\3D_Enclosure_Waveshare28\`](file:///d:/ESP32Radio/3D_Enclosure_Waveshare28/)**:

| File | Description | Recommended Orientation |
| :--- | :--- | :--- |
| **[`Waveshare28_Radio_Virtual_Assembly_Exploded.stl`](file:///d:/ESP32Radio/3D_Enclosure_Waveshare28/Waveshare28_Radio_Virtual_Assembly_Exploded.stl)** | **Full 3D Virtual Exploded Assembly STL**: All enclosure parts + Waveshare board + dual 2030 speakers exploded along the Z-axis for slicer and 3D viewer inspection! | Slicer / CAD 3D viewer inspection. |
| **[`Waveshare28_Radio_Virtual_Assembly_Mated.stl`](file:///d:/ESP32Radio/3D_Enclosure_Waveshare28/Waveshare28_Radio_Virtual_Assembly_Mated.stl)** | **Full 3D Virtual Mated Assembly STL**: Complete CAD stackup fully mated at zero-clearance operating positions. | Slicer / CAD 3D viewer inspection. |
| **[`Waveshare28_Radio_Front_Case.stl`](file:///d:/ESP32Radio/3D_Enclosure_Waveshare28/Waveshare28_Radio_Front_Case.stl)** | Front cabinet housing with dual speaker chambers, 100% clean viewing aperture, precision glass nest, 2 bottom button holes (RESET hole sealed), SD slot, and right USB-C port. Flat continuous bottom wall! | Front face down on build plate ($Z=0$). |
| **[`Waveshare28_Radio_Back_Cover.stl`](file:///d:/ESP32Radio/3D_Enclosure_Waveshare28/Waveshare28_Radio_Back_Cover.stl)** | Rear cover shell with solid corner pillars, 10mm recessed screw wells, interlocking seal lip, 7 sound vents, battery cradle, and $8^\circ$ desktop tilt feet. | Flat bottom on build plate ($Z=0$). |
| **[`Waveshare28_Radio_Board_Clamp.stl`](file:///d:/ESP32Radio/3D_Enclosure_Waveshare28/Waveshare28_Radio_Board_Clamp.stl)** | 3D-printed rear retention frame with 4 registration nubs and outer chassis screw ears that locks the PCB firmly into the front bezel. Zero collision with connectors! | Flat on build plate. |
| **[`Waveshare28_Radio_Button_Plungers.stl`](file:///d:/ESP32Radio/3D_Enclosure_Waveshare28/Waveshare28_Radio_Button_Plungers.stl)** | Set of 2 precision captive button plungers ($4.5\text{ mm}$ reach) for BOOT and BAT_PWR. | Flange flat on build plate. |
| **[`Waveshare28_Radio_All_Parts.stl`](file:///d:/ESP32Radio/3D_Enclosure_Waveshare28/Waveshare28_Radio_All_Parts.stl)** | Complete print plate with all 4 printable parts arranged and oriented for a single print job. | Ready to slice as-is. |

---

## 3D Printing Guidelines

* **Material**: PLA, PLA+, PETG, or ABS/ASA.
* **Layer Height**: `0.20 mm` (or `0.16 mm` for ultra-smooth acoustic grilles and bevels).
* **Perimeters / Walls**: 3 to 4 perimeters (`1.2 mm` to `1.6 mm` wall thickness).
* **Infill**: 15% to 20% (Gyroid or Grid infill).
* **Supports**:
  * **Front Case**: Print with front face flat on the bed. Minimal supports needed only for the small USB-C cutout bridge.
  * **Back Cover**: Zero supports needed when printed flat on its base.
  * **Board Clamp & Buttons**: Zero supports needed.

---

## Hardware Fasteners Needed

1. **Board Retention (from inside the front case)**:
   * $4\times$ **M2.5 or M3 $\times$ 5–6 mm** self-tapping or machine screws (fastening the 3D-printed **Board Clamp Frame** into the 4 front chassis bosses).
2. **Case Closure (from the rear cover into the front corner bosses)**:
   * $4\times$ **M3 $\times$ 12 mm, 16 mm, or 20 mm** screws:
     * With an **M3 $\times$ 16 mm screw**: Passes through the $5.0\text{ mm}$ internal shoulder and protrudes **$11.0\text{ mm}$ deep** into the front case boss.
     * With an **M3 $\times$ 12 mm screw**: Protrudes **$7.0\text{ mm}$** into the front boss.
3. **Speakers**:
   * Dual 2030 cavity speakers press-fit into the internal side pockets (secure with a dab of B-7000 or thin double-sided foam tape).
4. **Battery (Optional)**:
   * 3.7V Li-ion pouch cell (up to $60\text{ mm} \times 28\text{ mm} \times 7\text{ mm}$) fits inside the rear battery cradle and plugs directly into the onboard BAT connector.

---

## Step-by-Step Assembly Instructions

1. **Install Button Plungers**:
   * Push the 2 button plungers (`BOOT` and `BAT_PWR`) through the **bottom wall** holes from the **inside** out. The wide retaining flange prevents them from falling through.
2. **Mount the Display Board**:
   * Lay the front cabinet face down.
   * Drop the display board into the perimeter retention nest ($73.8 \times 51.4\text{ mm}$). The front glass seats flush against the bezel frame with zero obstruction over the screen.
   * Place the 3D-printed **Board Clamp Frame** over the back of the PCB and secure it to the 4 chassis bosses with M2.5/M3 screws.
3. **Install Dual Stereo Speakers**:
   * Slide the Left and Right 2030 cavity speakers into the side acoustic chambers.
   * Plug the 4-pin speaker cable into the top onboard speaker header.
4. **Connect Battery (Optional)**:
   * Place the battery in the rear cradle and connect to the BAT header.
5. **Close Enclosure**:
   * Align the rear cover's perimeter lip into the front cabinet.
   * Secure with 4x M3 screws through the rear counterbored holes. The solid corner pillars meet the front case bosses with zero flex.

---

## 🔬 Virtual Assembly & Fitment Verification

The complete mechanical assembly was validated in CAD using [`virtual_assembly_model.py`](file:///d:/ESP32Radio/3D_Enclosure_Waveshare28/virtual_assembly_model.py) and rendered with [`render_realistic_previews.py`](file:///d:/ESP32Radio/3D_Enclosure_Waveshare28/render_realistic_previews.py):
* **Photorealistic Board CAD Geometry**:
  * **ESP32-S3 Module**: Nickel-silver stamped RF shielding can with laser-etched relief, 2.4GHz inverted-F meander PCB antenna, and IPEX1/U.FL external antenna micro-connector.
  * **Shrouded Expansion Suite**: Shrouded GH1.25/MX sockets with internal gold-plated pin arrays (12-Pin multi-function, 4-Pin UART/I2C, 2-Pin battery, 2-Pin speaker).
  * **Audio Subsystem**: Dedicated ES8311 audio codec IC, NS4168 class-D amplifier IC, shielded power inductors, and decoupling ceramic capacitor banks.
  * **RTC Subsystem**: Metal leaf-spring battery retainer with seated CR coin cell.
  * **MicroSD Assembly**: Stamped metal push-push cage with ground tabs, card guide ribs, and inserted card with finger lip.
  * **USB Type-C**: Seamless deep-drawn stainless steel shell with tongue insulator.
* **100% Unified Monolithic U-Bracket (Single-Piece Solid)**:
  * The clamp wings now solidly overlap the top crossbar by $6.0\text{ mm}$ ($Y = 22.0\text{ to }28.5\text{ mm}$), permanently uniting the two side wings and the top mounting bar into **ONE rigid, monolithic U-shaped arch**.
  * Rests directly on top of the 4 factory brass standoffs ($Z = 9.8\text{ mm}$), elevating it safely above all SMD components, connectors, and the USB-C metal shell.
  * Clamp wings are centered directly along the standoff axes ($X = -30.00\text{ mm}$ on left, $X = +22.54\text{ mm}$ on right), completely clear of the USB-C port ($X = +25.5\text{--}31.0\text{ mm}$).
  * **Zero cuts/arches on the bracket**: 100% solid, flat, continuous structural wings.
* **100% Horizontal Side-Entry Connectors Facing Periphery (Matching Physical Hardware)**:
  * **Bottom Edge**: 12-pin expansion header is modeled with its true compact SH1.0 footprint ($12.0 \times 3.0 \times 1.7\text{ mm}$), opening outward towards the bottom periphery ($-Y$). It sits between the MicroSD socket and the bottom-right standoff, leaving $> 1.7\text{ mm}$ of clear open space to the standoff flange ($> 4.19\text{ mm}$ to screw center).
  * **Top Edge**: Battery (2-pin), RTC (2-pin), and Speaker (4-pin) open outward towards the top periphery ($+Y$).
  * **Right Edge**: UART (4-pin) and I2C (4-pin) open outward towards the right periphery ($+X$).
  * **All 4 Factory Standoffs 100% Unobstructed**: Every brass standoff is fully exposed and visible, with zero component overlap.
* **Enlarged Smooth-Fit Perimeter Cavity (+1.5mm to +2.0mm Tolerance)**:
  * Front case glass nest enlarged from $73.8 \times 51.4\text{ mm}$ to **$75.0\text{ mm} \times 52.5\text{ mm}$** ($\approx 1.0\text{ mm}$ clearance per side).
  * Guarantees smooth drop-in insertion without friction, binding, or tight-tolerance 3D printing shrinkage issues.
* **Clean 2-Button Architecture (`RESET` Hole Closed)**:
  * To prevent accidental hardware reboots during playback, the middle `RESET` hole is **100% closed with solid plastic**.
  * Only 2 precision buttons are exposed through the cabinet wall:
    * **`BOOT` (User Programmable Button)**: $X = -16.54\text{ mm}$ (Single click = Next, Double click = Prev, Hold = Vol+, Deep sleep wakeup).
    * **`BAT_PWR` (Hardware Power Button)**: $X = -2.54\text{ mm}$ (Exclusive battery Power Up / Power Down).
  * **Corrected Orientation & Precision Plunger Architecture**:
    * **External Button Cap / Shaft**: $\varnothing 3.6\text{ mm}$ sliding through $\varnothing 4.2\text{ mm}$ guide tunnels, protruding $1.4\text{ mm}$ outside the bottom wall ($Y = -32.40\text{ to } -31.00\text{ mm}$) for a comfortable, tactile press.
    * **Internal Retaining Collar / Flange**: $\varnothing 5.2\text{ mm} \times 0.8\text{ mm}$ thick, captivating inside a $\varnothing 5.6\text{ mm} \times 1.0\text{ mm}$ counterbore pocket in the cabinet wall ($Y = -30.20\text{ to } -29.40\text{ mm}$). Cannot fall out.
    * **Actuator Contact Nub**: $\varnothing 1.8\text{ mm} \times 0.6\text{ mm}$ pin pointing inward ($+Y$) directly into the microswitch actuator, resting at $Y = -28.80\text{ mm}$ with a crisp $0.12\text{ mm}$ pre-travel clearance to the switch tip ($Y = -28.68\text{ mm}$).
* **Direct MicroSD Access**: Centered at $X = +5.96\text{ mm}$, mouth sits only $2.8\text{ mm}$ from the flat bottom wall; ejected cards spring $1.5\text{ mm}$ into your fingers.
* **Continuous Aesthetic Bottom**: Zero scallops or notches cut into the outer case profile.
* **Sleek Flat Rear Cover**: 2 rear desk-stand tilt feet blocks completely removed; back surface is 100% clean and flat.
* **Unified Bonded Display & Board Module**: In CAD & 3D viewers, the touch glass, active LCD panel, midframe, and PCB motherboard are treated as 1 rigid, factory-bonded hardware unit moving together.
* **Symmetrical Display Window**: Front viewing window ($58.4 \times 44.0\text{ mm}$) and active screen ($57.6 \times 43.2\text{ mm}$) are centered symmetrically at $X = 0.00\text{ mm}$ between the stereo speakers.
* **Rock-Solid Screw Reach**: Standard M3x16mm screws thread $11.0\text{ mm}$ deep into the front bosses.
