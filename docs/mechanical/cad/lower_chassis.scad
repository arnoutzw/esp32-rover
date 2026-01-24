// ============================================
// ESP32 Rover - Lower Chassis Plate
// ============================================
// Design Specification v1.0
//
// This is the lower deck of the sandwich-plate
// chassis design. Houses battery and drivetrain.
// ============================================

// Main dimensions
chassis_length = 160;
chassis_width = 80;
chassis_thickness = 3;

// Battery dimensions (2× 18650 side-by-side)
battery_length = 67;
battery_width = 40;  // 2 × 18mm + 4mm clearance
battery_depth = 20;
battery_offset_x = 10;  // From front

// Motor mount (A2212)
motor_mount_x = chassis_length - 25;
motor_center_hole = 8;
motor_hole_spacing_1 = 16;  // First pattern
motor_hole_spacing_2 = 19;  // Second pattern

// Differential mount area
diff_width = 50;
diff_length = 30;
diff_offset_x = chassis_length - 45;

// Mounting holes
mount_hole_dia = 3.2;  // M3 clearance
mount_edge_offset = 8;

// Resolution
$fn = 32;

module lower_chassis() {
    difference() {
        union() {
            // Main plate
            cube([chassis_length, chassis_width, chassis_thickness]);

            // Battery retention walls
            translate([battery_offset_x - 2, (chassis_width - battery_width) / 2 - 2, 0])
                cube([2, battery_width + 4, chassis_thickness + 5]);
            translate([battery_offset_x + battery_length, (chassis_width - battery_width) / 2 - 2, 0])
                cube([2, battery_width + 4, chassis_thickness + 5]);
        }

        // Battery cutout
        translate([battery_offset_x, (chassis_width - battery_width) / 2, -1])
            cube([battery_length, battery_width, chassis_thickness + 2]);

        // Motor mount center hole
        translate([motor_mount_x, chassis_width / 2, -1])
            cylinder(d = motor_center_hole, h = chassis_thickness + 2);

        // Motor mount screw holes - Pattern 1 (16mm)
        for (angle = [0, 90, 180, 270]) {
            translate([motor_mount_x, chassis_width / 2, 0])
                rotate([0, 0, angle + 45])
                    translate([motor_hole_spacing_1 / 2, 0, -1])
                        cylinder(d = mount_hole_dia, h = chassis_thickness + 2, $fn = 16);
        }

        // Motor mount screw holes - Pattern 2 (19mm) - slotted for adjustment
        for (angle = [0, 180]) {
            translate([motor_mount_x, chassis_width / 2, 0])
                rotate([0, 0, angle])
                    translate([motor_hole_spacing_2 / 2, 0, -1])
                        hull() {
                            cylinder(d = mount_hole_dia, h = chassis_thickness + 2, $fn = 16);
                            translate([-3, 0, 0])
                                cylinder(d = mount_hole_dia, h = chassis_thickness + 2, $fn = 16);
                        }
        }

        // Deck mounting holes - corners
        for (x = [mount_edge_offset, chassis_length - mount_edge_offset]) {
            for (y = [mount_edge_offset, chassis_width - mount_edge_offset]) {
                translate([x, y, -1])
                    cylinder(d = mount_hole_dia, h = chassis_thickness + 2, $fn = 16);
            }
        }

        // Deck mounting holes - center edges
        for (x = [chassis_length / 2]) {
            for (y = [mount_edge_offset, chassis_width - mount_edge_offset]) {
                translate([x, y, -1])
                    cylinder(d = mount_hole_dia, h = chassis_thickness + 2, $fn = 16);
            }
        }

        // Differential mounting slots
        translate([diff_offset_x, (chassis_width - diff_width) / 2, -1])
            cube([diff_length, diff_width, chassis_thickness + 2]);

        // Weight reduction cutouts
        translate([battery_offset_x + battery_length + 10, 10, -1])
            rounded_rect(30, 20, chassis_thickness + 2, 3);
        translate([battery_offset_x + battery_length + 10, chassis_width - 30, -1])
            rounded_rect(30, 20, chassis_thickness + 2, 3);

        // Front steering mount holes
        for (y = [15, chassis_width - 15]) {
            translate([15, y, -1])
                cylinder(d = mount_hole_dia, h = chassis_thickness + 2, $fn = 16);
        }
    }
}

module rounded_rect(w, h, d, r) {
    hull() {
        translate([r, r, 0]) cylinder(r = r, h = d, $fn = 16);
        translate([w - r, r, 0]) cylinder(r = r, h = d, $fn = 16);
        translate([r, h - r, 0]) cylinder(r = r, h = d, $fn = 16);
        translate([w - r, h - r, 0]) cylinder(r = r, h = d, $fn = 16);
    }
}

// Render the part
lower_chassis();
