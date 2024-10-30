$fn = 64;

nodemcu_x = 26;
nodemcu_y = 49.9;
nodemcu_z = 5;

transmitter_x = 29.9;
transmitter_y = 5.5;
transmitter_z = 15.5;

divider_y = 20;
divider_wall_thickness = 1;
wiring_z = 0;
wiring_x = 6;
divider_x = max(transmitter_x, nodemcu_x) - wiring_x*2;
transmitter_base_wall_thickness = 1.5;

box_thickness = 2.5;
box_bottom_thickness = 2;
box_top_thickness = 2;
box_rounding = 2;

box_inside_x = max(nodemcu_x, transmitter_x);
box_inside_y = nodemcu_y + divider_y + transmitter_y;
box_inside_z = max(nodemcu_z, transmitter_z)+wiring_z;

box_outside_x = box_inside_x + box_thickness*2;
box_outside_y = box_inside_y + box_thickness*2;
box_outside_z = box_inside_z + box_top_thickness + box_bottom_thickness;

usb_cutout_height = 4;
usb_cutout_offset_z = 2;
usb_cutout_offset_x = 7;
usb_cutout_width = 8.5;
antenna_radius = 6.5/2;
antenna_height = 4;
antenna_elongation = 20; //hack but whatever

// yea yea i know, horrible misuse of words
module prism(lower, upper, length, height)
{
    points = [
    [ -lower/2, -length/2,  0 ],  //0
    [ lower/2,  -length/2,  0 ],  //1
    [ lower/2,  length/2,   0 ],  //2
    [ -lower/2,  length/2,  0 ],  //3
    [ -upper/2, -length/2, height ],  //4
    [ upper/2,  -length/2, height ],  //5
    [ upper/2,  length/2,  height ],  //6
    [ -upper/2, length/2,  height ]]; //7
    
    faces = [
    [0,1,2,3],  // bottom
    [4,5,1,0],  // front
    [7,6,5,4],  // top
    [5,6,2,1],  // right
    [6,7,3,2],  // back
    [7,4,0,3]]; // left
  
    polyhedron( points, faces );
}


module box()
{
    union()
    {
        // Holes
        difference() 
        {
            // Main box
            translate([0,box_inside_y/2,0]) 
            difference() 
            {
                translate([0,0,box_rounding])
                hull()
                {
                    translate([  box_outside_x/2 -box_rounding,   box_outside_y/2 -box_rounding, 0]) sphere(r = box_rounding);
                    translate([  box_outside_x/2-box_rounding,  -(box_outside_y/2 -box_rounding),0]) sphere(r = box_rounding);
                    translate([-(box_outside_x/2 -box_rounding),  box_outside_y/2 -box_rounding, 0]) sphere(r = box_rounding);
                    translate([-(box_outside_x/2 -box_rounding),-(box_outside_y/2 -box_rounding),0]) sphere(r = box_rounding);

                    translate([  box_outside_x/2 -box_rounding,   box_outside_y/2 -box_rounding, box_outside_z-box_rounding*2]) sphere(r = box_rounding);
                    translate([  box_outside_x/2-box_rounding,  -(box_outside_y/2 -box_rounding),box_outside_z-box_rounding*2]) sphere(r = box_rounding);
                    translate([-(box_outside_x/2 -box_rounding),  box_outside_y/2 -box_rounding, box_outside_z-box_rounding*2]) sphere(r = box_rounding);
                    translate([-(box_outside_x/2 -box_rounding),-(box_outside_y/2 -box_rounding),box_outside_z-box_rounding*2]) sphere(r = box_rounding);
                }
                // Big Cutout
                translate([0,0,box_bottom_thickness+(box_inside_z+box_top_thickness)/2]) cube([box_inside_x, box_inside_y, box_inside_z+box_top_thickness], center=true);
                // Lid cutout
                translate([0,-box_thickness/2,box_bottom_thickness+box_inside_z]) 
                    prism(lower=box_inside_x+box_thickness, upper=box_inside_x, length=box_inside_y+box_thickness, height=box_top_thickness);
            }
            
            // MicroUSB cutout
            translate([
                usb_cutout_offset_x,
                -box_thickness/2,
                usb_cutout_height/2+box_bottom_thickness+usb_cutout_offset_z]) 
                cube([usb_cutout_width,box_thickness,usb_cutout_height], center=true);
            // Antenna cutout
            translate([0,-(antenna_radius*2 - transmitter_y)/2,0]) 
            translate([
                -box_thickness-box_inside_x/2,
                nodemcu_y + divider_y + transmitter_y/2,
                antenna_radius+box_bottom_thickness+antenna_height]) 
                hull()
                {
                    rotate([0,90,0]) 
                        cylinder(h=box_thickness, r=antenna_radius);
                    translate([0,0,antenna_elongation])     
                        rotate([0,90,0]) 
                        cylinder(h=box_thickness, r=antenna_radius);
                }       
        }
        // Divider
        divider_height = max(nodemcu_z, transmitter_z);
        translate([
            0,
            divider_y/2 + nodemcu_y,
            divider_height/2+box_bottom_thickness]) 
            difference() 
            {
                cube([divider_x,divider_y,divider_height], center=true);
                cube([divider_x-divider_wall_thickness*2,divider_y-divider_wall_thickness*2,divider_height], center=true);
            }
        // Base for transmitter
        // h = 1.5;
        // translate([
            // 0,
            // transmitter_y/2 + nodemcu_y + divider_y,
            // h/2+box_bottom_thickness]) 
            // difference() 
            // {
            //     cube([transmitter_x,transmitter_y,h], center=true);
            //     cube([transmitter_x,transmitter_y-transmitter_base_wall_thickness*2,h],center=true);
            // }
        // Nodemcu walls
        wall_width = (box_inside_x - nodemcu_x)/2;
        for( i = [1,-1])
        {
            translate([
                i*(box_inside_x/2 - wall_width/2),
                nodemcu_y/2,
                divider_height/2 + box_bottom_thickness])
                cube([wall_width,nodemcu_y,divider_height], center=true);
        }
    }
}

tolerance = 0.3;
module lid()
{
    length = box_inside_y+box_thickness-tolerance;
    difference() 
    {
        prism(lower=box_inside_x+box_thickness-tolerance, upper=box_inside_x-tolerance, length=length, height=box_top_thickness);
        translate([0,-length/2 + box_rounding,0]) 
        difference() {
            translate([0,-box_rounding/2,box_top_thickness/2]) 
                cube([box_outside_x,box_rounding,box_top_thickness], center=true);
            hull()
            {
                for( i = [1,-1])
                {   
                    translate([i*(box_outside_x/2 -box_rounding),0,box_top_thickness-box_rounding]) sphere(r = box_rounding);
                }
            }
        }
    }
    translate([0,0,box_top_thickness]) 
        rotate([0,0,90])
        linear_extrude(height = 0.4) 
        text("I'm snorfin' here", 
            halign = "center", 
            size=7, 
            font="Liberation Sans:style=Bold", 
            valign="center");
}

box();

translate([box_outside_x + 10,box_inside_y/2 + box_thickness/2,0])
lid();