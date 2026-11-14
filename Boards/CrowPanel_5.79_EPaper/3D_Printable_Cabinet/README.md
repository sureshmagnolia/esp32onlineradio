# Elecrow CrowPanel 5.79" E-Paper Smart Clock & Audio Cabinet
### Custom 3D Printable Enclosure with Heavy-Duty Snap-Fit Bracket, Single Corner Bosses & Dual 18650 Battery Bay

This enclosure transforms the **Elecrow CrowPanel 5.79" ESP32-S3 E-Paper HMI Display** (`272×792` resolution) into a dedicated **Smart Wall Clock, Internet Radio, and Music Player**.

---

## 1. Resolved Mechanical Refinements

Based on slicer inspection and user feedback, the enclosure incorporates 4 major architectural upgrades:

1. **Single Corner Screw Boss (Clean Corners)**:
   - The redundant double-boss "peanut" holes in the front bezel have been **100% eliminated**.
   - Each corner now features **exactly ONE single, clean cylindrical screw boss** ($X = \pm 82.0\text{ mm}, Y = \pm 26.75\text{ mm}$) for securing the rear cabinet with an M2.5 or M3 screw.
2. **Spring-Mode Compliant Bracket (`CrowPanel_5.79_Mounting_Bracket.stl`)**:
   - **Compliant Cantilever Spring Arms**: Replaces rigid, breakable snap tabs with flexible $1.8\text{ mm}$ thick leaf-spring beams isolated by $22.0\text{ mm} \times 2.0\text{ mm}$ flexure relief slots.
   - **Pinch-to-Release Ergonomics**: Raised grip pads on top and bottom spring arms allow you to squeeze the tabs inward with thumb and forefinger, instantly retracting the teeth from the detents to take the bracket out effortlessly without tools or prying!
   - **Solid 4.0mm Base Plate** + **2.5mm Perimeter Box-Truss Flange** (total $6.5\text{ mm}$ height, Volume: $31,515\text{ mm}^3$): Completely rigid against bending or cracking.
   - **Finger Lift Extraction Notches**: Recessed pry notches on left and right ends for easy fingertip removal.
   - **Tool-Less Quick Swapping**: Board and bracket press in and click with a tactile spring action, and pinch to remove anytime for maintenance!
3. **Keyholes Moved Away from Buttons (>10mm Clearance)**:
   - Symmetrical wall keyholes are positioned at **$X = -65.0\text{ mm}$ and $X = +68.0\text{ mm}$, $Y = +14.0\text{ mm}$**.
   - The right keyhole is separated from the Reset button ($X = +54.18\text{ mm}, Y = +23.27\text{ mm}$) by **$16.64\text{ mm}$ center-to-center**, leaving over **$10.2\text{ mm}$ of solid wall** between hole edges. Zero collision!
4. **Shallow Speaker Retention Rim (No Tall Cup)**:
   - The tall 17mm carrier cup has been eliminated.
   - Replaced with a simple **$2.5\text{ mm}$ shallow rim** on the back door (inner dia $40.5\text{ mm}$). The 40mm speaker drops in against the rear grille and is secured flat with double-sided foam tape or adhesive.
5. **Deepened Rear Cabinet & Expanded Battery Bay (Ample Headroom & Zero Projection)**:
   - Rear cabinet outer depth increased from $22.0\text{ mm}$ to **$28.0\text{ mm}$**, expanding the internal cavity to **$25.8\text{ mm}$ deep** ($+30\%$ more cavity volume).
   - Dual 18650 cradle expanded to **$72.0\text{ mm}$ length $\times 40.0\text{ mm}$ width $\times 9.0\text{ mm}$ depth** with curved saddles, wire routing notch, and **$7.3\text{ mm}$ of clear headroom** above standard $18.5\text{ mm}$ cells!
   - Supports bare 18650 cells, protected cells (67-69mm), or commercial dual-18650 battery sleds with zero risk of touching the board or bulging outside.
   - Internal 10mm tubular guide bosses ensure rigid, wobble-free action for the extended $25.2\text{ mm}$ Boot & Reset button plungers.

---

## 2. 3D Printable STL Files (All 100% Watertight)

| File | Size | Dimensions | Description |
| :--- | :--- | :--- | :--- |
| **[`CrowPanel_5.79_Front_Bezel.stl`](file:///d:/ESP32Radio/Boards/CrowPanel_5.79_EPaper/3D_Printable_Cabinet/CrowPanel_5.79_Front_Bezel.stl)** | 71 KB | $173.0 \times 62.5 \times 14.0\text{ mm}$ | Slim front frame with 140.5×48.5mm beveled window, side dial slot, bottom USB-C/SD slots, and **SINGLE boss per corner**. |
| **[`CrowPanel_5.79_Mounting_Bracket.stl`](file:///d:/ESP32Radio/Boards/CrowPanel_5.79_EPaper/3D_Printable_Cabinet/CrowPanel_5.79_Mounting_Bracket.stl)** | 49 KB | $167.0 \times 56.5 \times 6.5\text{ mm}$ | **Spring-mode quick-release bracket**: 4 compliant cantilever spring arms + pinch pads + 22mm flex slots + end lift notches. |
| **[`CrowPanel_5.79_Rear_Cabinet.stl`](file:///d:/ESP32Radio/Boards/CrowPanel_5.79_EPaper/3D_Printable_Cabinet/CrowPanel_5.79_Rear_Cabinet.stl)** | 446 KB | $173.0 \times 62.5 \times 28.0\text{ mm}$ | **Deepened back box (25.8mm cavity)**: 72×40mm battery cradle with 7.3mm headroom, shallow 2.5mm speaker rim, rigid button guide tubes, non-merging keyholes, and 4 corner screws. |
| **[`CrowPanel_5.79_Button_Caps.stl`](file:///d:/ESP32Radio/Boards/CrowPanel_5.79_EPaper/3D_Printable_Cabinet/CrowPanel_5.79_Button_Caps.stl)** | 45 KB | $18.8 \times 6.8 \times 25.2\text{ mm}$ | 2× Extended captive push plungers with dia 3.6mm stiff guide stems and retaining flanges for BOOT and RESET switches. |

---

## 3. Two-Tier Assembly Procedure

```
1. [CrowPanel Display Module] drops into [Front Bezel]
2. [Spring-Mode Quick-Release Bracket]
   ├── 2× M3 × 6mm screws tighten through bracket into board's factory brass inserts
   ├── Bracket & Board assembly presses into Front Bezel until all 4 spring arms "CLICK" into place!
   └── TO REMOVE / TAKE OUT: Simply pinch the top and bottom spring pads inward and lift out!
3. [Rear Cabinet]
   ├── 40mm speaker drops into shallow 2.5mm rim on back door (tape/glue flat)
   ├── 2× 18650 cells seated in expanded 72×40mm right-half cradle (wires routed through top notch)
   ├── 2× Button Plungers inserted through internal guide tubes
   └── 4× M2.5/M3 × 25-30mm corner screws close the back box flush against the front bezel
```
