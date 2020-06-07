#include <CapacitiveSensor.h>
#include <Adafruit_NeoPixel.h>

#define BAUD_RATE         115200
#define LED_STRIP_PIN     6
#define THREASHOLD_OVER   30
#define THREASHOLD_TOUCH  300
#define LED_COUNT         16
#define LED_SINUS_LENGHT  12.4

Adafruit_NeoPixel strip(LED_COUNT, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);

CapacitiveSensor   cs_4_2 = CapacitiveSensor(4,2);        // 10M resistor between pins 4 & 2, pin 2 is sensor pin, add a wire and or foil if desired

int b = 0;

enum{
  IDLE,
  OVERING,
  TOUCH
}state_self = IDLE;

enum{
  _IDLE,
  _OVERING,
  _TOUCH
}state_partner = _IDLE;

void setup()                    
{
  Serial.begin(BAUD_RATE);
   
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
}

void loop()                    
{
  long total1 =  cs_4_2.capacitiveSensor(30);
  

  //TEMPORARY will use mqtt to send and retreive the capacitive sensor data
  if (Serial.available() > 0) {
    int incomingByte = Serial.read();

    // say what you got:
    Serial.print("I received: ");
    Serial.println(incomingByte, DEC);

    if(incomingByte == 48)
      state_partner = _IDLE;
    else if(incomingByte == 49)
      state_partner = _OVERING;
    else if(incomingByte == 50)
      state_partner = _TOUCH;      
  }
  
  //retreive capacitive sensor data
  //Serial.println(total1);

  //define in what state we are
  if(total1>THREASHOLD_TOUCH)
    state_self = TOUCH;
  else if(total1>THREASHOLD_OVER)
    state_self = OVERING;
  else    
    state_self = IDLE;


  //go through all leds to light them properly
  for(int i= 0; i<LED_COUNT; i++){

    uint32_t rgbcolorSelf;
    uint8_t offsetSelf;


    uint32_t rgbcolorPartner;
    uint8_t offsetPartner;

    //create an offset depending of the our capacitive sensor
    switch (state_self)
    {
      case IDLE:        
        offsetSelf = 0;
        break;
      case OVERING:          
        offsetSelf = 10;
        break;
      case TOUCH: 
        offsetSelf = 30;
        break;
    }

    //create an offset depending of the our partner capacitive sensor
    switch (state_partner)
    {
      case _IDLE:        
        offsetPartner = 0;
        break;
      case _OVERING:          
        offsetPartner = 30;
        break;
      case _TOUCH: 
        if(state_self == TOUCH)
          offsetPartner = 255;  //if both touch, double frequency
        else          
          offsetPartner = 127;
        break;
    }
    
    int valueSelf = (sin(radians((i+b)*(360/LED_SINUS_LENGHT)))+1)*offsetSelf;          
    rgbcolorSelf = strip.ColorHSV(500, 255, valueSelf); 
          
    int valuePartner = (sin(radians((b)*(360/LED_SINUS_LENGHT)))+1)*offsetPartner;          
    rgbcolorPartner = strip.ColorHSV(43690, 127, valuePartner); 
    //the final color is the addition of self and partner color
    strip.setPixelColor((i+b)%LED_COUNT, rgbcolorSelf+rgbcolorPartner);
  }    
  strip.show();
  b++;
  
  delay(100);   
}
