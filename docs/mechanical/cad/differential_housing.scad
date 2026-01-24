// ============================================
// ESP32 Rover - Differential Housing
// ============================================
// Design Specification v1.0
//
// Open differential housing for rear axle.
// Includes spur gear mount.
// ============================================

// Differential dimensions
diff_width = 50;  // Overall width
diff_body_dia = 25;
diff_body_length = 30;

// Axle
axle_dia = 5;
axle_bore = 5.2;

// Spur gear
spur_gear_dia = 48;  // 48T at M0.5 = 24mm pitch dia
spur_gear_bore = 8;
spur_gear_thickness = 5;
spur_gear_offset = 5;  // From center

// Spider gear cavity
spider_cavity_dia = 18;
spider_cavity_depth = 12;

// Mounting
mount_hole_dia = 3.2;
mount_hole_spacing = 40;

// Resolution
$fn = 32;

module differential_housing() {
    difference() {
        union() {
            // Main body
            rotate([0, 90, 0])
                cylinder(d = diff_body_dia, h = diff_body_length, center = true);

            // Axle housings
            translate([-diff_width / 2, 0, 0])
                rotate([0, 90, 0])
                    cylinder(d = 12, h = 10);
            translate([diff_width / 2 - 10, 0, 0])
                rotate([0, 90, 0])
                    cylinder(d = 12, h = 10);

            // Spur gear boss
            translate([spur_gear_offset, 0, 0])
                rotate([0, 90, 0])
                    cylinder(d = spur_gear_bore + 6, h = spur_gear_thickness);

            // Mounting ears
            translate([-mount_hole_spacing / 2, 0, -diff_body_dia / 2])
                cylinder(d = 8, h = 5);
            translate([mount_hole_spacing / 2, 0, -diff_body_dia / 2])
                cylinder(d = 8, h = 5);
        }

        // Axle bores
        translate([-diff_width / 2 - 1, 0, 0])
            rotate([0, 90, 0])
                cylinder(d = axle_bore, h = diff_width + 2);

        // Spider gear cavity (left)
        translate([-spider_cavity_depth / 2, 0, 0])
            rotate([0, 90, 0])
                cylinder(d = spider_cavity_dia, h = spider_cavity_depth, center = true);

        // Spider gear cavity (right)
        translate([spider_cavity_depth / 2, 0, 0])
            rotate([0, 90, 0])
                cylinder(d = spider_cavity_dia, h = spider_cavity_depth, center = true);

        // Cross pin hole
        translate([0, 0, -spider_cavity_dia / 2 - 1])
            cylinder(d = 3, h = spider_cavity_dia + 2);

        // Spur gear bore
        translate([spur_gear_offset - 1, 0, 0])
            rotate([0, 90, 0])
                cylinder(d = spur_gear_bore, h = spur_gear_thickness + 2);

        // Spur gear keyway
        translate([spur_gear_offset, -1.5, 0])
            rotate([0, 90, 0])
                cube([axle_dia, 3, spur_gear_thickness + 2], center = true);

        // Mounting holes
        translate([-mount_hole_spacing / 2, 0, -diff_body_dia / 2 - 1])
            cylinder(d = mount_hole_dia, h = 10, $fn = 16);
        translate([mount_hole_spacing / 2, 0, -diff_body_dia / 2 - 1])
            cylinder(d = mount_hole_dia, h = 10, $fn = 16);

        // Bearing seats (for 5x10x4 bearings)
        translate([-diff_width / 2 + 2, 0, 0])
            rotate([0, 90, 0])
                cylinder(d = 10.1, h = 4);
        translate([diff_width / 2 - 6, 0, 0])
            rotate([0, 90, 0])
                cylinder(d = 10.1, h = 4);

        // Assembly split line (for printing in two halves)
        // Uncomment one of these to create left or right half
        // translate([0, -20, -20]) cube([40, 40, 40]);  // Right half
        // translate([-40, -20, -20]) cube([40, 40, 40]); // Left half
    }
}

// Render the part
differential_housing();
