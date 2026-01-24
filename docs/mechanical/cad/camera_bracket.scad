// ============================================
// ESP32 Rover - Camera Bracket (45°)
// ============================================
// Design Specification v1.0
//
// 45-degree angled bracket for ESP32-CAM.
// Provides optimal FPV viewing angle.
// ============================================

// ESP32-CAM dimensions
pcb_width = 27;
pcb_length = 40.5;
pcb_thickness = 1.6;

// Bracket dimensions
bracket_base_length = 50;
bracket_base_width = 35;
bracket_base_thickness = 3;

bracket_angle = 45;  // Degrees from horizontal
bracket_back_height = 30;
bracket_wall_thickness = 2;

// Camera lens position (from bottom of PCB)
lens_offset_y = 8;
lens_diameter = 10;

// Mounting
mount_hole_dia = 3.2;  // M3 clearance

// Resolution
$fn = 32;

module camera_bracket() {
    difference() {
        union() {
            // Base plate
            cube([bracket_base_length, bracket_base_width, bracket_base_thickness]);

            // Angled back support
            translate([0, 0, bracket_base_thickness])
                rotate([bracket_angle, 0, 0])
                    translate([0, -bracket_wall_thickness, 0])
                        cube([bracket_base_length, bracket_wall_thickness, bracket_back_height]);

            // Side walls
            translate([0, 0, bracket_base_thickness])
                rotate([bracket_angle, 0, 0])
                    translate([0, 0, 0])
                        cube([bracket_wall_thickness, pcb_length + 5, bracket_back_height]);

            translate([bracket_base_length - bracket_wall_thickness, 0, bracket_base_thickness])
                rotate([bracket_angle, 0, 0])
                    translate([0, 0, 0])
                        cube([bracket_wall_thickness, pcb_length + 5, bracket_back_height]);

            // PCB support ledges
            translate([bracket_wall_thickness, 0, bracket_base_thickness])
                rotate([bracket_angle, 0, 0])
                    translate([0, 2, 0])
                        cube([5, pcb_length, 2]);

            translate([bracket_base_length - bracket_wall_thickness - 5, 0, bracket_base_thickness])
                rotate([bracket_angle, 0, 0])
                    translate([0, 2, 0])
                        cube([5, pcb_length, 2]);

            // Gussets for strength
            translate([0, 0, bracket_base_thickness])
                linear_extrude(height = bracket_wall_thickness)
                    polygon([[0, 0], [15, 0], [0, 15]]);

            translate([bracket_base_length, 0, bracket_base_thickness])
                linear_extrude(height = bracket_wall_thickness)
                    polygon([[0, 0], [-15, 0], [0, 15]]);
        }

        // Camera lens cutout
        translate([bracket_base_length / 2, 0, bracket_base_thickness])
            rotate([bracket_angle, 0, 0])
                translate([0, lens_offset_y + 5, -1])
                    cylinder(d = lens_diameter + 2, h = bracket_wall_thickness + 2);

        // PCB slot
        translate([(bracket_base_length - pcb_width) / 2, 0, bracket_base_thickness])
            rotate([bracket_angle, 0, 0])
                translate([0, 2, 2])
                    cube([pcb_width + 0.5, pcb_length + 2, pcb_thickness + 0.5]);

        // Base mounting holes
        translate([10, bracket_base_width / 2 - 10, -1])
            cylinder(d = mount_hole_dia, h = bracket_base_thickness + 2, $fn = 16);
        translate([10, bracket_base_width / 2 + 10, -1])
            cylinder(d = mount_hole_dia, h = bracket_base_thickness + 2, $fn = 16);
        translate([bracket_base_length - 10, bracket_base_width / 2 - 10, -1])
            cylinder(d = mount_hole_dia, h = bracket_base_thickness + 2, $fn = 16);
        translate([bracket_base_length - 10, bracket_base_width / 2 + 10, -1])
            cylinder(d = mount_hole_dia, h = bracket_base_thickness + 2, $fn = 16);

        // Ventilation slots on back
        for (i = [0:4]) {
            translate([10 + i * 7, -5, bracket_base_thickness])
                rotate([bracket_angle, 0, 0])
                    translate([0, -bracket_wall_thickness - 1, 5])
                        cube([3, bracket_wall_thickness + 2, bracket_back_height - 10]);
        }

        // Cable exit slot
        translate([bracket_base_length / 2 - 5, 0, bracket_base_thickness])
            rotate([bracket_angle, 0, 0])
                translate([0, pcb_length, 0])
                    cube([10, 10, 5]);
    }
}

// Render the part
camera_bracket();
