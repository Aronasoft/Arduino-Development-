#include <DHT.h>
#include <DHT_U.h>

#include <TimerOne.h>
#include <SoftwareSerial.h>

/* Temperature and Humidity Sensor PIN */
#define DHTPIN 2

/* PIR Sensor PIN */
#define ENTRY 10

/* Type of DHT Module, Here it is DHT11 */
#define DHTTYPE DHT11

/* Max read time out when the program just 
 *  leave the reading loop
 *  
 *  Must be in multiple of 5, because 5 seconds 
 *  is in global timeperiod 
 */
#define MAXREADTIMEOUT 10

/* Spawning a class */
DHT dht(DHTPIN, DHTTYPE);

#define NW_NAME "AIRTEL"


#define APN_NAME "airtelgprs.cpm"
String APN = "airtelgprs.com";

SoftwareSerial GSMSerial(8, 7);
uint8_t checkForPDPCOunt = 0;
uint8_t maxGlobalReadTimout = 0;

/* This runs first, but only one time */
void setup()
{
  pinMode(ENTRY, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  GSMSerial.begin(9600); 
  Serial.begin(9600);    
  delay(1000);

  boolean MatchResponse = false;

  digitalWrite(LED_BUILTIN, HIGH);
  dht.begin();

  //sendAndRetryATCommand("AT+CUSD=1,\"*123#\"", "OK", 500, 1, 1); 
  
   
  Serial.println(F("DEBUG: GSM INITIALIZING"));
  GSMSetup();
  
  Serial.println(F("DEBUG: CHECK GSM REGISTRATION"));
  checkNetworkRegistration();

  Serial.println(F("DEBUG: SETTING UP GPRS"));
  setupGPRS();
  
  Serial.println(F("DEBUG: SETTING UP TCP/IP"));
  setupTCP();

  Serial.println(F("DEBUG: STARTING PERSON COUNTING"));

  digitalWrite(LED_BUILTIN, LOW);
  
  Timer1.initialize(5000000);
  Timer1.attachInterrupt(incrementPeriod);
}

/* Main loop of the Program */
void loop()
{
  int personCount = 0;
  float dht11_humidity = 0;
  float dht11_temp = 0;
  
  while(1){
    flushArbitraryWaitingBytes();
    if(!digitalRead(ENTRY))
    {
      while(!digitalRead(ENTRY))
      { 
      }
      digitalWrite(LED_BUILTIN, HIGH);   
      dht11_humidity = dht.readHumidity();
      dht11_temp = dht.readTemperature();
      Serial.print(F("DEBUG: TEMPERATURE: "));
      Serial.println(dht11_temp);
      Serial.print(F("DEBUG: HUMIDITY: "));
      Serial.println(dht11_humidity);
      Serial.println(F("DEBUG: IN SENDING LOOP"));
      sendDataHandler(1, dht11_temp, dht11_humidity);
      digitalWrite(LED_BUILTIN, LOW);
    }
    
    if(checkForPDPCOunt == 120)
    {
      Timer1.stop();
      checkForPDPCOunt = 0;
      if(sendAndRetryATCommand("AT+CGACT?", "1,0", 5000, 1, 1) == true)
      {
        setupGPRS();
      }
      Timer1.start();
    }
  }
}

void send2Server(void)
{  
  char strA[200] = {0};

  int personCount = 1;
  char personCountString[2] = {0};
  
  while(personCount < 100)
  {
    String personCountInString = String(personCount);

    GSMSerial.println(F("AT+CIPSTART=\"TCP\",\"iotapi.aronasoft.com\",\"80\""));//start up the connection
    delay(3000);
    ShowSerialData();

    String str= "GET http://iotapi.aronasoft.com/index.php?value="+personCountInString;
    
    GSMSerial.println(F("AT+CIPSEND"));
    delay(500);
    GSMSerial.println(str);
    GSMSerial.println((char)26);//sending
    delay(13000);
    ShowSerialData();
    personCount++;
    delay(1000);

    Serial.println("REDOING");
  }
} 

void ShowSerialData(void)
{
  uint8_t respBuffCount = 0;
  char recvSerialBuff[200] = {0};

  while(GSMSerial.available() > 0)
  {
    recvSerialBuff[respBuffCount] = GSMSerial.read();
    respBuffCount++;
    if(respBuffCount == 199)
      break;
  }
  Serial.println(recvSerialBuff);
}

int GetGSMResponse(char *GSMResponseBuff, uint8_t maxSerialReadRetryCount)
{
  boolean checkModule = false;
  uint8_t retryCount = 0;
  uint8_t respBuffCount = 0;
  char recvSerialBuff[200] = {0};
  
  maxGlobalReadTimout = 0;
  
  while(respBuffCount == 0 && retryCount < maxSerialReadRetryCount) 
  {        
    Serial.println(F("DEBUG: ***********************----------------------------------------------"));
    Serial.print(F("DEBUG: Debug Count in gsm response="));
    Serial.println(retryCount);
    Serial.println(F("DEBUG: ***********************----------------------------------------------"));
    while(GSMSerial.available() > 0)
    {
      recvSerialBuff[respBuffCount] = GSMSerial.read();
      GSMResponseBuff[respBuffCount] = recvSerialBuff[respBuffCount];
      respBuffCount++;
      if(respBuffCount == 199)
      {
        break;
      }
      delay(20);
    }
    if(maxGlobalReadTimout > 10)
    {
      maxGlobalReadTimout = 0;
      break;
    }
  }

  if(respBuffCount == 0)
  {
    delay(500);
    if(retryCount == (maxSerialReadRetryCount-1))
    {
      checkModule = isModuleDead();
      if(checkModule == false)
      {
        Serial.println(F("DEBUG: Module Not Dead, GSM Problem?"));
        Serial.println(F("DEBUG: Switching to Sanity check, wait for less than Minute"));
        //TODO: DO SOMETHING WHEN MODULE IS NOT DEAD
      }/* else if(respBuffCount == 0) {
        Serial.println("DEBUG: Module dead, GSM reset");
        delay(10000);
        resetGSMBoard();
        //TODO: WAIT FOR SOME TIME AND THEN RESET GSM MODULE
      }*/
    }
    retryCount++;
    //continue;
   } else {
    return respBuffCount;
   }
}

boolean checkForResponse(char *expectedResponse, uint8_t maxSerialReadRetryCount)//, uint8_t ERROR_CODE)
{
  int CharacterCountGSMResponse = 0;
  char GSMResponseBuff[200] = {0};
  
  CharacterCountGSMResponse = GetGSMResponse(GSMResponseBuff, maxSerialReadRetryCount);
  
  Serial.println(F("DEBUG: #########################"));
  
  Serial.print(F("DEBUG: CHARACTER RECEIVED="));
  Serial.println(CharacterCountGSMResponse);
  
  Serial.println(F("----------------------------------"));

  Serial.println(F("DEBUG: RESPONSE RECEIVED="));
  Serial.println(GSMResponseBuff);
  
  Serial.println(F("----------------------------------"));
  
  Serial.print(F("DEBUG: RESPONSE EXPECTED="));
  Serial.println(expectedResponse);
  

  if(strstr(GSMResponseBuff, expectedResponse) > 0)
  {
    Serial.println(F("DEBUG: RESPONSE MATCH"));
    Serial.println(F("DEBUG: #########################"));
    Serial.println(F("\r\n"));
    return true;
  } else {
    Serial.println(F("DEBUG: ARBITRARY RESPONSE"));
    Serial.println(F("DEBUG: RESPONSE DO NOT MATCH"));
    flushArbitraryWaitingBytes();
    Serial.println(F("DEBUG: #########################"));
    Serial.println(F("\r\n"));
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
  
boolean sendAndRetryATCommand(char *ATCommand, char *expectedResponse, int delayForResponse, uint8_t maxSerialReadRetryCount, uint8_t maximumATCommandRetryCount)
{
  boolean matchResponse = false;
  uint8_t retryCount = 0;
  
  while(retryCount < maximumATCommandRetryCount)
  { 
    Serial.print(F("DEBUG: "));
    Serial.println(ATCommand);
    Serial.print(F("DEBUG: Debug Count in send and retry at command="));
    Serial.println(retryCount);
    Serial.println(F("DEBUG: ***********************"));
    GSMSerial.println(ATCommand);
    delay(delayForResponse);
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
  }
}

void GSMSetup(void)
{
  char * ATCommand = 0;
  flushArbitraryWaitingBytes();
  delay(5000);
  
  sendAndRetryATCommand("AT+CSMINS?", "0,1", 300, 1, 1);
  flushArbitraryWaitingBytes();
  
  sendAndRetryATCommand("ATE0", "OK", 300, 1, 1);
  flushArbitraryWaitingBytes();
  
  sendAndRetryATCommand("AT", "OK", 300, 1, 1);
  flushArbitraryWaitingBytes();
  
  sendAndRetryATCommand("AT+CPIN?", "READY", 300, 1, 1);
  flushArbitraryWaitingBytes();
  
  sendAndRetryATCommand("AT+CIPSPRT=0", "OK", 300, 1, 1);
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

  
  sendAndRetryATCommand("AT+COPS?", NW_NAME, 300, 1, 1);                                    //TODO
  flushArbitraryWaitingBytes();
  
  sendAndRetryATCommand("AT+CREG?", ": ", 300, 1, 1);                                     //TODO
  flushArbitraryWaitingBytes();

  while(sendAndRetryATCommand("AT+CSTT?", "airtelgprs.com", 800, 1, 1) == false)
  {
    flushArbitraryWaitingBytes();   
  //sendAndRetryATCommand("AT+CSTT=\"airtelgprs.com\",\"\",\"\"", "airtelgprs.com", 800, 1, 1);
    sendAndRetryATCommand(APNAT_Command, APN_NAME , 800, 1, 1);
    flushArbitraryWaitingBytes();
  }
  
  sendAndRetryATCommand("AT+CSTT?", "airtelgprs.com", 800, 1, 1);
  flushArbitraryWaitingBytes();
}

/* 
 *  Check if module is replying to AT Commands.
 *  Returns false if module is not dead and
 *  replying to AT Commands.  
 */
boolean isModuleDead(void)
{
  char cmdRespBuff[10] = {0};
  uint8_t respBuffCount = 0;
  GSMSerial.println(F("AT"));
  delay(200);
  while(GSMSerial.available() > 0)
  {
    cmdRespBuff[respBuffCount] = GSMSerial.read();
    respBuffCount++;
  }
  if(respBuffCount > 2) //Could compare it to 0, I took 2
  {
    if(strstr(cmdRespBuff, "OK") > 0)
    {
      return false;
    } 
  } else if(respBuffCount == 0) {
    return true;
  }
}

/*
 * Resets te module
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
  flushArbitraryWaitingBytes();
  sendAndRetryATCommand("AT+CIPSHUT", "OK", 500, 1, 1);
  
  flushArbitraryWaitingBytes();
  if(sendAndRetryATCommand("AT+CGATT?", ": 1", 800, 1, 1) == true)
  {
    flushArbitraryWaitingBytes();
    sendAndRetryATCommand("AT+CGATT=0", "OK", 800, 1, 1);
    flushArbitraryWaitingBytes();
  }

  if(sendAndRetryATCommand("AT+CGDCONT?", "airtelgprs.com", 800, 1, 1) == false)
  {
    flushArbitraryWaitingBytes();
    sendAndRetryATCommand("AT+CGDCONT=1, \"IP\",\"airtelgprs.com\"", "OK", 800, 1, 1);
    flushArbitraryWaitingBytes();
  }
  flushArbitraryWaitingBytes();
  //delay(5000);
  
  if(sendAndRetryATCommand("AT+CGACT?", "1,0", 800, 1, 1) == true) 
  {
    flushArbitraryWaitingBytes();
    while(sendAndRetryATCommand("AT+CGACT=1", "ERROR", 800, 1, 1) == true)
    {
      flushArbitraryWaitingBytes();
      if(sendAndRetryATCommand("AT+CGACT?", "1,1", 800, 1, 1) == true)
      {
        break;
      }
    }
  }
  flushArbitraryWaitingBytes();

  if(sendAndRetryATCommand("AT+CGATT?", ": 0", 800, 1, 1) == true)
  {
    sendAndRetryATCommand("AT+CGATT=1", "OK", 1000, 1, 1);
    flushArbitraryWaitingBytes();
  }

  //sendAndRetryATCommand("AT+CIICR", "OK", 2000, 1, 1);
}

void flushArbitraryWaitingBytes(void)
{
  char arVar = 0;
  uint8_t noOfTimes = 0;
  while(noOfTimes < 100)
  {
    while(GSMSerial.available() > 0)
    {
      arVar = GSMSerial.read();
    }
    noOfTimes++;
  }
}

void setupTCP(void)
{
  flushArbitraryWaitingBytes();
  
  if(sendAndRetryATCommand("AT+CIPMUX?", ": 0", 500, 1, 1) == false)
  {
    Serial.println(F("DEBUG: IN CIPMUX"));
    flushArbitraryWaitingBytes();
    sendAndRetryATCommand("AT+CIPMUX=0", "OK", 500, 1, 1);
    flushArbitraryWaitingBytes();
  }
  
  while(sendAndRetryATCommand("AT+CIPSTATUS", "PDP DEACT", 1000, 1, 1) == true)
  {
    setupGPRS();
  }
  
  flushArbitraryWaitingBytes();
  sendAndRetryATCommand("AT+CIPTKA=1", "ERROR", 500, 1,1);
  flushArbitraryWaitingBytes();
}

/*
 * Sending data through GPRS. 
 * It first check for the connection
 * If the connection is already made
 * it start sending data and if PDP is deactivated
 * it will connect to PDP Then send the data
 */
void sendDataHandler(int personCount, float temp, float humidity)
{
  uint8_t sendDataRetry = 0;
  
  String personCountInString = String(personCount);
  String temperatureString = String(temp);
  String humidityString = String(humidity);
  String data = "GET http://iotapi.aronasoft.com/helpers/functions.php?type=arduinoApi&count="+personCountInString+"&temp="+temperatureString+"&humidity="+humidityString;

  //http://iotapi.aronasoft.com/helpers/functions.php?type=arduinoApi&count=1&temp=30.0&humidity=50.5
  
  flushArbitraryWaitingBytes();

  if(sendAndRetryATCommand("AT+CIPSTART=\"TCP\",\"iotapi.aronasoft.com\",\"80\"", "DEACT", 500, 1,1) == true)
  {
    setupGPRS();
  }

  while(sendAndRetryATCommand("AT+CIPSTART=\"TCP\",\"iotapi.aronasoft.com\",\"80\"", "ALREADY", 500, 1,1) != true)
  {
    if(sendAndRetryATCommand("AT+CIPSTART=\"TCP\",\"iotapi.aronasoft.com\",\"80\"", "DEACT", 500, 1,1) == true)
    {
      setupGPRS();
    }
    flushArbitraryWaitingBytes();
    if(sendAndRetryATCommand("AT+CIPSTATUS", "CONNECT OK", 500, 1, 1) == true)
    {
      flushArbitraryWaitingBytes();
      break;
    } else {
      continue;
    }
  }

  while(1)
  {
    Serial.println(F("DEBUG: SENDING DATA"));
    Serial.println(data);
    GSMSerial.println(F("AT+CIPSEND"));
    delay(500);
    GSMSerial.println(data);
    GSMSerial.println((char)26);//sending
    if(sendAndRetryATCommand("\r\n", "SEND OK", 100, 1,1) == true)
    {
      break;
    } else {
      sendDataRetry++;
    }
    if(sendDataRetry == 3)
    {
      break;
    }
    
  }
  
  flushArbitraryWaitingBytes();
}

/* Timer1 interrupt handler 
 *  
 *  This increment the check variable for PDP
 *  and the timeout for read when reading froms GSM serial
 *  
 */
void incrementPeriod(void)
{
  //Serial.println(F("DEBUG: IN TIMER HANDLER"));
  //Serial.print(F("DEBUG: periodOfSendingData: "));
  //Serial.println(periodOfSendingData);
  checkForPDPCOunt++;
  maxGlobalReadTimout++;
}
