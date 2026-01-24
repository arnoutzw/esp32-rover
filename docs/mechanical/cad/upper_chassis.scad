// ============================================
// ESP32 Rover - Upper Chassis Plate
// ============================================
// Design Specification v1.0
//
// This is the upper deck of the sandwich-plate
// chassis design. Houses electronics.
// ============================================

// Main dimensions
chassis_length = 140;
chassis_width = 70;
chassis_thickness = 3;

// ESP32-CAM dimensions
esp32cam_width = 28;
esp32cam_length = 42;
esp32cam_offset_x = 5;  // From front

// SimpleFOC Mini dimensions
simplefoc_width = 25;
simplefoc_length = 25;
simplefoc_offset_x = 60;

// MG90S Servo dimensions
servo_width = 23;
servo_length = 12;
servo_offset_x = 95;

// Mounting holes
mount_hole_dia = 3.2;  // M3 clearance
standoff_hole_dia = 3.2;
mount_edge_offset = 5;  // Smaller offset for upper deck

// Resolution
$fn = 32;

module upper_chassis() {
    difference() {
        union() {
            // Main plate
            cube([chassis_length, chassis_width, chassis_thickness]);

            // ESP32-CAM mounting posts
            translate([esp32cam_offset_x + 5, (chassis_width - esp32cam_width) / 2 + 3, chassis_thickness])
                cylinder(d = 6, h = 3);
            translate([esp32cam_offset_x + esp32cam_length - 5, (chassis_width - esp32cam_width) / 2 + 3, chassis_thickness])
                cylinder(d = 6, h = 3);
            translate([esp32cam_offset_x + 5, (chassis_width + esp32cam_width) / 2 - 3, chassis_thickness])
                cylinder(d = 6, h = 3);
            translate([esp32cam_offset_x + esp32cam_length - 5, (chassis_width + esp32cam_width) / 2 - 3, chassis_thickness])
                cylinder(d = 6, h = 3);
        }

        // ESP32-CAM cutout for antenna clearance
        translate([esp32cam_offset_x, (chassis_width - esp32cam_width) / 2, -1])
            cube([esp32cam_length, esp32cam_width, chassis_thickness + 2]);

        // ESP32-CAM mounting post holes
        translate([esp32cam_offset_x + 5, (chassis_width - esp32cam_width) / 2 + 3, -1])
            cylinder(d = 2.5, h = chassis_thickness + 10, $fn = 16);
        translate([esp32cam_offset_x + esp32cam_length - 5, (chassis_width - esp32cam_width) / 2 + 3, -1])
            cylinder(d = 2.5, h = chassis_thickness + 10, $fn = 16);
        translate([esp32cam_offset_x + 5, (chassis_width + esp32cam_width) / 2 - 3, -1])
            cylinder(d = 2.5, h = chassis_thickness + 10, $fn = 16);
        translate([esp32cam_offset_x + esp32cam_length - 5, (chassis_width + esp32cam_width) / 2 - 3, -1])
            cylinder(d = 2.5, h = chassis_thickness + 10, $fn = 16);

        // SimpleFOC Mini mounting holes
        translate([simplefoc_offset_x, (chassis_width - simplefoc_width) / 2, -1]) {
            translate([3, 3, 0])
                cylinder(d = mount_hole_dia, h = chassis_thickness + 2, $fn = 16);
            translate([simplefoc_length - 3, 3, 0])
                cylinder(d = mount_hole_dia, h = chassis_thickness + 2, $fn = 16);
            translate([3, simplefoc_width - 3, 0])
                cylinder(d = mount_hole_dia, h = chassis_thickness + 2, $fn = 16);
            translate([simplefoc_length - 3, simplefoc_width - 3, 0])
                cylinder(d = mount_hole_dia, h = chassis_thickness + 2, $fn = 16);
        }

        // SimpleFOC ventilation cutout
        translate([simplefoc_offset_x + 6, (chassis_width - simplefoc_width) / 2 + 6, -1])
            cube([simplefoc_length - 12, simplefoc_width - 12, chassis_thickness + 2]);

        // Servo mounting holes (MG90S)
        translate([servo_offset_x, (chassis_width - 32) / 2, -1]) {
            // Tab mounting holes
            translate([0, 5, 0])
                cylinder(d = 2.2, h = chassis_thickness + 2, $fn = 16);
            translate([0, 32 - 5, 0])
                cylinder(d = 2.2, h = chassis_thickness + 2, $fn = 16);
        }

        // Servo body cutout
        translate([servo_offset_x - servo_length / 2, (chassis_width - servo_width) / 2, -1])
            cube([servo_length, servo_width, chassis_thickness + 2]);

        // Standoff holes - matching lower deck
        // Corners (offset from lower deck corners)
        translate([mount_edge_offset + 3, mount_edge_offset + 3, -1])
            cylinder(d = standoff_hole_dia, h = chassis_thickness + 2, $fn = 16);
        translate([chassis_length - mount_edge_offset - 3, mount_edge_offset + 3, -1])
            cylinder(d = standoff_hole_dia, h = chassis_thickness + 2, $fn = 16);
        translate([mount_edge_offset + 3, chassis_width - mount_edge_offset - 3, -1])
            cylinder(d = standoff_hole_dia, h = chassis_thickness + 2, $fn = 16);
        translate([chassis_length - mount_edge_offset - 3, chassis_width - mount_edge_offset - 3, -1])
            cylinder(d = standoff_hole_dia, h = chassis_thickness + 2, $fn = 16);

        // Center standoffs
        translate([chassis_length / 2, mount_edge_offset + 3, -1])
            cylinder(d = standoff_hole_dia, h = chassis_thickness + 2, $fn = 16);
        translate([chassis_length / 2, chassis_width - mount_edge_offset - 3, -1])
            cylinder(d = standoff_hole_dia, h = chassis_thickness + 2, $fn = 16);

        // Wire routing slots
        translate([simplefoc_offset_x - 5, chassis_width / 2 - 5, -1])
            cube([3, 10, chassis_thickness + 2]);
        translate([servo_offset_x + 10, chassis_width / 2 - 3, -1])
            cube([3, 6, chassis_thickness + 2]);
    }
}

// Render the part
upper_chassis();
