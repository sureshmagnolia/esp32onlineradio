; ========================================================
; Creality Falcon 5W Laser G-Code
; Cutout Profile: Flush Rear Body Cutout (85 x 56 mm)
; Cut Dimensions: 85.0 x 56.0 mm (R=2.5mm)
; Speed: 400 mm/min | Laser Power: 90% (S900) | Passes: 4
; Origin: (0,0) at Top-Left Corner of Door Plate (153 x 90 mm)
; ========================================================
G21          ; Set units to millimeters
G90          ; Absolute positioning
M5           ; Ensure laser is OFF
G0 Z0 F1000  ; Home/Neutral Z

; --- PHASE 1: FRAMING PASS (Visual alignment at 0.5% power) ---
; The laser dot will illuminate the boundary box so you can verify positioning
G0 X55.0 Y17.5 F1200
M3 S5        ; Low power pilot beam (0.5%)
G1 X140.0 Y17.5 F1200
G1 X140.0 Y73.5 F1200
G1 X55.0 Y73.5 F1200
G1 X55.0 Y17.5 F1200
M5           ; Laser OFF after framing
G4 P1.0      ; Pause 1 second for visual check

; --- PHASE 2: CUTTING PASSES ---
; --- Pass 1 of 4 ---
G0 X57.500 Y17.500 F400
M3 S900   ; Laser ON at 90% power
G1 X137.500 Y17.500 F400
G2 X140.000 Y20.000 I0.0 J2.500
G1 X140.000 Y71.000
G2 X137.500 Y73.500 I-2.500 J0.0
G1 X57.500 Y73.500
G2 X55.000 Y71.000 I0.0 J-2.500
G1 X55.000 Y20.000
G2 X57.500 Y17.500 I2.500 J0.0
M5           ; Laser OFF at end of pass
G4 P0.2      ; Short cooldown pause (0.2s)

; --- Pass 2 of 4 ---
G0 X57.500 Y17.500 F400
M3 S900   ; Laser ON at 90% power
G1 X137.500 Y17.500 F400
G2 X140.000 Y20.000 I0.0 J2.500
G1 X140.000 Y71.000
G2 X137.500 Y73.500 I-2.500 J0.0
G1 X57.500 Y73.500
G2 X55.000 Y71.000 I0.0 J-2.500
G1 X55.000 Y20.000
G2 X57.500 Y17.500 I2.500 J0.0
M5           ; Laser OFF at end of pass
G4 P0.2      ; Short cooldown pause (0.2s)

; --- Pass 3 of 4 ---
G0 X57.500 Y17.500 F400
M3 S900   ; Laser ON at 90% power
G1 X137.500 Y17.500 F400
G2 X140.000 Y20.000 I0.0 J2.500
G1 X140.000 Y71.000
G2 X137.500 Y73.500 I-2.500 J0.0
G1 X57.500 Y73.500
G2 X55.000 Y71.000 I0.0 J-2.500
G1 X55.000 Y20.000
G2 X57.500 Y17.500 I2.500 J0.0
M5           ; Laser OFF at end of pass
G4 P0.2      ; Short cooldown pause (0.2s)

; --- Pass 4 of 4 ---
G0 X57.500 Y17.500 F400
M3 S900   ; Laser ON at 90% power
G1 X137.500 Y17.500 F400
G2 X140.000 Y20.000 I0.0 J2.500
G1 X140.000 Y71.000
G2 X137.500 Y73.500 I-2.500 J0.0
G1 X57.500 Y73.500
G2 X55.000 Y71.000 I0.0 J-2.500
G1 X55.000 Y20.000
G2 X57.500 Y17.500 I2.500 J0.0
M5           ; Laser OFF at end of pass
G4 P0.2      ; Short cooldown pause (0.2s)

; --- FINISH SEQUENCE ---
M5           ; Ensure laser is OFF
G0 X0 Y0 F2000 ; Return to origin (0,0)
M2           ; Program end
