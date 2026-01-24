// ============================================
// ESP32 Rover - Steering Knuckle
// ============================================
// Design Specification v1.0
//
// Front steering knuckle with Ackerman geometry.
// Print 2× (mirror for left/right)
// ============================================

// Dimensions
knuckle_height = 20;
knuckle_width = 15;
knuckle_depth = 12;

// Kingpin
kingpin_dia = 3.2;  // M3 clearance
kingpin_offset_x = 5;

// Wheel hub
hub_dia = 5.2;  // 5mm axle clearance
hub_length = 10;

// Steering arm
arm_length = 15;
arm_width = 5;
arm_thickness = 4;
arm_angle = 22;  // Ackerman angle (degrees inward)
pushrod_hole = 1.6;  // For 1.5mm wire

// Resolution
$fn = 32;

module steering_knuckle(mirror = false) {
    mirror_factor = mirror ? -1 : 1;

    difference() {
        union() {
            // Main body
            cube([knuckle_width, knuckle_depth, knuckle_height]);

            // Wheel hub boss
            translate([knuckle_width, knuckle_depth / 2, knuckle_height / 2])
                rotate([0, 90, 0])
                    cylinder(d = hub_dia + 4, h = hub_length);

            // Steering arm
            translate([knuckle_width / 2, knuckle_depth, 3])
                rotate([0, 0, mirror_factor * arm_angle])
                    translate([-arm_width / 2, 0, 0])
                        cube([arm_width, arm_length, arm_thickness]);

            // Steering arm reinforcement
            translate([knuckle_width / 2, knuckle_depth, 3])
                rotate([0, 0, mirror_factor * arm_angle])
                    translate([0, 0, 0])
                        cylinder(d = arm_width, h = arm_thickness);
        }

        // Kingpin bore (top)
        translate([kingpin_offset_x, knuckle_depth / 2, knuckle_height - 5])
            cylinder(d = kingpin_dia, h = 10);

        // Kingpin bore (bottom)
        translate([kingpin_offset_x, knuckle_depth / 2, -1])
            cylinder(d = kingpin_dia, h = 7);

        // Wheel hub bore
        translate([knuckle_width - 1, knuckle_depth / 2, knuckle_height / 2])
            rotate([0, 90, 0])
                cylinder(d = hub_dia, h = hub_length + 2);

        // Bearing seats (optional - for 3x6x2 bearings)
        translate([knuckle_width + 1, knuckle_depth / 2, knuckle_height / 2])
            rotate([0, 90, 0])
                cylinder(d = 6.1, h = 2.5);
        translate([knuckle_width + hub_length - 1.5, knuckle_depth / 2, knuckle_height / 2])
            rotate([0, 90, 0])
                cylinder(d = 6.1, h = 2.5);

        // Push rod hole
        translate([knuckle_width / 2, knuckle_depth, 3 + arm_thickness / 2])
            rotate([0, 0, mirror_factor * arm_angle])
                translate([0, arm_length - 3, 0])
                    rotate([90, 0, 0])
                        cylinder(d = pushrod_hole, h = 10, center = true);

        // Weight reduction
        translate([knuckle_width / 2, knuckle_depth / 2, 8])
            rotate([0, 0, 45])
                cube([6, 6, 10], center = true);
    }
}

// Render right knuckle
steering_knuckle(mirror = false);

// Uncomment to render left knuckle
// translate([30, 0, 0]) steering_knuckle(mirror = true);
