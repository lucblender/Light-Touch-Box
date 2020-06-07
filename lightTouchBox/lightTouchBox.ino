#include <CapacitiveSensor.h>
#include <Adafruit_NeoPixel.h>

//#define DEBUG

#ifdef DEBUG
 #define DEBUG_PRINT(x)     Serial.print (x)
 #define DEBUG_PRINTDEC(x)     Serial.print (x, DEC)
 #define DEBUG_PRINTLN(x)  Serial.println (x)
#else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTDEC(x)
 #define DEBUG_PRINTLN(x)
#endif

#define BAUD_RATE         115200
#define LED_STRIP_PIN     6
#define THREASHOLD_OVER   80
#define THREASHOLD_TOUCH  300
#define LED_COUNT         16
#define LED_SINUS_LENGHT  12.4

Adafruit_NeoPixel strip(LED_COUNT, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);

CapacitiveSensor   cs_4_2 = CapacitiveSensor(4,2);        // 10M resistor between pins 4 & 2, pin 2 is sensor pin, add a wire and or foil if desired

uint8_t b = 0;

enum{
  IDLE,
  OVERING,
  TOUCH
}stateSelf = IDLE;
int lastStateSelf = IDLE;

enum{
  _IDLE,
  _OVERING,
  _TOUCH
}statePartner = _IDLE;

enum{
  IDLE_LIGHT,
  ON_LIGHT
}stateLight = IDLE_LIGHT;
int lastStateLight = IDLE_LIGHT;

void setup()                    
{
  Serial.begin(BAUD_RATE);
   
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
}

void loop()                    
{
  long total1 =  cs_4_2.capacitiveSensor(80);  

  DEBUG_PRINTLN(total1);
  //TEMPORARY will use mqtt to send and retreive the capacitive sensor data
  if (Serial.available() > 0) {
    int incomingByte = Serial.read();

    if(incomingByte == 48)
      statePartner = _IDLE;
    else if(incomingByte == 49)
      statePartner = _OVERING;
    else if(incomingByte == 50)
      statePartner = _TOUCH;      
  }

  switch (lastStateSelf)
  {
    case IDLE:
    case OVERING:
    case TOUCH:
      //define in what state we are
      if(total1>THREASHOLD_TOUCH)
        stateSelf = TOUCH;
      else if(total1>THREASHOLD_OVER)
        stateSelf = OVERING;
      else    
        stateSelf = IDLE;
      break;
   
    default:
      break;
  }

  if(stateSelf != lastStateSelf)
  {
    switch (stateSelf)
    {
      case IDLE:
        DEBUG_PRINTLN("Enter IDLE send mqtt here");
        break;      
      case OVERING:
        DEBUG_PRINTLN("Enter OVERING send mqtt here");
        break;
      case TOUCH:
        DEBUG_PRINTLN("Enter TOUCH send mqtt here");
        break;
    }
    lastStateSelf = stateSelf;
  }
  
  switch (lastStateLight)
  {
    case IDLE_LIGHT:
      if(stateSelf!=IDLE || statePartner!=_IDLE){
        stateLight = ON_LIGHT;
      }
    case ON_LIGHT:
      light();
      if(stateSelf==IDLE && statePartner==_IDLE){
        stateLight = IDLE_LIGHT;
      }
    default:
      break;
  }

  if(stateLight != lastStateLight)
  {
    switch (stateLight)
    {
      case IDLE_LIGHT:
        DEBUG_PRINTLN("Enter IDLE_LIGHT");           
        strip.clear();
        strip.show();
        break;      
      case ON_LIGHT:
        DEBUG_PRINTLN("Enter ON_LIGHT");
        break;
    }
    lastStateLight = stateLight;
  }
  
  delay(100);   
}

void light(){
    //go through all leds to light them properly
  for(int i= 0; i<LED_COUNT; i++){

    uint32_t rgbcolorSelf;
    uint8_t offsetSelf;


    uint32_t rgbcolorPartner;
    uint8_t offsetPartner;

    //create an offset depending of the our capacitive sensor
    switch (stateSelf)
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
    switch (statePartner)
    {
      case _IDLE:        
        offsetPartner = 0;
        break;
      case _OVERING:          
        offsetPartner = 30;
        break;
      case _TOUCH: 
        if(stateSelf == TOUCH)
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
  b++%16;
}
