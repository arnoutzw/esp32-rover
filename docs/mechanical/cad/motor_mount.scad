// ============================================
// ESP32 Rover - Motor Mount (Sliding)
// ============================================
// Design Specification v1.0
//
// Sliding motor mount for A2212 BLDC motor.
// Allows gear mesh adjustment.
// ============================================

// A2212 Motor dimensions
motor_diameter = 28;
motor_body_height = 15;
shaft_diameter = 3.17;
shaft_clearance = 4;
mount_hole_spacing = 16;  // M3 pattern

// Mount dimensions
mount_length = 45;
mount_width = 35;
mount_thickness = 5;
mount_wall = 3;

// Slot for adjustment
slot_length = 8;  // 4mm adjustment each direction
slot_width = 3.5;  // M3 clearance

// Resolution
$fn = 32;

module motor_mount() {
    difference() {
        union() {
            // Base plate
            cube([mount_length, mount_width, mount_thickness]);

            // Motor cradle walls
            translate([mount_length / 2 - motor_diameter / 2 - mount_wall, 0, mount_thickness])
                cube([mount_wall, mount_width, motor_body_height]);
            translate([mount_length / 2 + motor_diameter / 2, 0, mount_thickness])
                cube([mount_wall, mount_width, motor_body_height]);
        }

        // Motor body cutout (semicircle)
        translate([mount_length / 2, mount_width / 2, mount_thickness])
            cylinder(d = motor_diameter + 0.5, h = motor_body_height + 1);

        // Shaft clearance
        translate([mount_length / 2, mount_width / 2, -1])
            cylinder(d = shaft_clearance, h = mount_thickness + 2);

        // Motor mounting holes (M3)
        for (angle = [0, 90, 180, 270]) {
            translate([mount_length / 2, mount_width / 2, 0])
                rotate([0, 0, angle + 45])
                    translate([mount_hole_spacing / 2, 0, -1])
                        cylinder(d = 3.2, h = mount_thickness + motor_body_height + 2, $fn = 16);
        }

        // Adjustment slots (for chassis mounting)
        for (y = [8, mount_width - 8]) {
            translate([10, y, -1])
                hull() {
                    cylinder(d = slot_width, h = mount_thickness + 2, $fn = 16);
                    translate([slot_length, 0, 0])
                        cylinder(d = slot_width, h = mount_thickness + 2, $fn = 16);
                }
            translate([mount_length - 10 - slot_length, y, -1])
                hull() {
                    cylinder(d = slot_width, h = mount_thickness + 2, $fn = 16);
                    translate([slot_length, 0, 0])
                        cylinder(d = slot_width, h = mount_thickness + 2, $fn = 16);
                }
        }

        // Ventilation slots
        for (i = [0:2]) {
            translate([mount_length / 2 - 8 + i * 8, -1, mount_thickness + 5])
                cube([3, mount_width + 2, 8]);
        }
    }
}

// Render the part
motor_mount();
