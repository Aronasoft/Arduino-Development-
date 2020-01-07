#include <Ticker.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>

#include <SoftwareSerial.h>

#include <DHT.h>
#include <DHT_U.h>

#define DEBUG 0

/* Temperature and Humidity Sensor PIN */
#define DHTPIN 4

/* PIR Sensor PIN */
#define PIRPIN 13

/* Type of DHT Module, Here it is DHT11 */
#define DHTTYPE DHT11

/* Max read time out when the program just 
 *  leave the reading loop
 *  
 *  Must be in multiple of 5, because 5 seconds 
 *  is in global timeperiod 
 */
#define MAXREADTIMEOUT 10

#define MAX_READ_TIMEOUT 10
#define MAX_STUCK_TIMEOUT 10  
#define MAX_CMD_RETRY 5

#define SSID_USER "rpi0w"
#define WIFI_PASSWORD "987654321"

#define NW_NAME "AIRTEL"

#define APN_NAME "airtelgprs.com"

DHT dht(DHTPIN, DHTTYPE);

WiFiClient client;

HTTPClient http;
    
String APN = "airtelgprs.com";

ESP8266WiFiMulti WiFiMulti;

SoftwareSerial GSMSerial(14,12);

uint8_t checkForPDPCOunt = 0;
uint8_t maxGlobalReadTimout = 0;

boolean readTimeoutflag = false;
boolean CMDretryflag = false;

Ticker gsmSerReadTimout;
Ticker gsmRetryMax;

/* This runs first, but only one time */
void setup()
{
  pinMode(PIRPIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  
  GSMSerial.begin(9600); 
  Serial.begin(9600); 
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID_USER, WIFI_PASSWORD);   
  dht.begin();
  
  delay(6000);
    
  Serial.println("DEBUG: MODULE START");
  if(SIM900start() == false)
  {
    if(WiFi.status() != WL_CONNECTED)
    {
      while(1)
      {
        Serial.println("DEBUG: CANT SEND DATA, NO METHOD TO SEND DATA Not Connected to both WiFi and GSM Module");
        digitalWrite(LED_BUILTIN, HIGH);
        delay(1000);
        digitalWrite(LED_BUILTIN, LOW);
        delay(1000);
        yield();

        Serial.println("Retrying .....");
        if(WiFi.status() == WL_CONNECTED)
        {
          if(isModuleConnected() == true){
            break;
          }
          break;
        } else if(isModuleConnected() == true)
        {
          break;
        }
      }
    }
  } else {
    if(WiFi.status() == WL_CONNECTED)
    {
      //TODO: DISABLE GSM FOR LOW POWER USE ETC.
    }
  }
  
  Serial.println(F("DEBUG: STARTING PERSON COUNTING"));
  digitalWrite(LED_BUILTIN, LOW);
}

/* Main loop of the Program */
void loop()
{
  float dht11_humidity = 0;
  float dht11_temp = 0;

  yield();
  
  if(!digitalRead(PIRPIN))
  {   
    while(!digitalRead(PIRPIN))
    { 
      yield();
    }

    dht11_humidity = dht.readHumidity();
    dht11_temp = dht.readTemperature();
    Serial.print(F("DEBUG: TEMPERATURE: "));
    Serial.println(dht11_temp);
    Serial.print(F("DEBUG: HUMIDITY: "));
    Serial.println(dht11_humidity);

    String temperatureString = String(dht11_temp);
    String humidityString = String(dht11_humidity);
    
    String data = "http://iotapi.aronasoft.com/helpers/functions.php?type=arduinoApi&count=5&temp="+temperatureString+"&humidity="+humidityString;

    Serial.println(data);
    
    if ((WiFi.status() == WL_CONNECTED))
    { 
      sendDataViaWiFi(data);
    }
    else {   
      sendDataViaGPRS(data);
    }
  }
}

int GetGSMResponse(char *GSMResponseBuff, uint8_t maxSerialReadRetryCount)
{
  boolean checkModule = false;
  uint8_t retryCount = 0;
  uint8_t respBuffCount = 0;
  char recvChar = 0;
  
  maxGlobalReadTimout = 0;

  gsmSerReadTimout.attach(MAX_READ_TIMEOUT, gsmSerialReadTimeoutHandler);
  while(respBuffCount == 0 && retryCount < maxSerialReadRetryCount) 
  {     
    yield();
    
    while(GSMSerial.available() > 0)
    {
      recvChar = GSMSerial.read();
      GSMResponseBuff[respBuffCount] = recvChar;
      respBuffCount++;
      if(respBuffCount == 199)
      {
        break;
      }
      yield();
      delay(10);
    }
    if(readTimeoutflag == true)
    {
      readTimeoutflag = false;
      gsmSerReadTimout.detach();
      break;
    }
  }
  
  if(respBuffCount == 0)
  {
    delay(500);
    if(isModuleConnected() == false)
    {
      Serial.println(F("DEBUG: Module Not Dead, GSM Problem?"));
      Serial.println(F("DEBUG: Switching to Sanity check, wait for less than Minute"));
      //TODO: DO SOMETHING WHEN MODULE IS NOT DEAD
    }
  } else {
    return respBuffCount;
  }
}

void gsmSerialReadTimeoutHandler(void)
{
  readTimeoutflag = true;
}
void gsmRETRYHandler(void)
{
  CMDretryflag = true;
}

boolean checkForResponse(char *expectedResponse, uint8_t maxSerialReadRetryCount)
{
  int CharacterCountGSMResponse = 0;
  char GSMResponseBuff[200] = {0};
  
  CharacterCountGSMResponse = GetGSMResponse(GSMResponseBuff, maxSerialReadRetryCount);

#ifdef DEBUG 1
  Serial.println(F("DEBUG: #########################"));
  
  Serial.print(F("DEBUG: CHARACTER RECEIVED="));
  Serial.println(CharacterCountGSMResponse);
  
  Serial.println(F("----------------------------------"));

  Serial.println(F("DEBUG: RESPONSE RECEIVED="));
  Serial.println(GSMResponseBuff);
  
  Serial.println(F("----------------------------------"));
  
  Serial.print(F("DEBUG: RESPONSE EXPECTED="));
  Serial.println(expectedResponse);
#endif

  if(strstr(GSMResponseBuff, expectedResponse) > 0)
  {
#ifdef DEBUG 1
    Serial.println(F("DEBUG: RESPONSE MATCH"));
    Serial.println(F("DEBUG: #########################"));
    Serial.println(F("\r\n"));
#endif
    return true;
  } else {
#ifdef DEBUG 1
    Serial.println(F("DEBUG: ARBITRARY RESPONSE"));
    Serial.println(F("DEBUG: RESPONSE DO NOT MATCH"));
    Serial.println(F("DEBUG: #########################"));
    Serial.println(F("\r\n"));
#endif
    return false;
  }
}

/*
 * Sends and retrys an at command any number of time
 * as specified in the parameter maximumATCommandRetryCount
 * 
 * maxSerialReadRetryCount is the number of retry it will 
 * make for read once it sends the at command.
 * This parameter is passed to next function for
 * aquiring the response checkForResponse
 * 
 */
  
boolean sendAndRetryATCommand(char *ATCommand, char *expectedResponse, uint8_t maxSerialReadRetryCount, uint8_t maximumATCommandRetryCount)
{
  boolean matchResponse = false;
  uint8_t retryCount = 0;
  uint8_t writeCount = 0;
    //////////////////////////////-----------------------------------------------------------
  //gsmSerReadTimout.attach(MAX_READ_TIMEOUT, gsmSerialReadTimeoutHandler);
    
  while(retryCount < maximumATCommandRetryCount)
  { 
    yield();
    Serial.print(F("DEBUG: "));
    Serial.println(ATCommand);
    Serial.print(F("DEBUG: Debug Count in send and retry at command="));
    Serial.println(retryCount);
    Serial.println(F("DEBUG: ***********************"));

    GSMSerial.println(ATCommand);
    matchResponse = checkForResponse(expectedResponse, maxSerialReadRetryCount);
    if(matchResponse == false) {
      if(retryCount == maximumATCommandRetryCount - 1)
      {
        return false;
      }
      flushArbitraryWaitingBytes();
      retryCount++;
      continue;
    } else if(matchResponse == true)
    {
      return true;
      break;
    }

    /*if(readTimeoutflag == true)
    {
      readTimeoutflag = false;
      gsmSerReadTimout.detach();
      break;
    }*/
  }
}

boolean SIM900start(void)
{
  if(isModuleConnected() == false)
  {
    
    return false;
    //TODO: CHECK WIFI? PUT UP WARNING LED?
  } else {
    Serial.println(F("DEBUG: GSM INITIALIZING"));
    GSMSetup();
  
    Serial.println(F("DEBUG: CHECK GSM REGISTRATION"));
    checkNetworkRegistration();

    Serial.println(F("DEBUG: SETTING UP GPRS"));
    setupGPRS();
  
    Serial.println(F("DEBUG: SETTING UP TCP/IP"));
    setupTCP();

    return true;
  }
}

/*
 * Sim ple check and setup for GSM 
 */
void GSMSetup(void)
{ 
  sendAndRetryATCommand("ATE0", "OK", 1, 1);
  flushArbitraryWaitingBytes();
  
  sendAndRetryATCommand("AT", "OK", 1, 1);
  flushArbitraryWaitingBytes();
  
  sendAndRetryATCommand("AT+CPIN?", "READY", 1, 1);
  flushArbitraryWaitingBytes();
  
  sendAndRetryATCommand("AT+CIPSPRT=0", "OK", 1, 1);
  flushArbitraryWaitingBytes();
}

/*
 * FOr checking of the network registration
 * and if it is connected to the network. 
 * BY default it should connect to the 2 GSM Network
 * automatically. If it doesnt happen specific network id is needed 
 * to implement the AT COMMAND for network registration.
 */
void checkNetworkRegistration(void)
{
  flushArbitraryWaitingBytes();
  String STRING_APNAT_Command = "AT+CSTT=\""+APN+"\",\"\",\"\"";

  char APNAT_Command[50] = {0};

  STRING_APNAT_Command.toCharArray(APNAT_Command, 50);

  
  sendAndRetryATCommand("AT+COPS?", NW_NAME, 1, 1);                                    //TODO
  flushArbitraryWaitingBytes();
  
  sendAndRetryATCommand("AT+CREG?", ": ", 1, 1);                                     //TODO
  flushArbitraryWaitingBytes();

  gsmRetryMax.attach(MAX_CMD_RETRY, gsmRETRYHandler);
  while(sendAndRetryATCommand("AT+CSTT?", APN_NAME, 1, 1) == false)
  {
    flushArbitraryWaitingBytes();   
  //sendAndRetryATCommand("AT+CSTT=\"airtelgprs.com\",\"\",\"\"", "airtelgprs.com", 1, 1);
    sendAndRetryATCommand(APNAT_Command, APN_NAME , 1, 1);
    if(CMDretryflag == true)
    {
      CMDretryflag = false;
      gsmRetryMax.detach();
      break;
    }
  }
  
  sendAndRetryATCommand("AT+CSTT?", "airtelgprs.com", 1, 1);
  flushArbitraryWaitingBytes();
}

/* 
 *  Check if module is replying to AT Commands.
 *  Returns true if module connected.
 *  Otherwise false.  
 */
boolean isModuleConnected(void)
{
  char respBuff[10] = {0};
  uint8_t respBuffCount = 0;
  GSMSerial.println(F("AT"));
  delay(200);
  while(GSMSerial.available() > 0)
  {
    respBuff[respBuffCount] = GSMSerial.read();
    respBuffCount++;
  }
  if(respBuffCount > 3) //Could compare it to 0, I took 2
  {
    if(strstr(respBuff, "OK") > 0)
    {
      return true;
    } 
  } else if(respBuffCount == 0) {
    return false;
  }
}

/*
 * Resets te module
 * 
 * No idea if it works
 */
void resetGSMBoard(void)
{
 GSMSerial.println("AT+CFUN=4,0,1");
}

/*
 * Always call after network registration
 * that is, after this funtion - checkNetworkRegistration().
 * Otherwise bad things happen
 * 
 */
void setupGPRS(void)
{
  String STRING_APNAT_Command = "AT+CGDCONT=1,\"IP\",\""+APN+"\"";

  char APNAT_Command[50] = {0};

  STRING_APNAT_Command.toCharArray(APNAT_Command, 50);
  
  flushArbitraryWaitingBytes();
  
  sendAndRetryATCommand("AT+CIPSHUT", "OK", 1, 1);
  
  flushArbitraryWaitingBytes();
  
  if(sendAndRetryATCommand("AT+CGATT?", ": 1", 1, 1) == true)
  {
    sendAndRetryATCommand("AT+CGATT=0", "OK", 1, 1);
  }
  flushArbitraryWaitingBytes();

  if(sendAndRetryATCommand("AT+CGDCONT?", APN_NAME, 1, 1) == false)
  {
    sendAndRetryATCommand(APNAT_Command, "OK", 1, 1);
  }
  flushArbitraryWaitingBytes();
  
  if(sendAndRetryATCommand("AT+CGACT?", "1,0", 1, 1) == true) 
  {
    flushArbitraryWaitingBytes();
    while(sendAndRetryATCommand("AT+CGACT=1", "ERROR", 1, 1) == true)
    {
      flushArbitraryWaitingBytes();
      if(sendAndRetryATCommand("AT+CGACT?", "1,1", 1, 1) == true)
      {
        break;
      }
    }
  }
  flushArbitraryWaitingBytes();

  if(sendAndRetryATCommand("AT+CGATT?", ": 0", 1, 1) == true)
  {
    sendAndRetryATCommand("AT+CGATT=1", "OK", 1, 1);
    flushArbitraryWaitingBytes();
  }
}

void flushArbitraryWaitingBytes(void)
{
  char recvVar = 0;
  uint8_t noOfTimes = 0;
  while(noOfTimes < 40)
  {
    while(GSMSerial.available() > 0)
    {
      recvVar = GSMSerial.read();
    }
    noOfTimes++;
  }
}

void setupTCP(void)
{
  flushArbitraryWaitingBytes();
  
  if(sendAndRetryATCommand("AT+CIPMUX?", ": 0", 1, 1) == false)
  {
    Serial.println(F("DEBUG: IN CIPMUX"));
    flushArbitraryWaitingBytes();
    sendAndRetryATCommand("AT+CIPMUX=0", "OK", 1, 1);
    flushArbitraryWaitingBytes();
  }
  
  while(sendAndRetryATCommand("AT+CIPSTATUS", "PDP DEACT", 1, 1) == true)
  {
    setupGPRS();
  }
  
  flushArbitraryWaitingBytes();
  sendAndRetryATCommand("AT+CIPTKA=1", "ERROR", 1,1);
  flushArbitraryWaitingBytes();
}

/*
 * Sending data through GPRS. 
 * It first check for the connection
 * If the connection is already made
 * it start sending data and if PDP is deactivated
 * it will connect to PDP Then send the data
 */
void sendDataViaGPRS(String data)
{
  uint8_t sendDataRetry = 0;
  //String GSMdata = "GET " + data + "&sentby=G";

  String GSMData = "GET " + data;
  
  //if(sendAndRetryATCommand("AT+CIPSTART=\"TCP\",\"iotapi.aronasoft.com\",\"80\"", "DEACT", 1,1) == true)
  if(sendAndRetryATCommand("AT+CIPSTATUS", "DEACT", 1,1) == true)
  {
    setupGPRS();
  }

  while(sendAndRetryATCommand("AT+CIPSTART=\"TCP\",\"iotapi.aronasoft.com\",\"80\"", "ALREADY", 1,1) != true)
  {
    if(sendAndRetryATCommand("AT+CIPSTATUS", "CONNECT OK", 1, 1) == true)
    {
      flushArbitraryWaitingBytes();
      break;
    } else {
      continue;
    }
  }

  Serial.println(F("DEBUG: SENDING DATA"));
  Serial.println(data);
  GSMSerial.println(F("AT+CIPSEND"));
  delay(500);
  GSMSerial.println(GSMData);
  GSMSerial.println((char)26);
    
  while(1)
  {
    //sending
    if(sendAndRetryATCommand("\r\n", "success", 1,1) == true)
    {
      break;
    } else {
      sendDataRetry++;
    }
    if(sendDataRetry == 3)
    {
      break;
    }
    yield();
  }
  
  flushArbitraryWaitingBytes();
}

void sendDataViaWiFi(String data)
{
  //String wifidata = data + "&sentby=W";
//  http.begin(client, wifidata);

  http.begin(client, data);
  int httpCode = http.GET();
  
  if (httpCode > 0){   
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
  
      // file found at server
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
      String payload = http.getString();
      Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}
