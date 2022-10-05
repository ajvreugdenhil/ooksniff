$fn = 32;

include <measurements.scad>

main();

module insert_post()
{
    difference()
    {
        cylinder(h=box_internal_height+wall_thickness, d=insert_diameter+wall_thickness*2);
        translate([0,0,(box_internal_height+wall_thickness)-insert_length]) cylinder(h=insert_length, d=insert_diameter);
    }
}

module main()
{
    translate([insert_post_d/2,insert_post_d/2,0]) insert_post();
    translate([box_internal_width,insert_post_d/2,0]) insert_post();

    translate([box_internal_width/2+insert_post_d/4,insert_post_d/2+box_internal_length+wall_thickness,0]) insert_post();

    translate([wall_thickness+9, wall_thickness+(44-20), 0]) cube([8,20,box_internal_height+wall_thickness]);

    difference()
    {
        cube([box_internal_width+wall_thickness*2, box_internal_length+wall_thickness*2, box_internal_height+wall_thickness]);
        translate([wall_thickness,wall_thickness,wall_thickness]) cube([box_internal_width, box_internal_length, box_internal_height]);
        translate([0, wall_thickness+box_internal_length - 8.25  -(25.8-8.25)/2, wall_thickness]) cube([wall_thickness, 8.25, 3.85]);
    }
}