
/*
MIT License

Copyright (c) 2022 George Hofmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

  Pin assignments:
   01  TX0
   02  switch for configuration via bluetooth
   03  RX0
   04  GPIO
   05  GPIO
   12  A+ stepper 1
   13  A- stepper 1
   14  B+ stepper 1
   15  B- stepper 1
   16  A+ stepper 2
   17  A- stepper 2
   18  B+ stepper 2
   19  B- stepper 2
   21  A+ stepper 3
   22  A- stepper 3
   23  B+ stepper 3
   25  B- stepper 3
   26  A+ stepper 4
   27  A- stepper 4
   32  B+ stepper 4
   33  B- stepper 4
   34  switch 1 requires pullup on board
   35  switch 2 requires pullup on board
   36  switch 3 requires pullup on board
   39  switch 4 requires pullup on board
   */

#include <iostream>>
#include <Preferences.h>
#include <PubSubClient.h>
#include "WiFi.h"
#include <BluetoothSerial.h>
#include "StepperMRTO.h"
//#define testing

using namespace std;

Preferences myPrefs;
char *deviceSpace[] = {"d1", "d2", "d3", "d4"};

// wifi
WiFiClient espClient;
String SSID;
String wifiPassword;

// mqtt
String mqttServer;
String mqttNode;
String mqttChannel;
char mqttchannel[50];
String turnoutTopic;
PubSubClient client(espClient);

// Bluetooth
BluetoothSerial BTSerial;
String BTname;
String BTpassword;
String pwCandidate;
String pwtest;

String nodeName;

// constants and variables - turnouts

uint16_t const numDevices = 4;
String devName[numDevices];             // TBD
uint16_t const stepsPerRevolution = 20; // number of steps per revolution
uint16_t const NOMINAL_SPEED = 1000;
uint16_t const NOMINAL_STROKE = 500;
uint16_t const NOMINAL_TORQUE_INTERVAL = 1000;

// motor pins
uint16_t const APlus1Pin = 12;
uint16_t const AMinus1Pin = 13;
uint16_t const BPlus1Pin = 14;
uint16_t const BMinus1Pin = 15;
uint16_t const APlus2Pin = 16;
uint16_t const AMinus2Pin = 17;
uint16_t const BPlus2Pin = 18;
uint16_t const BMinus2Pin = 19;
uint16_t const APlus3Pin = 21;
uint16_t const AMinus3Pin = 22;
uint16_t const BPlus3Pin = 23;
uint16_t const BMinus3Pin = 25;
uint16_t const APlus4Pin = 26;
uint16_t const AMinus4Pin = 27;
uint16_t const BPlus4Pin = 32;
uint16_t const BMinus4Pin = 33;

StepperMRTO myStepper[] =
    {StepperMRTO(stepsPerRevolution, APlus1Pin, AMinus1Pin, BPlus1Pin, BMinus1Pin),
     StepperMRTO(stepsPerRevolution, APlus2Pin, AMinus2Pin, BPlus2Pin, BMinus2Pin),
     StepperMRTO(stepsPerRevolution, APlus3Pin, AMinus3Pin, BPlus3Pin, BMinus3Pin),
     StepperMRTO(stepsPerRevolution, APlus4Pin, AMinus4Pin, BPlus4Pin, BMinus4Pin)};

// switch pins for manual control, these must have pullup resistors
uint16_t const switchPin[4] = {34, 35, 36, 39};

bool returnToMenu = false; // for actuation feature in menu TBD

/*****************************************************************************/
void setup()
{
  byte myVal;

  Serial.begin(115200);
  pinMode(2, INPUT_PULLUP); // this is used for resetting Bluetooth

  // get the stored configuration values, defaults are the second parameter in the list
  myPrefs.begin("general");
  nodeName = myPrefs.getString("nodename", "MQTTtosNode");
  // BTname = myPrefs.getString("BTname", "MQTTtosNode");
  BTname = nodeName;
  BTpassword = myPrefs.getString("password", "IGNORE");
  SSID = myPrefs.getString("SSID", "none");
  wifiPassword = myPrefs.getString("wifipassword", "none");
  mqttServer = myPrefs.getString("mqttserver", "none");
  // strcpy(mqttchannel, myPrefs.getString("mqttchannel", "trains/").c_str());
  mqttChannel = myPrefs.getString("mqttchannel", "trains/");
  myPrefs.end();

  // wifiPassword = "Bogus";

  // Bluetooth
  // myPrefs.begin("general");
  // if (myPrefs.getBool("BTon", true)) TBD always turn on BT on reboot?
  BTSerial.begin(nodeName);
  // myPrefs.end();

  // WiFi
  setup_wifi();

  // MQTT
  char mqtt_server[mqttServer.length() + 1]; // converting from string to char array required for client parameter
  strcpy(mqtt_server, mqttServer.c_str());
  uint8_t ip[4];
  sscanf(mqtt_server, "%u.%u.%u.%u", &ip[0], &ip[1], &ip[2], &ip[3]);
  client.setServer(ip, 1883);
  // client.setSocketTimeout(60);
  client.setKeepAlive(60);
  client.setCallback(callback);

  // TO specific
  // strcpy(turnoutTopic, mqttchannel);
  // turnoutTopic = string(mqttchannel) + "track/turnout";
  turnoutTopic = mqttChannel + "track/turnout";
  // strcat(turnoutTopic, "track/turnout/");

  // read the stored values for speed, throw, torque and reversed
  // send those values to the stepper objects
  for (int i = 0; i < numDevices; i++)
  {
    myPrefs.begin(deviceSpace[i]);
    devName[i] = myPrefs.getString("name", "noname");

    // set the rotational speed using rpm as parameter, defaults to NOMINAL_SPEED
    myStepper[i].setSpeed(myPrefs.getUShort("speed", NOMINAL_SPEED));

    // set the length of the stroke, defaults to NOMINAL_STROKE
    // nominal value of 500 should be enough for most turnouts
    // it could be made less through experimentation
    myStepper[i].setStrokeSteps(myPrefs.getUShort("throw", NOMINAL_STROKE));

    // limit the torque, defaults to NOMINAL_TORQUE_INTERVAL
    // this defines the maximum time in microseconds that current will be allowed to flow in each step
    // the default is 1000, a larger number provides more torque but consumes more current
    // at 1000 rpm the step length is 3000 ms for a 20 step/revolution motor
    myStepper[i].setTorqueLimit(myPrefs.getUShort("force", NOMINAL_TORQUE_INTERVAL));

    // configure the direction, defaults to false (non-reversed)
    // design assumes device is installed on the closed side of turnout and that turnout is closed
    // the first movement after startup will be to pull the throwbar thus throwing the track switch
    // setting reversed to true will set motion to the opposite of above as required if machine is located on diverging side
    myStepper[i].setReversed(myPrefs.getBool("reversed", false));
    myPrefs.end();
  }
}

/*****************************************************************************/
void setup_wifi()
{
  delay(10);

  char ssid[SSID.length() + 1];
  strcpy(ssid, SSID.c_str());
  char wifipassword[wifiPassword.length() + 1];
  strcpy(wifipassword, wifiPassword.c_str());

  WiFi.begin(ssid, wifipassword);

  if (WiFi.status() != WL_CONNECTED)
    pinMode(2, OUTPUT);

  uint32_t now = millis();

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(300);
    Serial.print(".");
    // blink the blue LED to indicate error condition
    digitalWrite(2, HIGH);
    delay(300);
    digitalWrite(2, LOW);

    if (millis() - now > 5000)
      break;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    digitalWrite(2, LOW);
    pinMode(2, INPUT);
    return;
  }
  else
  {
    // allow operator to get in via Bluetooth to provide credentials
    while (true)
    {
      if (BTSerial.available())
      {
        flushSerialIn();
        if (pwCheck())
          configure();
      }
    }
  }
}

/*****************************************************************************/
void reconnect()
{
  bool flasher = false;

  char mqtt_node[nodeName.length() + 1];
  strcpy(mqtt_node, nodeName.c_str());

  // Loop until we're reconnected TBD change all Serial to BTSerial (maybe)
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_node))
    {
      Serial.println("connected");
      char subscription[50];
      // accept all <channel>/track/turnout/ topics
      // they will be of the form <JMRI channel>/track/turnout/<JMRI system name> THROWN/CLOSED
      // strcpy(subscription, mqttchannel);
      strcpy(subscription, mqttChannel.c_str());
      strcat(subscription, "track/turnout/+");
      client.subscribe(subscription, 1);
    }
    else
    {
      pinMode(2, OUTPUT);
      Serial.print("Failed to connect to ");
      Serial.print(mqttServer);
      Serial.print(" Response was ");
      Serial.println(client.state());
      Serial.println("Looping every 2 seconds. MQTT server must be configured using BT menu");
      flasher = !flasher;
      Serial.println(flasher);
      if (flasher == true)
        digitalWrite(2, HIGH);
      else
        digitalWrite(2, LOW);

      // Wait 2 seconds before retrying
      if (BTSerial.available())
      {
        flushSerialIn();
        if (pwCheck())
          configure();
        // the device will be rebooted at this point after the operator resets mqtt server
      }
      delay(2000);
    }
  }
}

/*****************************************************************************/
void loop()
{
  // this will reset the password to "IGNORE" and turn on Bluetooth
  // use this if Bluetooth has been disabled from the menu (to prevent hackers in the house)
  // or if password was forgotten
  // TBD maybe this should go in setup, use first switch pin to save pins
  // if (digitalRead(2) == LOW) //TBD fix this, does not work as expected
  // {
  //   BTSerial.begin();
  //   myPrefs.begin("general", false);
  //   myPrefs.putBool("BTon", true);
  //   myPrefs.putString("password", "IGNORE");
  //   myPrefs.end();
  //   configure();
  // }

  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  if (BTSerial.available())
  {
    flushSerialIn();
    if (pwCheck())
      configure();
  }

  // checkSwitches(); TBD need pullups for this to work

  // if (runSteppers() && returnToMenu) // runSteppers returns true at end of throw, check if throw was commanded from menu
  // {
  //   returnToMenu = false; // we came here from the menu 'A' command, return to menu
  //   configure();
  // }
}

/*****************************************************************************/
// this is a callback from the mqtt object, made when a subscribed message comes in
void callback(char *topic, byte *message, unsigned int length)
{
  String topicString;
  String lastPart;
  char messChars[50];

  topicString = String(topic);

  for (int i = 0; i < length; i++)
    messChars[i] = (char)message[i];
  messChars[length] = '\0';

  if (turnoutTopic.equals(topicString.substring(0, topicString.lastIndexOf('/') + 1)))
    return;

  lastPart = topicString.substring(topicString.lastIndexOf('/') + 1);

  for (int i = 0; i < numDevices; i++)
  {
    if (lastPart.equals(devName[i]))
    {
      if (strcmp(messChars, "THROWN") == 0)
        // myStepper[i].setReady(1);
        BTSerial.println("Thrown received");
      if (strcmp(messChars, "CLOSED") == 0)
        // myStepper[i].setReady(0);
        BTSerial.println("Closed received");
    }
  }
}

/*****************************************************************************/
void checkSwitches()
{
  // for (int i = 0; i < numDevices; i++)
  for (int i = 0; i < 1; i++) // TBD remove this, testing only
  {
    // if corresponding stepper is in ready state or is running just skip it (debounces the switch)
    if ((digitalRead(switchPin[i]) == LOW) && (!myStepper[i].getRunState()) && (!myStepper[i].getReadyState()))
    {
      switch (myStepper[i].getLastCommanded())
      {
      case 0:
        myStepper[i].setReady(1);
        break;
      case 1:
        myStepper[i].setReady(0);
        break;
      default:
        myStepper[i].setReady(1);
        break;
      }
      return;
    }
  }
}

/*****************************************************************************/
bool runSteppers() // returns true if a throw was completed, false otherwise
{
  // this routine must be called repeatedly in the loop
  bool throwComplete = false;

  for (int i = 0; i < numDevices; i++)
  {
    if (myStepper[i].getRunState()) // returns false if not in running state
    {
      // throwComplete = myStepper[i].run();
      if (myStepper[i].run()) // true if completed
      {
        // if (myStepper->getLastCommanded())
        //   myCbus.sendMessage(ACON, i);
        // else
        //   myCbus.sendMessage(ACOF, i);
        // return true;
      }
      // TBD if returns true then complete, send a message, requires mod to stepper
      return false; // if it did run don't try to run any others
    }
  }

  // if we got here nothing was running so check for steppers that are ready and set the first found to run
  for (int i = 0; i < numDevices; i++)
  {
    if (myStepper[i].getReadyState())
    {
      // the first one we find that is ready we set to run and then exit
      myStepper[i].run();
    }
  }
  return false;
}

/*****************************************************************************/
// displays the menu for user interaction for configuration or testing
void showMenu()
{
  BTSerial.println(" ");
  BTSerial.print("\nMain menu for ");
  BTSerial.println(BTname);
  BTSerial.println("\n Enter: ");
  BTSerial.println(" 'P' - Print status");
  BTSerial.println(" 'N' - Set node name");
  BTSerial.println(" 'X' - Set Bluetooth password");
  BTSerial.println(" 'W' - Set WiFi credentials");
  BTSerial.println(" 'M' - Set MQTT server IP address");
  BTSerial.println(" 'C' - Set MQTT channel");
  BTSerial.println(" 'T' - Set turnout name(s)");
  // BTSerial.println(" 'Z' - Turn off Bluetooth until reset from pin 2");
  BTSerial.println(" 'D' - Debug display on/off");
  BTSerial.println(" 'B' - Restart machine");

  BTSerial.println("\n Enter 'R' to return to run mode (automatic after 30 sec of inactivity)");
}

/*****************************************************************************/
// business end of the menu
void configure()
{
  uint16_t devID;
  uint16_t paramVal;
  uint16_t enteredVal;
  uint16_t _turnoutNumber;
  bool paramBool;
  String pw;
  String myString;
  bool beenHereDoneThat = false;
  char myChar;
  IPAddress ipAdr;

  while (true)
  {
    if (!beenHereDoneThat)
    {
      showMenu();
      beenHereDoneThat = true;
    }
    else
      BTSerial.println("\nEnter empty line to show menu again");

    switch (getUpperChar(millis()))
    {

    case 'T': // turnout names
      while (true)
      {
        BTSerial.print("\n Enter turnout number (1 - 4), blank line to exit: ");
        _turnoutNumber = getNumber(0, numDevices);
        if (_turnoutNumber <= 0)
          break;
        BTSerial.print("\nEnter name, blank line to exit: ");
        while (!BTSerial.available())
        {
        }
        myString = BTSerial.readString();
        myString.trim();
        if (myString.length() == 0)
          break;
        devName[_turnoutNumber - 1] = myString;
        myPrefs.begin(deviceSpace[_turnoutNumber - 1], false);
        myPrefs.putString("name", myString);
        myPrefs.end();
        BTSerial.print("Changed to ");
        BTSerial.println(myString);
        break;
      }

    case 'P':
    {
      BTSerial.println("\nCurrent configuration");
      BTSerial.print("Node name (MQTT and Bluetooth) = ");
      BTSerial.println(nodeName);
      ipAdr = WiFi.localIP();
      BTSerial.print("Local IP address = ");
      BTSerial.println(ipAdr);
      BTSerial.print("SSID = ");
      BTSerial.println(SSID);
      BTSerial.print("MQTT server = ");
      BTSerial.println(mqttServer);
      BTSerial.print("MQTT channel = ");
      BTSerial.println(mqttChannel);

      for (int i = 0; i < numDevices; i++)
      {
        BTSerial.print("Device ");
        BTSerial.print(i + 1);
        BTSerial.print(" name = ");
        BTSerial.println(devName[i]);
      }
    }
    break;

    case 'X':
      BTSerial.print("\nEnter password: ");
      while (!BTSerial.available())
      {
      }
      pw = BTSerial.readString();
      pw.trim();
      if (myString.length() == 0)
        break;
      myPrefs.begin("general", false);
      myPrefs.putString("password", pw);
      myPrefs.end();
      BTpassword = pw;
      BTSerial.print("Changed to ");
      BTSerial.println(pw);
      break;

    case 'W':
      setCredentials();
      break;

    case 'M':
      setMQTT();
      break;

    case 'C': // set mqtt channel
      BTSerial.print("\nCurrent MQTT channel: ");
      BTSerial.println(mqttChannel);
      BTSerial.print("Enter new MQTT channel or blank line to exit: ");
      while (!BTSerial.available())
      {
      }
      myString = BTSerial.readString();
      myString.trim();
      if (myString.length() == 0)
        break;
      myPrefs.begin("general", false);
      myPrefs.putString("mqttchannel", myString);
      myPrefs.end();
      BTSerial.print("\nChanged to ");
      BTSerial.println(myString);
      BTSerial.println("\nReboot is required");
      break;

    case 'N':
      BTSerial.println("Enter a name for this node (must be unique), blank to exit: ");
      while (!BTSerial.available())
      {
      }
      myString = BTSerial.readString();
      myString.trim();
      if (myString.length() == 0)
        break;
      myPrefs.begin("general", false);
      myPrefs.putString("nodename", myString);
      myPrefs.end();
      BTSerial.print("Changed to ");
      BTSerial.println(myString);
      BTSerial.println("\nReboot is required");
      break;

    case 'Z':
      myPrefs.begin("general", false);
      myPrefs.putBool("BTon", false);
      myPrefs.end();
      BTSerial.end();
      BTSerial.disconnect();
      break;

    case 'D':
      BTSerial.println("Turn debug display on ('Y') or off ('N') ");
      myChar = getUpperChar(30000);
      BTSerial.println("Which detector (1-8)?");
      devID = getNumber(1, 8);
      // bod[devID - 1].setDisplayDetect(myChar = 'Y');
      break;

    case 'R':
      BTSerial.println("\nBack to run mode");
      delay(1000);
      return;

    case 'B': // reboot
      BTSerial.println("\nDevice will now be rebooted...");
      delay(1000);
      ESP.restart();
      break;

    default:
      beenHereDoneThat = false;
      break;
    }
  }
}

/*****************************************************************************/
void setCredentials()
{
  String myString;

  BTSerial.print("\nEnter SSID: ");
  while (!BTSerial.available())
  {
  }
  myString = BTSerial.readString();
  myString.trim();
  myPrefs.begin("general", false);
  myPrefs.putString("SSID", myString);
  BTSerial.print("Changed to ");
  BTSerial.println(myString);

  BTSerial.print("\nEnter WiFi password: ");
  while (!BTSerial.available())
  {
  }
  myString = BTSerial.readString();
  myString.trim();
  myPrefs.putString("wifipassword", myString);
  myPrefs.end();
  BTSerial.print("Changed to ");
  BTSerial.println(myString);
  BTSerial.println("\nDevice will now be rebooted...");
  delay(3000);
  ESP.restart();
}

/*****************************************************************************/
void setMQTT()
{
  String myString;
  BTSerial.print("\nEnter MQTT server IP address: ");
  while (!BTSerial.available())
  {
  }
  myString = BTSerial.readString();
  myString.trim();
  myPrefs.begin("general", false);
  myPrefs.putString("mqttserver", myString);
  myPrefs.end();
  BTSerial.print("Changed to ");
  BTSerial.println(myString);
  BTSerial.println("\nDevice will now be rebooted...");
  delay(3000);
  ESP.restart();
}

/*****************************************************************************/
// returns upper case character
char getUpperChar(uint32_t invokeTime)
{
  // gets one character and converts to upper case, clears input buffer of C/R and newline
  char _myChar;

  while (true)
  {
    if (invokeTime > 0)
    {
      if (timeout(invokeTime))
        return 'R'; // operator dozed off
    }

    if (BTSerial.available() > 0)
    {
      delay(5);
      _myChar = BTSerial.read();

      if (_myChar > 96)
        _myChar -= 32;

      flushSerialIn();
      return _myChar;
    }
  }
}

/*****************************************************************************/
// get rid of unseen characters in buffer
void flushSerialIn(void)
{
  while (BTSerial.available() > 0)
  { // clear the buffer
    delay(5);
    BTSerial.read();
  }
}

/*****************************************************************************/
// returns an integer within specified limits
int getNumber(int min, int max)
{
  int _inNumber;

  while (true)
  {
    if (BTSerial.available() > 0)
    {
      _inNumber = BTSerial.parseInt();

      if ((_inNumber < min) || (_inNumber > max))
      {
        BTSerial.println(F("Out of range, reenter"));
        flushSerialIn();
      }
      else
      {
        flushSerialIn();
        return _inNumber;
      }
    }
  }
}

/*****************************************************************************/
// tests password

bool pwCheck()
{
  uint32_t startTime;
  uint32_t now;

  myPrefs.begin("general", true);
  pwCandidate = myPrefs.getString("password", "IGNORE");
  myPrefs.end();

  pwCandidate.trim();

  if (pwCandidate.equalsIgnoreCase("IGNORE"))
    return true;

  startTime = millis();

  BTSerial.print("Enter password: ");
  flushSerialIn();

  while (!BTSerial.available())
  {
    if (millis() - startTime >= 20000)
    {
      printMsg("Timed out");
      BTSerial.disconnect();
      return false;
    }
  }
  pwCandidate = BTSerial.readString();
  pwCandidate.trim();

  if (pwCandidate.equalsIgnoreCase(BTpassword))
  {
    return true;
  }
  else
  {
    printMsg("Wrong password entered, disconnecting");
    BTSerial.disconnect();
    return false;
  }
}

/*****************************************************************************/
bool timeout(uint32_t myTime)
{
  if (millis() - myTime > 30000)
    // disconnect if no action from operator for 30 seconds
    return true;
  else
    return false;
}

/*****************************************************************************/
void printMsg(char *msg)
{
  BTSerial.println(msg);
}

/*****************************************************************************/
void printBinary(uint8_t binVal)
{
  BTSerial.println(binVal, HEX);
}
