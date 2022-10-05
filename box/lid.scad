$fn = 32;

include <measurements.scad>

difference()
{
    union()
    {
        cube([box_internal_width+wall_thickness*2, box_internal_length+wall_thickness*2, wall_thickness/2]);
        #translate([box_internal_width/2+insert_post_d/4,insert_post_d/2+box_internal_length+wall_thickness,0]) cylinder(h=1, d=insert_post_d);
    }
    #translate([box_internal_width/2+insert_post_d/4,insert_post_d/2+box_internal_length+wall_thickness,0]) cylinder(d=bolt_diameter, h=1);


    #translate([insert_post_d/2,insert_post_d/2,0]) cylinder(d=bolt_diameter, h=1);
    #translate([box_internal_width,insert_post_d/2,0]) cylinder(d=bolt_diameter, h=1);
}