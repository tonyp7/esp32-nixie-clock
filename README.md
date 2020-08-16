# esp32-nixie-clock

A smart nixie clock capable of automatically setting time and adjusting for timezones / summer time transitions.

![Clock face](https://raw.githubusercontent.com/tonyp7/esp32-nixie-clock/master/pictures/nixie-clock-front-1024.jpg)

# Features

**Smart**

  - Time is set automatically through a web API
  - A webapp is provided to adjust wifi settings, timezone and backlights
  
The wifi settings are managed by [esp32-wifi-manager](https://github.com/tonyp7/esp32-wifi-manager).

**A modern dispay driver**

  - SPI based driver
  - No multiplexing of nixies
  - No relying on old nixie drivers

Each pin is fully driven by its individual 300V NPN transistor.

**A robust high voltage converter**

 - A modern flyback converter with top of the line controller
 - No 555, no old 34063 trickery

The nixie clock is powered by a LT3757 controller and a big 1:10 transformer. The design can be found [here](https://github.com/tonyp7/170v-nixie-power-supply). The last thing you need is for your clock to catch fire!

# Repository structure

- bom: Bill of Materials for the PCBs
- nixie_clock_code: An esp-idf project to compile and flash on an ESP32
- pcb: PCB source files and renders for the clock
- pictures: A placeholder for the pictures you are seeing in this readme file
- schematics: Electronics schematics for the clock

# Photos

### Clock in its environment
![Nixie Clock](https://raw.githubusercontent.com/tonyp7/esp32-nixie-clock/master/pictures/nixie-clock-coffee-table-1024.jpg)

### The clock face PCB
![Clock face PCB](https://raw.githubusercontent.com/tonyp7/esp32-nixie-clock/master/pictures/clock-face-assembled-1024.jpg)

### The logic board PCB
![Logic board PCB PCB](https://raw.githubusercontent.com/tonyp7/esp32-nixie-clock/master/pictures/logic-board-assembled-1024.jpg)

