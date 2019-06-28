# filament_minder
Arduino sketch for a HX711 load cell based filament usage tracker.

To use: 

Calibration:  You will have to obtain the LOADCELL_OFFSET and LOADCELL_DIVIDER for your particular load cell.   This can be done by reading the load cell, installed in the Filament Minder, but with no spool installed, and the OFFSET required will be a number that negates the reading to zero.  Eg, if the hx711 reads -145700 unloaded, the offset will be 145700.  The divider can be calculated once the offset is configured by hanging a known weight, either a calibration weight, or something measured on a different scale, from the load cell.  Then once you see what value the HX711 thinks it is, you can do a simple cross multiplication to determine what the divider should be to give the correct results in grams.

Adding a new spool: To configure a new spool, hang the spool from the spindle, use the left and right buttons to navigate to the slot you wish the new spool to occupy, then hold the round button until instructed to release it.  Then you can use the left and right buttons to adjust the capacity of the new spool to match the spool size (1000g=1kg, for example).  When correct, press the round button again and the capacity and starting weight will be saved.
