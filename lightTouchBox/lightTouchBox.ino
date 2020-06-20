#include <CapacitiveSensor.h>
#include <Adafruit_NeoPixel.h>

#include <ArduinoBLE.h>
#include <WiFiNINA.h> 
#include <PubSubClient.h>
#include "credential.h"
#include "utility/wifi_drv.h"

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
#define THREASHOLD_OVER   400
#define THREASHOLD_TOUCH  2000
#define LED_COUNT         16
#define LED_SINUS_LENGHT  12.4
#define DEBOUNCE_LIMIT    10
#define DELAY_MS          100
#define BLE_SKIP_MS       15000
#define BLE_SKIP_CYCLE    (BLE_SKIP_MS/DELAY_MS)

char subTopic[] = "UUIDA/ligthTouch";
char pubTopic[] = "UUIDB/ligthTouch";

char* ssid = networkSSID;
char* password = networkPASSWORD;
const char* mqttServer = mqttSERVER;
const char* mqttUsername = mqttUSERNAME;
const char* mqttPassword = mqttPASSWORD;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

//Bluetooth parameters
BLEUnsignedIntCharacteristic capacitiveMeasureCharacteristic("2A58", BLERead | BLENotify);
BLEBooleanCharacteristic configurationDoneCharacteristic("41300001-EB90-09D7-0001-413031360000", BLERead | BLEWrite);
BLECharacteristic ssidCharacteristic("41300001-EB90-09D7-0001-413031360001", BLERead | BLEWrite,32);
BLECharacteristic passwordCharacteristic("41300001-EB90-09D7-0001-413031360002", BLERead | BLEWrite,63);
BLEService lightBoxService("19B10010-E8F2-537E-4F6C-D104768A1214"); 
//Actual value of bluetooth characteristic
boolean configurationDoneBle = false;
char ssidValue[32] ;
char passwordValue[63] ;

//Wireless connection status
boolean wifiConnectionSuccess = false;
boolean bleConnected = false;
boolean bleConfigured = false;

//cycle in loop since debounce started
uint8_t debounceCycle = 0;
//cycle in loop since ble advertising started
uint8_t bleSkipCycle = 0;

//offset of light incrementing to do light animation
uint8_t lightOffset = 0;

Adafruit_NeoPixel strip(LED_COUNT, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);
CapacitiveSensor   cs_4_2 = CapacitiveSensor(4,2);        // 10M resistor between pins 4 & 2, pin 2 is sensor pin, add a wire and or foil if desired

//all state machin states
enum{
  INIT,
  BLE_CONFIGURATION,
  WIFI_CONNECTION,
  IDLE,
  OVERING_DEBOUNCE,
  OVERING,
  TOUCH
}stateSelf = INIT;
int lastStateSelf = INIT;

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

enum{
  OFF,
  CONNECTING,
  FAIL,  
  ON
}stateWifi = OFF;


void setup_wifi() 
{
  int retry = 0;
  stateWifi = CONNECTING;
  delay(10);
  
  // We start by connecting to a WiFi network
  
  lightConnectionStatus();
  DEBUG_PRINT("Connecting to ");
  DEBUG_PRINTLN(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED && retry < 50) 
  {
    lightConnectionStatus();
    delay(200);
    DEBUG_PRINT(".");
    retry ++;
  }

  if(WiFi.status() != WL_CONNECTED){
    stateWifi = FAIL;
    wifiConnectionSuccess = false;  
    DEBUG_PRINTLN("WiFi connection failed!!!");
    for(int i = 0; i<200;i++)
    {
      lightConnectionStatus();
      delay(5);          
      strip.clear();
      strip.show();
    }    
    NVIC_SystemReset();
  }
  else{
    stateWifi = ON;
    wifiConnectionSuccess = true;
    randomSeed(micros());
  
    DEBUG_PRINTLN("WiFi connected");
    DEBUG_PRINTLN("IP address: ");
    DEBUG_PRINTLN(WiFi.localIP());
    for(int i = 0; i<200;i++)
    {
      lightConnectionStatus();
      delay(5);          
      strip.clear();
      strip.show();
    }
  }

}

void setup_ble(){
// begin initialization
  if (!BLE.begin()) {
    DEBUG_PRINTLN("starting BLE failed!");

    while (1);
  }
  //retreive the first 8 byte of the serial number to add it to the 
  //bluetooth local name
  volatile uint32_t val1;
  volatile uint32_t *ptr1 = (volatile uint32_t *)0x0080A00C;
  val1 = *ptr1;

  Serial.print("chip id: 0x ");
  char buf[18];
  sprintf(buf, "LIGHTBOX-%8x", val1);
  buf[17] = '\0';
  Serial.println(buf);
  
  // set the local name peripheral advertises
  BLE.setLocalName(buf);  
  // set the UUID for the service this peripheral advertises:
  BLE.setAdvertisedService(lightBoxService);
  
  if(bleConfigured == false){  
    // add the characteristics to the service
    lightBoxService.addCharacteristic(configurationDoneCharacteristic);
    lightBoxService.addCharacteristic(capacitiveMeasureCharacteristic);
    lightBoxService.addCharacteristic(ssidCharacteristic);
    lightBoxService.addCharacteristic(passwordCharacteristic);
  
    configurationDoneCharacteristic.setValue(configurationDoneBle);
    
    sprintf(ssidValue,ssid);
    ssidCharacteristic.setValue((unsigned char *)ssidValue,32) ;
    
    sprintf(passwordValue,password);
    passwordCharacteristic.setValue((unsigned char *)passwordValue,63) ;
  
    // add the service
    BLE.addService(lightBoxService);
  
    BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
    BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
  
    configurationDoneCharacteristic.setEventHandler(BLEWritten, appCharacteristicWrittenConfigurationDone);
    ssidCharacteristic.setEventHandler(BLEWritten, appCharacteristicWrittenSsid);
    passwordCharacteristic.setEventHandler(BLEWritten, appCharacteristicWrittenPassword);
    //bleConfigured = true;
  }

  // start advertising
  BLE.advertise();
}

void appCharacteristicWrittenConfigurationDone(BLEDevice central, BLECharacteristic characteristic) {
  DEBUG_PRINT("ConfigurationDone Characteristic event, written: ");
  DEBUG_PRINTLN(characteristic.uuid());

  configurationDoneBle = characteristic.value();
  DEBUG_PRINTLN(configurationDoneBle);
}
  
void appCharacteristicWrittenSsid(BLEDevice central, BLECharacteristic characteristic) {
  
  DEBUG_PRINT(ssid);
  DEBUG_PRINTLN("TEST");
  DEBUG_PRINT("SSID Characteristic event, written: ");
  DEBUG_PRINTLN(characteristic.uuid());

  DEBUG_PRINT("Admin : SSID recu ");
  DEBUG_PRINTLN(characteristic.valueLength());
  sprintf(ssidValue,"%32c",NULL);
  strncpy(ssidValue,(char*)characteristic.value(),characteristic.valueLength());
  ssidValue[characteristic.valueLength()] = '\0';
  ssid = ssidValue;
}

void appCharacteristicWrittenPassword(BLEDevice central, BLECharacteristic characteristic) {
  DEBUG_PRINT("Password Characteristic event, written: ");
  DEBUG_PRINTLN(characteristic.uuid());

  DEBUG_PRINT("Admin : Password recu ");
  DEBUG_PRINTLN(characteristic.valueLength());
  sprintf(passwordValue,"%63c",NULL);
  strncpy(passwordValue,(char*)characteristic.value(),characteristic.valueLength());
  passwordValue[characteristic.valueLength()] = '\0';
  password = passwordValue;
  DEBUG_PRINTLN(passwordValue);
}

void blePeripheralConnectHandler(BLEDevice central) {
  // central connected event handler
  DEBUG_PRINT("Connected event, central: ");
  DEBUG_PRINTLN(central.address());
  bleConnected = true;
}

void blePeripheralDisconnectHandler(BLEDevice central) {
  // central disconnected event handler
  DEBUG_PRINT("Disconnected event, central: ");
  DEBUG_PRINTLN(central.address());
  // start advertising
  BLE.advertise();
  bleConnected = false;
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  DEBUG_PRINT("Message arrived [");
  DEBUG_PRINT(topic);
  DEBUG_PRINT("] ");
  for (int i = 0; i < length; i++) 
  {
    DEBUG_PRINT((char)payload[i]);
  }
  DEBUG_PRINTLN();
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
    DEBUG_PRINT("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ArduinoClient-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqttUsername, mqttPassword)) 
    {
      DEBUG_PRINTLN("connected");
      // ... and resubscribe
      client.subscribe(subTopic);
    } else 
    {
      DEBUG_PRINT("failed, rc=");
      DEBUG_PRINT(client.state());
      DEBUG_PRINTLN(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()                    
{
  Serial.begin(BAUD_RATE);    
  strip.begin();
  strip.show(); // Initialize all pixels to 'off' 
}

void loop()                    
{
  BLE.poll();
  long total1 =  cs_4_2.capacitiveSensor(80);  

  capacitiveMeasureCharacteristic.setValue(total1);

  //DEBUG_PRINTLN(total1);
  if(wifiConnectionSuccess == true){
    if (!client.connected()) 
    {
      reconnect();
    }
    client.loop();
  }
  
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
    case INIT:      
      stateSelf = BLE_CONFIGURATION;
      break;
    case BLE_CONFIGURATION:
      if(bleConnected == true)
        bleSkipCycle = 0;
      else
        bleSkipCycle++;
      
      lightConnectionStatus();
      if(configurationDoneBle == true || bleSkipCycle>BLE_SKIP_CYCLE)        
        stateSelf = WIFI_CONNECTION;
      break;
    case WIFI_CONNECTION:
      if(wifiConnectionSuccess == true)              
        stateSelf = IDLE; 
      break;
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
    case INIT:     
      DEBUG_PRINTLN("Enter INIT"); 
      break;
    case BLE_CONFIGURATION:
      setup_ble();
      DEBUG_PRINTLN("Enter BLE_CONFIGURATION"); 
      break;
    case WIFI_CONNECTION:
      DEBUG_PRINTLN("Enter WIFI_CONNECTION"); 
      BLE.stopAdvertise();
      BLE.end();  
      wiFiDrv.wifiDriverDeinit();
      wiFiDrv.wifiDriverInit();
      setup_wifi();
      client.setServer(mqttServer, 1883);
      client.setCallback(callback);
      break;
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
      if((stateSelf==OVERING||stateSelf==TOUCH) || statePartner!=_IDLE){
        stateLight = ON_LIGHT;
      }
      break;
    case ON_LIGHT:
      light();
      if((stateSelf!=OVERING&&stateSelf!=TOUCH) && statePartner==_IDLE){
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
  delay(DELAY_MS);   
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
    
    int valueSelf = (sin(radians((i+lightOffset)*(360/LED_SINUS_LENGHT)))+1)*offsetSelf;          
    rgbcolorSelf = strip.ColorHSV(500, 255, valueSelf); 
          
    int valuePartner = (sin(radians((lightOffset)*(360/LED_SINUS_LENGHT)))+1)*offsetPartner;          
    rgbcolorPartner = strip.ColorHSV(43690, 127, valuePartner); 
    //the final color is the addition of self and partner color
    strip.setPixelColor((i+lightOffset)%LED_COUNT, rgbcolorSelf+rgbcolorPartner);
  }    
  strip.show();
  lightOffset++%16;
}
void lightConnectionStatus(){    
  //go through all leds to light them properly
  for(int i= 0; i<LED_COUNT; i++){
    int hsvValue;
    uint32_t rgbcolor;

    //create an offset depending of the our capacitive sensor
    switch (stateSelf)
    {
      case BLE_CONFIGURATION:    
        if(bleConnected == false){                
          hsvValue = (sin(radians((i+lightOffset)*(360/LED_SINUS_LENGHT)))+1)*10;          
          rgbcolor = strip.ColorHSV(42962, 255, hsvValue); 
        }
        else{
          hsvValue = (sin(radians((lightOffset)*(360/LED_SINUS_LENGHT)))+1)*10;          
          rgbcolor = strip.ColorHSV(42962, 255, hsvValue);           
        }
        break;
      case WIFI_CONNECTION:   
        switch(stateWifi){
          case OFF:
          case CONNECTING:
            hsvValue = (sin(radians((lightOffset)*(360/LED_SINUS_LENGHT)))+1)*10;        
            rgbcolor = strip.ColorHSV(0, 0, hsvValue); 
            break;
          case ON:
            hsvValue = (sin(radians((lightOffset)*(360/LED_SINUS_LENGHT)))+1)*10;        
            rgbcolor = strip.ColorHSV(19478, 150, hsvValue); 
            break;
          case FAIL:
            hsvValue = (sin(radians((lightOffset)*(360/LED_SINUS_LENGHT)))+1)*10;        
            rgbcolor = strip.ColorHSV(5643, 255, hsvValue); 
            break;
        }        
        break;
    }
          
    //the final color is the addition of self and partner color
    strip.setPixelColor((i+lightOffset)%LED_COUNT, rgbcolor);
  }    
  strip.show();
  lightOffset++%16;
}
