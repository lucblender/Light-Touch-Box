# Light Touch Box

This git regroup all the files to create a smart Light Touch Box. The goal of those box is to have 2 box connected together (via a mqtt broker), and when one of the box is touched, the other box light up and vice-versa.

On top of the box you can find a resin bloc (50x50x65mm in my original design) that will light up.


## Requirement 

### Material

For one box you will need:
- Arduino nano 33 IOT
- Adafruit neopixel ring 16 leds
- 3D printed part of 3D files/Support Arduino LED
- 3D printed or laser cut part 
	- 3D files/Bottom
	- 3D files/Side_Hole
	- 3D files/Side_Plain x3
- Electric paint (tested only bare conductive paint)
- Some Resin

### Code

This project use arduino nano 33 IOT. The library to use are:

- CapacitiveSensor by Paul Badger, Paus Stoffregen
- Adafruit NeoPixel by Adafruit
- PubSubClient by Nick O'Leary

### Mechanic Mechanic

- Assembly all the box part (Bottom, Side_Hole and the three Side_Plain). 
- Paint all the sides of the box with the electric paint.
- On the arduino, put a 1MOhms between pin 2-4 and a wire soldered going from pin 2. The end of this wire goes through the little hole of the Side_Hole plate. Add a little bit of electric paint on the wire going out of the box.
- Solder the wire of the neopixel ring on the arduino (5V, GND and Data to Pin 6 of the arduino
- Insert the 3D printed Support Arduino LED inside the box
- Put the arduino and the led on the support
- Put the resin bloc on top of the led

#### 3D view

![Fusion View](../master/pictures/Fusion.PNG "Fusion View")