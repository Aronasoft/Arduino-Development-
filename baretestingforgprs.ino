#include <SoftwareSerial.h>


SoftwareSerial GSMSerial(8, 7);

void setup() {

  /*
   * GSM Serial for sending command to GSM module
   */
  GSMSerial.begin(9600); 

  /*
   * Serial for computer serial port
   */
  Serial.begin(9600);

  /*
   * Arbitrary delay
   */
  delay(3000);

  GSMSerial.println("AT+CSQ");
  delay(2000); 
  showSerialData();
  
  /*
   * Checking for Sim insert retuen value will be 0,1
   */
  GSMSerial.println("AT+CSMINS?");
  delay(2000); 
  showSerialData();

  /*
   * Check for module if its ready response READY
   */
  GSMSerial.println("AT+CPIN?");
  delay(2000); 
  showSerialData();

  /*
   * For SETTING GSM to '>' prompt 
   */
  GSMSerial.println("AT+CIPSPRT=0");
  delay(2000); 
  showSerialData();

  
  /*
   * For detaching GPRS 
   */
  GSMSerial.println("AT+CGATT=0");
  delay(2000); 
  showSerialData();
  
  /*
   *SHUT TCP connection
   */
  GSMSerial.println("AT+CIPSHUT");
  delay(2000); 
  showSerialData();

  /*
   * INSTEAD of xxxxxxx, put the APN provided by the 
   * service provider. APN should be either checked 
   * on the internet or on a cellphone so as to make sure
   * using this APN the GPRS works. APN Could be of any 
   * length and could contain special characters
   * 
   * NOTES-> REPLACE airtelgprs.com with your APN provided by service provider. 
   */
   
  GSMSerial.println("AT+CSTT=\"airtelgprs.com\",\"\",\"\"");
  delay(2000); 
  showSerialData();


  GSMSerial.println("AT+CGDCONT?");
  delay(2000); 
  showSerialData();
  
  /*
   * INSTEAD of xxxxxxx, put the APN provided by the 
   * service provider. APN should be either checked 
   * on the internet or on a cellphone so as to make sure
   * using this APN the GPRS works. APN Could be of any 
   * length and could contain special characters
   */
   
  GSMSerial.println("AT+CGDCONT=1, \"IP\",\"airtelgprs.com\"");
  delay(2000); 
  showSerialData();

  /*
   * for Attaching to PDP Context
   */
  GSMSerial.println(F("AT+CGACT=1"));
  delay(10000); 
  showSerialData();

  /*
   * For checking PDP Context
   * 
   * Expected RESPONSE: CGACT: 1,1
   * 
   * If PDP not activated then RESPONSE: CGACT: 1,0
   */
  GSMSerial.println(F("AT+CGACT?"));
  delay(2000); 
  showSerialData();

  /*
   * Attach GPRS
   */
  GSMSerial.println(F("AT+CGATT=1"));
  delay(2000); 
  showSerialData();

  /*
   * For status of TCP Connection to Server/Site
   * 
   * Expected: IP INTIAL OR START
   */
  GSMSerial.println(F("AT+CIPSTATUS"));
  delay(2000); 
  showSerialData();

  /*
   * For setting single connection
   */
  GSMSerial.println(F("AT+CIPMUX=0"));
  delay(2000); 
  showSerialData();

  /*
   * For making the connection to the server
   * 
   * Expected Response: CONNECT OK OR ALREADY CONNECTED
   */
  //for (uint8_t i = 0; i<3; i++) {
    Serial.println(F("IN CIPSTART"));
    GSMSerial.println(F("AT+CIPSTART=\"TCP\",\"iotapi.aronasoft.com\",\"80\""));
    delay(2000); 
    showSerialData();
    delay(2000);  
  //}
  
  /*
   * For status of TCP Connection to Server/Site
   * 
   * Expected: CONNECT OK
   */
  GSMSerial.println(F("AT+CIPSTATUS"));
  delay(2000); 
  showSerialData();

  /*
   * For sending data
   */
  Serial.print(F("DEBUG: SENDING DATA: "));
  Serial.println(F("GET http://iotapi.aronasoft.com/helpers/functions.php?type=arduinoApi&count=1&temp=30.0&humidity=50.5"));
  GSMSerial.println(F("AT+CIPSEND"));
  delay(500);
  GSMSerial.println(F("GET http://iotapi.aronasoft.com/helpers/functions.php?type=arduinoApi&count=1&temp=25.0&humidity=50.5"));
  GSMSerial.println((char)26);//sending
  delay(5000);
  showSerialData();
  
  //GSMSerial.println();
  //showSerialData();

}

void loop() {
  // put your main code here, to run repeatedly:

}

/*
 * For sending received GSM response to deial moniter
 */
void showSerialData(void) {
  char recvByte = 0;
  
  while(GSMSerial.available() > 0)
  {
    recvByte = GSMSerial.read();
    Serial.print(recvByte);
  }
  Serial.println("");
}
