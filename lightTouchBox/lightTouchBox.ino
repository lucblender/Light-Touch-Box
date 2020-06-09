#include <CapacitiveSensor.h>
#include <Adafruit_NeoPixel.h>

#include <WiFiNINA.h> 
#include <PubSubClient.h>




const char* ssid = networkSSID;
const char* password = networkPASSWORD;
const char* mqttServer = mqttSERVER;
const char* mqttUsername = mqttUSERNAME;
const char* mqttPassword = mqttPASSWORD;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

#define DEBUG

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
#define THREASHOLD_OVER   250
#define THREASHOLD_TOUCH  2000
#define LED_COUNT         16
#define LED_SINUS_LENGHT  12.4
#define DEBOUNCE_LIMIT    5

uint8_t debounceCycle = 0;

Adafruit_NeoPixel strip(LED_COUNT, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);

CapacitiveSensor   cs_4_2 = CapacitiveSensor(4,2);        // 10M resistor between pins 4 & 2, pin 2 is sensor pin, add a wire and or foil if desired

uint8_t b = 0;

enum{
  IDLE,
  OVERING_DEBOUNCE,
  OVERING,
  TOUCH_DEBOUNCE,
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


void setup_wifi() 
{
  delay(10);
  
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) 
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  if ((char)payload[0] == '0') 
  {
    statePartner =   _IDLE;
  }
  else if ((char)payload[0] == '1') 
  {
    statePartner =  _OVERING;
  }
  else if ((char)payload[0] == '2') 
  {
    statePartner = _TOUCH;
  }
}

void reconnect() 
{
  // Loop until we're reconnected
  while (!client.connected()) 
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ArduinoClient-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqttUsername, mqttPassword)) 
    {
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe(subTopic);
    } else 
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()                    
{
  Serial.begin(BAUD_RATE);
  
  setup_wifi();
  client.setServer(mqttServer, 1883);
  client.setCallback(callback);
   
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
}

void loop()                    
{
  long total1 =  cs_4_2.capacitiveSensor(80);  

  DEBUG_PRINTLN(total1);


  if (!client.connected()) 
  {
    reconnect();
  }
  client.loop();

  
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
      //define in what state we are
      if(total1>THREASHOLD_TOUCH)
        stateSelf = TOUCH;
      else if(total1>THREASHOLD_OVER)
        stateSelf = OVERING_DEBOUNCE;
      else    
        stateSelf = IDLE;
      break;
    case OVERING_DEBOUNCE:       
      if(total1>THREASHOLD_TOUCH)
        stateSelf = TOUCH;  
      else if(total1>THREASHOLD_OVER && total1 < THREASHOLD_TOUCH){
        debounceCycle++;
        if(debounceCycle >= DEBOUNCE_LIMIT)
          stateSelf = OVERING;
      }else{
          stateSelf = IDLE;        
      }
      break;
    case OVERING:
    case TOUCH:
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
    char payLoad[1];
    switch (stateSelf)
    {
      case IDLE:      
        itoa(0, payLoad, 10);
        client.publish(pubTopic, payLoad);
        DEBUG_PRINTLN("Enter IDLE TODO send mqtt here");
        break;    
      case OVERING_DEBOUNCE:        
        debounceCycle = 0;
        DEBUG_PRINTLN("Enter OVERING_DEBOUNCE TODO send mqtt here");
        break;  
      case OVERING:
        itoa(1, payLoad, 10);
        client.publish(pubTopic, payLoad);
        DEBUG_PRINTLN("Enter OVERING TODO send mqtt here");
        break;
      case TOUCH:      
        itoa(2, payLoad, 10);
        client.publish(pubTopic, payLoad);
        DEBUG_PRINTLN("Enter TOUCH TODO send mqtt here");
        break;
    }
    lastStateSelf = stateSelf;
  }
  
  switch (lastStateLight)
  {
    case IDLE_LIGHT:
        DEBUG_PRINTLN("IDLE_LIGHT");  
      if((stateSelf!=IDLE&&stateSelf!=OVERING_DEBOUNCE) || statePartner!=_IDLE){
        stateLight = ON_LIGHT;
      }
      break;
    case ON_LIGHT:
        DEBUG_PRINTLN("ON_LIGHT");  
      light();
      if((stateSelf==IDLE||stateSelf==OVERING_DEBOUNCE) && statePartner==_IDLE){
        stateLight = IDLE_LIGHT;
      }
      break;
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
  DEBUG_PRINTLN("light");
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
