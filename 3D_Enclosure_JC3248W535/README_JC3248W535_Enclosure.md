# ESP32-S3 JC3248W535 Desktop Radio Enclosure — 3D CAD & Virtual Assembly

Precision, 3D-printable retro-modern desktop internet radio enclosure engineered specifically for the **ESP32-S3 JC3248W535** (3.5" IPS 320x480 Capacitive Touch Display module).

---

## 📸 Interactive 3D WebGL CAD & X-Ray Viewer

Open the standalone interactive 3D viewer directly in any web browser:
👉 [`interactive_assembly_viewer.html`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/interactive_assembly_viewer.html)

### Key Interactive Features:
- **⚡ X-Ray View Mode**: Instantly makes the front unibody cabinet and rear plate translucent with edge wireframe highlights so you can inspect internal components, bosses, and screws in place.
- **Explode Slider ($0\% - 100\%$)**: Smooth real-time animation moving the parts along the $Z$ assembly axis.
- **Auto Explode**: Plays an automatic continuous assembly/disassembly cycle.
- **Camera View Presets**: `Front`, `Iso` (Isometric 3D), `Rear`, `Bottom` (inspecting the TF card slot), and `Right` (inspecting the USB Type-C port).
- **Component Visibility Toggles**: Isolate or hide individual parts across all 10 assemblies.
- **Component Inspector**: Click any part to inspect its physical dimensions, material properties, and assembly role.

---

## 🛠️ Key Design & Engineering Features

### 1. Deep Unibody Front Cabinet (39mm Depth)
- The entire main enclosure housing is a monolithic **39.0 mm deep unibody cabinet**:
  - Houses the screen, motherboard, mounting plate, speaker chamber, and battery all inside the front body itself.
  - **100% Solid Merged Corner Bosses**: All 4 corner screw bosses are solid triangular/rectangular prism blocks merged directly into the outer and inner walls of the cabinet, eliminating any thin air gaps for rock-solid 3D printing.
  - **1.5 mm Recessed Corner Landing Shelf ($Z = 37.5\text{ mm}$)**: Corner boss landing faces terminate precisely $1.5\text{ mm}$ below the $39.0\text{ mm}$ top rim, providing exact clearance for the back door's $1.5\text{ mm}$ perimeter indexing lip to enter the case and land flat against the corner boss pads.
  - **Clean, Unobstructed Internal Base**: Zero protruding blocks or saddles at the base of the front case, leaving a clean, open interior cavity for maximum internal routing and component clearance.
  - Active screen aperture: **$73.4\text{ mm} \times 49.0\text{ mm}$** with an elegant $45^\circ$ bevel chamfer.
  - **Zero visible front screws**: The front face is completely sleek, modern, and uncluttered.

### 2. Flat Screw-On Rear Back Door Plate (3mm Thickness)
- The back door is transformed into a **3.0 mm rigid flat cover plate**:
  - Extremely fast and simple to 3D print (flat on bed with zero supports).
  - Features an interlocking **1.5 mm perimeter indexing lip** ($149.2 \times 71.2\text{ mm}$) that fits snugly into the rear opening of the front cabinet and lands directly onto the front case recessed corner landing shelf for a rock-solid, wobble-free seal.
  - 4 flush counterbored M3 screw wells at the corners ($X = \pm 70.0\text{ mm}, Y = \pm 31.5\text{ mm}$).
  - 5 vertical acoustic resonance and ventilation slots behind the speaker chamber.

### 3. Clean Heavy-Duty Board Mounting Clamp Plate
- Upgraded to a **pure 3.2 mm thick solid polymer mounting plate** (battery slot removed):
  - **Clean & Uncompromised**: Battery cradle removed from the clamp plate, maximizing bending stiffness and printability while keeping the battery securely cradled in the front cabinet saddles.
  - **Identical Rectangular Wings**: Left wing and right wing are identical solid rectangular columns ($21\text{ mm}$ wide each), giving immense structural rigidity.
  - **Extra-Wide 72mm Bottom Opening**: Spans $lx = -36.0$ to $+36.0\text{ mm}$ so all 3 bottom JST connectors, the 4P header, the BOOT and RESET tactile switches, and the MicroSD card socket are completely visible and accessible with zero wire pinching.
  - **Dedicated Windows**: Clears USB-C port, top battery header, power switch, and central heatsink.

### 4. Dual-Stage Non-Clustered Screw Fastening System
- Completely separates internal motherboard screws from enclosure closure screws:
  1. **Stage 1 (Bracket-to-Board)**: 4x M2.5/M3 $\times 8\text{ mm}$ screws pass through counterbored inner holes into the module's 4 corner standoffs ($X = -14.0 \pm 42.15\text{ mm}, Y = \pm 26.15\text{ mm}$).
  2. **Stage 2 (Bracket-to-Body)**: 4x M3 $\times 10\text{ mm}$ screws pass through counterbored outer tabs into the front case chassis bosses on the side flanks ($X = -65.0\text{ mm}$ and $+37.0\text{ mm}, Y = \pm 12.0\text{ mm}$).
  3. **Stage 3 (Rear Closure)**: 4x M3 $\times 18\text{ mm}$ screws close the rear plate into the 4 full-depth corner bosses of the front cabinet ($X = \pm 70.0\text{ mm}, Y = \pm 31.5\text{ mm}$).
  - **Clearance**: All three screw groups are separated by $>18\text{ mm}$ to $22\text{ mm}$ of clear space, with zero clustering!

### 5. Dedicated Bottom Opening for MicroSD (TF) Card
- A precision slot ($14.0\text{ mm} \times 3.2\text{ mm}$) molded into the **bottom wall** of the cabinet directly aligned with the board's bottom horizontal TF card socket ($X = -14.0\text{ mm}, Y = -38.0\text{ mm}$).
- Includes an external chamfered guide funnel so cards can be inserted easily with your finger and ejected using the push-push spring mechanism without opening the cabinet.

### 6. Rear USB Type-C Cable Pass-Through & Board Clamp Window
- The **USB Type-C port is located on the REAR of the board** (oriented vertically at global $X = +21.86\text{ mm}, Y = +2.48\text{ mm}$):
  - **Board Mounting Clamp Window**: Features a large **$22.0 \times 32.0\text{ mm}$ window** (centered at $X = +21.0\text{ mm}, Y = -2.0\text{ mm}$) that completely exposes both the rear-facing Type-C port and the adjacent JST 1.25 4P power header with $>5.5\text{ mm}$ clearance on all sides ($0.0000\text{ mm}^3$ clamp overlap).
  - **Rear Back Door Pass-Through Port**: Features a matching vertical cable port ($9.0 \times 15.0\text{ mm}$) with an external funnel chamfer ($12.0 \times 18.0\text{ mm}$) directly aligned with the Type-C port, allowing standard USB-C cables to plug directly into the radio from behind without removing the back cover.
  - **Clean Front Cabinet Right Flank**: The false side-wall cutout has been removed, ensuring the right side of the cabinet is solid and seamless.

### 7. 2040 Loudspeaker Chamber & Acoustic Grille
- Dedicated front acoustic cavity on the right side of the cabinet ($X = +52.0\text{ mm}$).
- Front face features 7 vertical acoustic sound slots ($2.0\text{ mm} \times 28.0\text{ mm}$, pitch $3.2\text{ mm}$) for rich, forward-radiating audio.
- Rear plate features 5 matching acoustic resonance slots.

---

## 📐 Exact Physical Dimensions

| Component / Feature | Dimensions ($X \times Y \times Z$ in mm) | Notes |
| :--- | :--- | :--- |
| **Enclosure Outer Body** | $154.0 \times 76.0 \times 42.0\text{ mm}$ | Corner radius $R = 6.0\text{ mm}$, Wall thickness $= 2.2\text{ mm}$ |
| **Front Unibody Cabinet** | $154.0 \times 76.0 \times 39.0\text{ mm}$ | Deep unibody with solid merged corner bosses, clean interior base |
| **Rear Back Door Plate** | $154.0 \times 76.0 \times 3.0\text{ mm}$ | Flat screw-on plate with 1.5mm perimeter indexing lip & rear USB port |
| **Corner Boss Landing Shelf** | $Z = 37.5\text{ mm}$ ($1.5\text{ mm}$ recess) | Provides exact clearance for back door 1.5mm indexing lip |
| **Active Screen Aperture** | $73.4 \times 49.0\text{ mm}$ | Centered at $X = -14.0\text{ mm}, Y = 0.0\text{ mm}$ with $45^\circ$ bevel |
| **Glass Nest Pocket** | $95.5 \times 63.0 \times 1.8\text{ mm}$ | Smooth fit for $94.5 \times 62.0 \times 1.6\text{ mm}$ outer glass |
| **Board Mounting Plate** | $112.0 \times 62.0 \times 3.2\text{ mm}$ | Pure solid 3.2mm plate, 22x32mm USB window, 72mm bottom opening |
| **Bottom MicroSD Slot** | $14.0 \times 3.2\text{ mm}$ (Bottom wall) | External chamfered funnel for push-push ejection |
| **Rear USB Type-C Port** | $9.0 \times 15.0\text{ mm}$ (Back plate) | Aligned with board's rear-facing Type-C port |
| **2040 Speaker Cavity** | $20.8 \times 40.8 \times 7.5\text{ mm}$ | Centered at $X = +52.0\text{ mm}, Y = 0.0\text{ mm}$ |
| **Front Acoustic Slots** | 7 slots, each $2.0 \times 28.0\text{ mm}$ | Spaced at $3.2\text{ mm}$ pitch |
| **Rear Acoustic Slots** | 5 slots, each $2.4 \times 26.0\text{ mm}$ | Spaced at $4.5\text{ mm}$ pitch |
| **Corner Closure Screws** | $X = \pm 70.0\text{ mm}, Y = \pm 31.5\text{ mm}$ | 4x M3 counterbored holes through back plate |

---

## 📦 Watertight STL Files Catalog

All STL files are located in `d:\ESP32Radio\3D_Enclosure_JC3248W535\`:

| File Name | Vertices | Faces | Watertight | Description |
| :--- | :--- | :--- | :--- | :--- |
| [`JC3248W535_Radio_Front_Case.stl`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/JC3248W535_Radio_Front_Case.stl) | 2,035 | 4,102 | **Yes** | Deep unibody chassis (39mm), solid merged corner bosses, recessed shelf, clean base |
| [`JC3248W535_Radio_Board_Clamp.stl`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/JC3248W535_Radio_Board_Clamp.stl) | 1,076 | 2,184 | **Yes** | Clean 3.2mm mounting plate (22x32mm rear USB window, 72mm bottom opening) |
| [`JC3248W535_Radio_Back_Cover.stl`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/JC3248W535_Radio_Back_Cover.stl) | 1,115 | 2,266 | **Yes** | Flat 3.0mm rear back door plate with rear USB-C port, indexing lip & sound vents |
| [`JC3248W535_Radio_All_Parts.stl`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/JC3248W535_Radio_All_Parts.stl) | 4,226 | 8,552 | **Yes** | Single print-bed plate layout containing Front Case, Clamp, and Back Plate |
| [`JC3248W535_Radio_Virtual_Assembly_Mated.stl`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/JC3248W535_Radio_Virtual_Assembly_Mated.stl) | 7,467 | 14,950 | **Yes** | Full mated 3D assembly CAD with board, plate, battery, and screws |
| [`JC3248W535_Radio_Virtual_Assembly_Exploded.stl`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535/JC3248W535_Radio_Virtual_Assembly_Exploded.stl) | 8,023 | 16,090 | **Yes** | Exploded mechanical stackup model |

---

## 🖨️ 3D Printing Guidelines

- **Material**: PLA, PETG, or ABS/ASA (matte black, dark charcoal, or retro cream/wood filament).
- **Print Bed Orientation**:
  - **Front Case**: Place flat on print bed with front face DOWN ($Z = 0$). Zero supports required.
  - **Board Clamp Frame**: Place flat on print bed ($Z = 0$). Zero supports required.
  - **Rear Back Cover**: Place flat on print bed with rear exterior face DOWN ($Z = 0$). Zero supports required.
- **Layer Height**: $0.20\text{ mm}$ (Standard) or $0.16\text{ mm}$ (Fine for smooth beveled screen aperture).
- **Perimeters / Walls**: 3 to 4 perimeters ($1.2 - 1.6\text{ mm}$ wall thickness) for rigid screw bosses.
- **Infill**: $20\% - 25\%$ Gyroid or Grid infill.

---

## 🔩 Assembly Instructions

1. **Insert the Display Module**:
   - Place the 3D-printed Front Case face-down on a soft cloth or mousepad.
   - Drop the ESP32-S3 JC3248W535 module into the front case from the **inside**. The front glass will seat flush into the recessed nest with the active screen aligned with the front bezel aperture.
2. **Install the 2040 Loudspeaker**:
   - Press the 2040 speaker into the acoustic chamber on the right side of the front case. Plug the speaker 2-pin JST lead into the board's `Speak` header.
3. **Fasten the Board Clamp Frame**:
   - Place the 3D-printed Board Clamp Frame over the back of the module.
   - Fasten with 4x M3 $\times 8\text{ mm}$ pan head screws into the internal chassis bosses. Tighten evenly until snug. The module is now locked securely and vibration-proof!
4. **Insert MicroSD Card (Optional)**:
   - Slide your MicroSD card into the bottom slot until it clicks into the board's spring-loaded socket.
5. **Connect Battery (Optional)**:
   - Place an 18650 cell holder or LiPo pouch battery into the back cover cradle and plug into the top `Battery` JST header.
6. **Close Rear Back Cover**:
   - Align the rear cover's interlocking tongue with the front case rim.
   - Insert 4x M3 $\times 16\text{ mm}$ cap head screws into the 4 corner counterbored wells and tighten until flush.
