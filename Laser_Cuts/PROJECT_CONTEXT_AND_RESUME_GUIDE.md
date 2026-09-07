# Laser Cutting Project Context & Resumption Guide
**Creality Falcon 5W Laser Engraver / Cutter — ESP32-S3 JC3248W535 Back Door Cutout**

> **Directory**: `d:\ESP32Radio\Laser_Cuts\`  
> **Last Updated**: 2026-09-06  
> **Machine**: Creality Falcon 5W (72W total, 5W optical diode, 0.06mm spot size, 400×415mm working area)  
> **Target Workpiece**: Gold/Bronze injection-molded speaker back door plate ($153.0 \times 90.0\text{ mm}$)  
> **Component to Mount**: ESP32-S3 JC3248W535 (3.5" Capacitive Touch Display Module)

---

## 1. Quick Resumption Prompt (When Returning Later)
Whenever you start a new conversation and want to continue working on this laser cutting task, simply tell the assistant:
> *"I want to continue with the laser cutting project in `d:\ESP32Radio\Laser_Cuts\`. Please review `PROJECT_CONTEXT_AND_RESUME_GUIDE.md` and `CUTTING_JOB_SPEC.json`."*

The assistant will immediately have 100% of the context, coordinates, toolpaths, and machine profiles loaded.

---

## 2. Complete File Directory (`d:\ESP32Radio\Laser_Cuts\`)

### Core Cutting Files:
| File Name | Format | Description |
| :--- | :--- | :--- |
| [`CrealityFalcon_DisplayCutout_full_glass_96x63.dxf`](file:///d:/ESP32Radio/Laser_Cuts/CrealityFalcon_DisplayCutout_full_glass_96x63.dxf) | DXF | **Option 1 (Full Glass Cutout)**: $96.0 \times 63.0\text{ mm}$ ($R=3\text{ mm}$). Drop-in opening for entire display module. $4.0\text{ mm}$ space above bottom screw bosses. |
| [`CrealityFalcon_DisplayCutout_full_glass_96x63.svg`](file:///d:/ESP32Radio/Laser_Cuts/CrealityFalcon_DisplayCutout_full_glass_96x63.svg) | SVG | Vector file for LightBurn and LaserGRBL (1:1 scale mm). |
| [`CrealityFalcon_DisplayCutout_full_glass_96x63.gcode`](file:///d:/ESP32Radio/Laser_Cuts/CrealityFalcon_DisplayCutout_full_glass_96x63.gcode) | G-Code | Ready-to-run G-code for offline TF/MicroSD card on Falcon 5W ($400\text{ mm/min}$, $90\%$ power, 4 passes, framing pass included). |
| [`CrealityFalcon_DisplayCutout_flush_body_85x56.dxf`](file:///d:/ESP32Radio/Laser_Cuts/CrealityFalcon_DisplayCutout_flush_body_85x56.dxf) | DXF | **Option 2 (Flush Body Cutout)**: $85.0 \times 56.0\text{ mm}$ ($R=2.5\text{ mm}$). Rear housing body passes through; front glass rests on door surface. Preserves top printed labels. |
| [`CrealityFalcon_DisplayCutout_flush_body_85x56.svg`](file:///d:/ESP32Radio/Laser_Cuts/CrealityFalcon_DisplayCutout_flush_body_85x56.svg) | SVG | Option 2 vector file for LightBurn / LaserGRBL. |
| [`CrealityFalcon_DisplayCutout_flush_body_85x56.gcode`](file:///d:/ESP32Radio/Laser_Cuts/CrealityFalcon_DisplayCutout_flush_body_85x56.gcode) | G-Code | Option 2 ready-to-run G-code for Falcon 5W. |
| [`CrealityFalcon_DisplayCutout_screen_window_75x51.dxf`](file:///d:/ESP32Radio/Laser_Cuts/CrealityFalcon_DisplayCutout_screen_window_75x51.dxf) | DXF | **Option 3 (Active Screen Window + 4 Screws)**: $75.0 \times 51.0\text{ mm}$ viewing aperture + 4 pilot holes ($\varnothing 2.6\text{ mm}$ at $84.3 \times 52.3\text{ mm}$ pitch). Board mounts to inside door face. |
| [`CrealityFalcon_DisplayCutout_screen_window_75x51.svg`](file:///d:/ESP32Radio/Laser_Cuts/CrealityFalcon_DisplayCutout_screen_window_75x51.svg) | SVG | Option 3 vector file for LightBurn / LaserGRBL. |
| [`CrealityFalcon_DisplayCutout_screen_window_75x51.gcode`](file:///d:/ESP32Radio/Laser_Cuts/CrealityFalcon_DisplayCutout_screen_window_75x51.gcode) | G-Code | Option 3 ready-to-run G-code for Falcon 5W. |
| [`CrealityFalcon_SpeakerDoor_Master.svg`](file:///d:/ESP32Radio/Laser_Cuts/CrealityFalcon_SpeakerDoor_Master.svg) | SVG | Master multi-layer SVG with door contour, reference air holes, screw bosses, and cut outlines. |

### Verification, Test, and Interactive Tools:
| File Name | Format | Description |
| :--- | :--- | :--- |
| [`CrealityFalcon_SpeakerDoor_Template_1to1.pdf`](file:///d:/ESP32Radio/Laser_Cuts/CrealityFalcon_SpeakerDoor_Template_1to1.pdf) | PDF | **True 1:1 Scale Printable Template**: Print on standard A4 paper at 100% scale. Features a $100\text{ mm}$ calibration ruler, outer door boundary, screw bosses, and cutout options. |
| [`laser_cutter_door_alignment.html`](file:///d:/ESP32Radio/Laser_Cuts/laser_cutter_door_alignment.html) | HTML | **Interactive Web Visualizer**: Drag live X/Y offset sliders over the actual door photograph, toggle options, and check real-time clearances. |
| [`CUTTING_JOB_SPEC.json`](file:///d:/ESP32Radio/Laser_Cuts/CUTTING_JOB_SPEC.json) | JSON | Machine-readable project specification with all coordinates, dimensions, clearances, and G-code parameters. |
| [`README_Laser_Cutting_Creality_Falcon_5W.md`](file:///d:/ESP32Radio/Laser_Cuts/README_Laser_Cutting_Creality_Falcon_5W.md) | Markdown | Comprehensive operator manual for the Falcon 5W. |
| [`speaker_backdoor_photo.png`](file:///d:/ESP32Radio/Laser_Cuts/speaker_backdoor_photo.png) | PNG | Original high-resolution photo of the speaker back door. |
| [`laser_cutout_exact_overlay.png`](file:///d:/ESP32Radio/Laser_Cuts/laser_cutout_exact_overlay.png) | PNG | Annotated overlay image showing the cutout placed on the door with exact clearance callouts. |
| [`template_pdf_preview.png`](file:///d:/ESP32Radio/Laser_Cuts/template_pdf_preview.png) | PNG | Visual preview image of the printable 1:1 PDF template. |
| [`generate_laser_files.py`](file:///d:/ESP32Radio/Laser_Cuts/generate_laser_files.py) | Python | Generator script that recalculates all DXF, SVG, G-code, and PDF files. |
| [`generate_overlay.py`](file:///d:/ESP32Radio/Laser_Cuts/generate_overlay.py) | Python | Script that regenerates the visual overlay inspection image. |

---

## 3. Geometry & Coordinate System Summary

* **Origin $(0, 0)$**: Top-Left outer corner of the door ($153.0 \times 90.0\text{ mm}$).
* **X-Axis**: From left ($0.0\text{ mm}$) to right ($153.0\text{ mm}$).
* **Y-Axis**: From top ($0.0\text{ mm}$) to bottom ($90.0\text{ mm}$).
* **Keep-Out Zones**:
  * **Top Controls Safe Zone**: $Y = 0.0\text{ to } 16.0\text{ mm}$ (`LINE IN`, `USB`, `DC-5V`, `TF` ports and printed text).
  * **Bottom Screw Bosses**:
    * Left boss: $(X = 53.7, Y = 81.3\text{ mm})$, top edge of boss at $Y = 77.3\text{ mm}$.
    * Middle boss: $(X = 98.6, Y = 82.5\text{ mm})$, top edge of boss at $Y = 78.5\text{ mm}$.
    * Right boss: $(X = 144.7, Y = 84.4\text{ mm})$, top edge of boss at $Y = 80.4\text{ mm}$.
  * **Cutout Bottom Edge**: Set at **$Y = 74.5\text{ mm}$**, leaving **$4.0\text{ mm}$ of solid injection-molded plastic** directly above the middle screw boss.

---

## 4. Cutout Options Comparison

```
+---------------------------------------------------------------------------------------------------+
|  [OFF/ON]  [LINE IN]       [USB]    [DC-5V]    [TF]                          (Y=0..16mm Safe Zone)|
|                                                                                                   |
|  +---------------------+        +--------------------------------------------------------+        |
|  |                     |        |                                                        |        |
|  |                     |        |   OPTION 1: Full Glass Cutout (96 x 63 mm)             |        |
|  |    SPEAKER AREA     |        |   X = [47.0 .. 143.0 mm], Y = [11.5 .. 74.5 mm]        |        |
|  |   (Solid Plastic)   |        |   Covers air vents, 4.0mm clearance above screw boss   |        |
|  |                     |        |                                                        |        |
|  +---------------------+        +--------------------------------------------------------+        |
|                                                                                                   |
|           (O) Left Boss                (O) Middle Boss                      (O) Right Boss        |
|         X=53.7, Y=81.3               X=98.6, Y=82.5                       X=144.7, Y=84.4         |
+---------------------------------------------------------------------------------------------------+
```

1. **Option 1 (Full Glass: $96.0 \times 63.0\text{ mm}$, $R=3\text{ mm}$)**:
   * Recommended if inserting the whole display module from the front.
   * $4.0\text{ mm}$ clearance above the bottom screw bosses.
2. **Option 2 (Flush Body: $85.0 \times 56.0\text{ mm}$, $R=2.5\text{ mm}$)**:
   * Recommended if the black rear housing body passes through while the front glass rests on the door face.
   * $5.0\text{ mm}$ clearance above screw bosses, completely preserves top text labels.
3. **Option 3 (Screen Window: $75.0 \times 51.0\text{ mm}$ + 4 Screws)**:
   * Recommended for internal mounting against the inside door face.
   * Includes 4 pilot holes ($\varnothing 2.6\text{ mm}$ at $84.3 \times 52.3\text{ mm}$ pitch).

---

## 5. Falcon 5W Machine Checklist When Cutting

1. **Paper Verification**: Print [`CrealityFalcon_SpeakerDoor_Template_1to1.pdf`](file:///d:/ESP32Radio/Laser_Cuts/CrealityFalcon_SpeakerDoor_Template_1to1.pdf) at 100% scale and test-fit over the physical door.
2. **Focus Setting**: Use the Creality multi-step aluminum block between the nozzle tip and the door face.
3. **Origin Alignment**: Jog the laser dot directly over the **top-left outer corner** of the door plate.
4. **Framing Pass**: Run the framing pass ($0.5\%$ power pilot dot at $1200\text{ mm/min}$) to verify the boundary box stays $4.0\text{ mm}$ above the screw bosses.
5. **Cutting Parameters**: $400\text{ mm/min}$, $90\%$ power ($S900$), $3 - 4$ passes.
6. **Air Flow**: Ensure a continuous gentle air stream at the cut line to prevent melted plastic ridges.
