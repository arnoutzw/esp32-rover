// ============================================
// ESP32 Rover - Wheel Hub
// ============================================
// Design Specification v1.0
//
// Wheel hub adapter for 40mm rubber wheels.
// Print 4× for complete set.
// ============================================

// Axle dimensions
axle_dia = 5;
axle_bore = 5.2;  // Clearance fit

// Hub dimensions
hub_outer_dia = 20;
hub_length = 15;
hub_flange_dia = 25;
hub_flange_thickness = 3;

// Wheel interface
wheel_pattern = "hex";  // "hex" or "spline"
hex_size = 12;  // 12mm hex (common RC standard)

// Grub screw
grub_screw_dia = 3;  // M3
grub_screw_depth = 8;

// Resolution
$fn = 32;

module wheel_hub() {
    difference() {
        union() {
            // Main hub body
            cylinder(d = hub_outer_dia, h = hub_length);

            // Wheel mounting flange
            cylinder(d = hub_flange_dia, h = hub_flange_thickness);

            // Hex pattern for wheel
            translate([0, 0, hub_length - 5])
                cylinder(d = hex_size * 2 / sqrt(3), h = 5, $fn = 6);
        }

        // Axle bore
        translate([0, 0, -1])
            cylinder(d = axle_bore, h = hub_length + 2);

        // Grub screw hole (M3)
        translate([0, hub_outer_dia / 2 + 1, hub_length / 2])
            rotate([90, 0, 0])
                cylinder(d = grub_screw_dia, h = grub_screw_depth + 2, $fn = 16);

        // Grub screw flat (for set screw to grip)
        translate([-1.5, -axle_bore / 2 - 0.3, hub_length / 2 - 3])
            cube([3, 1, 6]);

        // Weight reduction holes in flange
        for (angle = [0:60:300]) {
            rotate([0, 0, angle + 30])
                translate([hub_flange_dia / 2 - 4, 0, -1])
                    cylinder(d = 4, h = hub_flange_thickness + 2, $fn = 16);
        }

        // Wheel bolt holes (M2)
        for (angle = [0:90:270]) {
            rotate([0, 0, angle + 45])
                translate([hub_flange_dia / 2 - 3, 0, -1])
                    cylinder(d = 2.2, h = hub_flange_thickness + 2, $fn = 16);
        }
    }
}

// Render the part
wheel_hub();

// Show axle reference (for visualization)
// %translate([0, 0, -5]) cylinder(d = axle_dia, h = hub_length + 10);
