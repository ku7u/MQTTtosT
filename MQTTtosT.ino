
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

ToDo:

  Pin assignments:
   01  TX0
   02
   03  RX0
   04  GPIO reserved for neopixel output
   05  switch for configuration via bluetooth
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
#include <esp_timer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include "WiFi.h"
#include <BluetoothSerial.h>
#include <Adafruit_NeoPixel.h>
#include "StepperMRTO.h"
//#define testing

using namespace std;

const char *version = "2.2.0";

Preferences myPrefs;
char *deviceSpace[] = {"d1", "d2", "d3", "d4"};

// wifi
WiFiClient espClient;
String SSID;
String wifiPassword;

// mqtt
String mqttServer;
String mqttNode;
String topicLeftEnd;
String topicFeedbackLeftEnd;
String turnoutTopic;
String turnoutFeedbackTopic;
PubSubClient client(espClient);

// Bluetooth
BluetoothSerial BTSerial;
String BTname;
String BTpassword;
String pwCandidate;
String pwtest;

String nodeName;

// constants and variables - turnouts
uint16_t const NUM_DEVICES = 4;
String devName[NUM_DEVICES];
uint16_t const STEPS_PER_REVOLUTION = 20; // number of steps per revolution
uint16_t const NOMINAL_SPEED = 1000;
uint16_t const NOMINAL_STROKE = 500;
uint16_t const NOMINAL_TORQUE_INTERVAL = 500;

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
    {StepperMRTO(STEPS_PER_REVOLUTION, APlus1Pin, AMinus1Pin, BPlus1Pin, BMinus1Pin),
     StepperMRTO(STEPS_PER_REVOLUTION, APlus2Pin, AMinus2Pin, BPlus2Pin, BMinus2Pin),
     StepperMRTO(STEPS_PER_REVOLUTION, APlus3Pin, AMinus3Pin, BPlus3Pin, BMinus3Pin),
     StepperMRTO(STEPS_PER_REVOLUTION, APlus4Pin, AMinus4Pin, BPlus4Pin, BMinus4Pin)};

// switch pins for manual control, these must have pullup resistors
uint16_t const switchPin[4] = {34, 35, 36, 39};
bool switchesAvailable = false; // change to true from menu iff switch pins have pullups

bool returnToMenu = false; // for test actuation feature from menu

// neoPixels
#define LED_PIN 4
#define LED_COUNT 4
uint32_t red;
uint32_t yellow;
uint32_t green;
uint32_t blue;
uint32_t dark = 0;
bool flasher;           // used to flash the yellow light while moving
uint32_t lastFlashTime; // for flasher

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

uint32_t testTimer;
uint32_t testCounter;

/*****************************************************************************/
void setup()
{
  byte myVal;

  Serial.begin(115200);
  pinMode(5, INPUT_PULLUP); // this is used for resetting Bluetooth

  // start neoPixels and set all to blue
  // neoPixels must be wired in order of devices, first nP is device 1
  strip.begin();
  red = strip.gamma32(strip.ColorHSV(0, 200, 70));
  yellow = strip.gamma32(strip.ColorHSV((65536 / 6) - 1500, 255, 100)); // a little brighter and yellower
  green = strip.gamma32(strip.ColorHSV(65536 / 3, 200, 70));
  blue = strip.gamma32(strip.ColorHSV(65536 * 2 / 3, 200, 70));
  for (int i = 0; i < NUM_DEVICES; i++)
    strip.setPixelColor(i, blue);
  strip.show();

  // get the stored configuration values, defaults are the second parameter in the list
  myPrefs.begin("general");
  switchesAvailable = myPrefs.getBool("switchesavailable", false);
  nodeName = myPrefs.getString("nodename", "MQTTtosNode");
  BTname = nodeName;                                    // share the node name with Bluetooth
  BTpassword = myPrefs.getString("password", "IGNORE"); // treats IGNORE as if no password
  SSID = myPrefs.getString("SSID", "none");
  wifiPassword = myPrefs.getString("wifipassword", "none");
  mqttServer = myPrefs.getString("mqttserver", "none");
  topicLeftEnd = myPrefs.getString("topicleftend", "trains/track/turnout/");
  topicFeedbackLeftEnd = myPrefs.getString("topicfeedbackleftend", "trains/track/sensor/turnout/");
  myPrefs.end();

  // Bluetooth
  myPrefs.begin("general");
  if (myPrefs.getBool("BTon", true))
    BTSerial.begin(nodeName);
  myPrefs.end();

  // WiFi
  setup_wifi();

  // MQTT
  char mqtt_server[mqttServer.length() + 1]; // converting from string to char array required for client parameter
  strcpy(mqtt_server, mqttServer.c_str());
  uint8_t ip[4];
  sscanf(mqtt_server, "%u.%u.%u.%u", &ip[0], &ip[1], &ip[2], &ip[3]);
  client.setServer(ip, 1883); // 1883 is the default port on mosquitto server
  client.setKeepAlive(60);    // this is probaably not necessary, just use the default
  client.setCallback(callback);
  connectMQTT();

  // turnout specific
  // read the stored values for speed, throw, torque and reversed
  // defaults are the second parameter in the list
  // send those values to the stepper objects
  for (int i = 0; i < NUM_DEVICES; i++)
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
    // this defines the time in microseconds that current current flow will be shortened in each step
    // the default is 500, a smaller number provides more torque but consumes more current
    // at 1000 rpm the step length is 3000 ms for a 20 step/revolution motor
    myStepper[i].setTorqueLimit(myPrefs.getUShort("force", NOMINAL_TORQUE_INTERVAL));

    // configure the direction, defaults to false (non-reversed)
    // design assumes device is installed on the closed side of turnout and that turnout is closed
    // the first movement after startup will be to pull the throwbar thus throwing the track switch
    // setting reversed to true will set motion to the opposite of above as required if machine is located on diverging side
    // as well, the device may need to be reversed depending on whether coil wires are reversed
    // bottom line is to set the reversed parameter to make the device work as expected
    myStepper[i].setReversed(myPrefs.getBool("reversed", false));
    myPrefs.end();
  }

  setupSubscriptions();
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
    // blink the blue LED to indicate error condition
    digitalWrite(2, LOW);
    delay(300);
    digitalWrite(2, HIGH);

    if (millis() - now > 5000)
      break;
  }

  pinMode(2, INPUT_PULLUP);

  if (WiFi.status() == WL_CONNECTED)
  {
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
void connectMQTT()
{
  bool flasher = false;

  char mqtt_node[nodeName.length() + 1];
  strcpy(mqtt_node, nodeName.c_str());

  uint32_t now = millis();

  // Loop until we're reconnected
  while (!client.connect(mqtt_node))
  {
    pinMode(2, OUTPUT);
    Serial.print("Failed to connect to ");
    Serial.print(mqttServer);
    Serial.print(" Response was ");
    Serial.println(client.state());
    Serial.println("Retrying. MQTT server must be configured using BT menu");
    flasher = !flasher;
    if (flasher == true)
      digitalWrite(2, HIGH);
    else
      digitalWrite(2, LOW);
    // Wait 1 second before retrying
    delay(1000);

    if (millis() - now > 5000 && BTSerial.available())
      // this is not looking good so bail out and let operator set configuration
      // if BT is not available just keep blinking the blue light
      break;
  }

  pinMode(2, INPUT_PULLUP);

  if (BTSerial.available() && !client.connected())
  // show the config menu so operator can set the MQTT server
  {
    flushSerialIn();
    if (pwCheck())
      configure();
    // the device should be rebooted at this point after the operator resets mqtt server
  }
}

/*****************************************************************************/
void setupSubscriptions()
{
  char subscription[100];
  // accept all <topic left end>/<node>/<device> topics
  // they will be of the form trains/track/turnout/<JMRI system name> THROWN/CLOSED
  // JMRI system name must be nodename + "/" + device name

  // feedback (not a subscription) is of the form trains/track/sensor/turnout/<JMRI system name> ACTIVE/INACTIVE

  turnoutTopic = topicLeftEnd + nodeName + "/";
  turnoutFeedbackTopic = topicFeedbackLeftEnd + nodeName + "/";

  for (int i = 0; i < NUM_DEVICES; i++)
  {
    strcpy(subscription, turnoutTopic.c_str());
    strcat(subscription, devName[i].c_str());

    client.subscribe(subscription, 1);
  }
}

/*****************************************************************************/
void loop()
{
  // testCounter++;

  // this will reset the password to "IGNORE" and turn on Bluetooth
  // use this if Bluetooth has been disabled from the menu (to prevent hackers in the house)
  // or if password was forgotten
  if (digitalRead(5) == LOW) // TBD does it work as expected
  {
    BTSerial.begin();
    myPrefs.begin("general", false);
    myPrefs.putBool("BTon", true);
    myPrefs.putString("password", "IGNORE");
    myPrefs.end();
    configure();
  }

  if (!client.connected())
  {
    connectMQTT();
    BTSerial.println("client connection");
    setupSubscriptions();
  }

  client.loop();

  if (BTSerial.available())
  {
    flushSerialIn();
    if (pwCheck())
      configure();
  }

  if (switchesAvailable)
    checkSwitches();

  //  testTimer = micros();
  if (runSteppers() && returnToMenu) // runSteppers returns true at end of throw, check if throw was commanded from menu
  {
    returnToMenu = false; // we came here from the menu 'A' command, return to menu
    configure();
  }
}

/*****************************************************************************/
// this is a callback from the mqtt object, made when a subscribed message comes in
// we only expect four topics that were subscribed so only need to check the last part which is the device
void callback(char *topic, byte *message, unsigned int length)
{
  String topicString;
  String lastPart;
  char messChars[50];

  topicString = String(topic);

  for (int i = 0; i < length; i++)
    messChars[i] = (char)message[i];
  messChars[length] = '\0';

  lastPart = topicString.substring(topicString.lastIndexOf('/') + 1);

  for (int i = 0; i < NUM_DEVICES; i++)
  {
    if (lastPart.equals(devName[i]))
    {
      if (strcmp(messChars, "THROWN") == 0)
      {
        myStepper[i].setReady(1);
        // BTSerial.println("Thrown received");
      }
      if (strcmp(messChars, "CLOSED") == 0)
      {
        myStepper[i].setReady(0);
        // BTSerial.println("Closed received");
      }
    }
  }
}

/*****************************************************************************/
// each push of a switch activaates the device to move in direction opposite to last commanded
void checkSwitches()
{
  for (int i = 0; i < NUM_DEVICES; i++)
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
// looks for steppers that are ready to run and runs them, one at a time (to limit current)
// this routine must be called repeatedly in the loop
bool runSteppers() // returns true if a throw was completed, false otherwise
{
  bool throwComplete = false;
  String feedbackTopic;

  for (int i = 0; i < NUM_DEVICES; i++)
  {
    if (myStepper[i].getRunState()) // returns false if not in running state
    {
      if (myStepper[i].run()) // true if completed
      {
        feedbackTopic = turnoutFeedbackTopic + devName[i];
        if (myStepper[i].getLastCommanded())
        {
          client.publish(feedbackTopic.c_str(), "ACTIVE");
          strip.setPixelColor(i, red);
        }
        else
        {
          client.publish(feedbackTopic.c_str(), "INACTIVE");
          strip.setPixelColor(i, green);
        }
        strip.show();
        return true;
      }
      // flash the yellow when in motion
      if (millis() - lastFlashTime > 100)
      {
        lastFlashTime = millis();
        if (flasher)
          strip.setPixelColor(i, yellow);
        else
          strip.setPixelColor(i, 0);
        flasher = !flasher;
        strip.show();
      }
      return false; // if it did run don't try to run any others
    }
  }

  // if we got here nothing was running so check for steppers that are ready and set the first found to run
  for (int i = 0; i < NUM_DEVICES; i++)
  {
    if (myStepper[i].getReadyState())
    {
      // the first one we find that is ready we set to run and then exit
      myStepper[i].run();
      // strip.setPixelColor(i, yellow);
      // strip.show();
      break;
    }
  }

  return false;
}

/*****************************************************************************/
// displays the menu for user interaction for configuration or testing
void showMenu()
{
  static bool beenDone;
  BTSerial.println(" ");
  BTSerial.print("\nTurnout Controller Main Menu for ");
  BTSerial.println(BTname);
  BTSerial.println("\n Enter: ");
  BTSerial.println(" 'P' - Print status");
  BTSerial.println(" 'N' - Set node name (Bluetooth and MQTT)");
  BTSerial.println(" 'X' - Set Bluetooth password");
  BTSerial.println(" 'W' - Set WiFi credentials");
  BTSerial.println(" 'M' - Set MQTT server IP address");
  BTSerial.println(" 'L' - Set left side of topic");
  BTSerial.println(" 'Y' - Enable/Disable switches");
  BTSerial.println(" 'T' - Set turnout name(s)");
  BTSerial.println(" 'S' - Set stepper parameters");
  BTSerial.println(" 'A' - Actuate a stepper for testing");
  BTSerial.println(" 'Z' - Turn off Bluetooth until pin 5 reset");
  // BTSerial.println(" 'D' - Debug display on/off");
  BTSerial.println(" 'B' - Restart machine");

  if (!beenDone)
  {
    BTSerial.print("\n Menu choices are not case sensitive");
    beenDone = true;
  }
  BTSerial.println("\n Enter empty line to return to run mode \n (automatic after 30 sec of inactivity)");
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
  bool changed;

  while (true)
  {
    if (!beenHereDoneThat)
    {
      showMenu();
      beenHereDoneThat = true;
    }
    else
      BTSerial.println("\nMain menu\n Enter 'R' to review menu, empty line to exit");

    switch (getUpperChar(millis()))
    {

    case 'T': // turnout names
      while (true)
      {
        BTSerial.print("\nTurnout naming menu\n Enter turnout number (1 - 4), empty line to exit: ");
        _turnoutNumber = getNumber(0, NUM_DEVICES);
        if (_turnoutNumber <= 0)
          break;
        BTSerial.print("\n Enter name, empty line to exit: ");
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
      BTSerial.print(" Firmware version: ");
      BTSerial.println(version);
      BTSerial.print(" Node name (MQTT and Bluetooth) = ");
      BTSerial.println(nodeName);
      ipAdr = WiFi.localIP();
      BTSerial.print(" Local IP address = ");
      BTSerial.println(ipAdr);
      BTSerial.print(" SSID = ");
      BTSerial.print(SSID);
      if (WiFi.status() == WL_CONNECTED)
        BTSerial.println(" connected");
      else
        BTSerial.println(" not connected");
      BTSerial.print(" MQTT server = ");
      BTSerial.print(mqttServer);
      if (client.connected())
        BTSerial.println(" connected");
      else
        BTSerial.println(" not connected");
      BTSerial.print(" MQTT topic header = ");
      BTSerial.println(topicLeftEnd);

      for (int i = 0; i < NUM_DEVICES; i++)
      {
        BTSerial.print(" Device ");
        BTSerial.print(i + 1);
        BTSerial.print(" name = ");
        BTSerial.println(devName[i]);
      }
    }
    break;

    case 'X':
      BTSerial.print("\n Enter new Bluetooth password, empty line to exit ");
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
      BTSerial.print(" Changed to ");
      BTSerial.println(pw);
      break;

    case 'W':
      setCredentials();
      break;

    case 'M':
      setMQTT();
      break;

    case 'N':
      BTSerial.println("\n Enter a name for this node (must be unique), empty line  to exit: ");
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
      BTSerial.print(" Changed to ");
      BTSerial.println(myString);
      BTSerial.println("\nReboot is required");
      break;

    case 'L': // set topic left end
      changed = false;
      BTSerial.print("\n Current topic header: ");
      BTSerial.println(topicLeftEnd);
      BTSerial.println("\n Enter new topic header or empty line to exit: ");
      while (!BTSerial.available())
      {
      }
      myString = BTSerial.readString();
      myString.trim();
      if (myString.length() != 0)
      // break;
      {
        changed = true;
        myPrefs.begin("general", false);
        myPrefs.putString("topicleftend", myString);
        myPrefs.end();
        BTSerial.print("\n Changed to ");
        BTSerial.println(myString);
      }

      BTSerial.print("\n Current feedback topic header: ");
      BTSerial.println(topicFeedbackLeftEnd);
      BTSerial.println("\n Enter new feedback topic header or empty line to exit: ");
      while (!BTSerial.available())
      {
      }
      myString = BTSerial.readString();
      myString.trim();
      if (myString.length() != 0)
      // break;
      {
        changed = true;
        myPrefs.begin("general", false);
        myPrefs.putString("topicfeedbackleftend", myString);
        myPrefs.end();
        BTSerial.print("\n Changed to ");
        BTSerial.println(myString);
      }
      if (changed)
        BTSerial.println("\n Reboot is required");
      break;

    case 'Y': // enable/disable switches
      BTSerial.print("\n Enable/Disable manual switches\n");
      BTSerial.print(" Pullups must be installed for switches to work!\n");
      BTSerial.print(" Enter 'E' or 'D', empty line to exit\n");
      myChar = getUpperChar(millis());
      if (myChar == 'E')
        switchesAvailable = true;
      else if (myChar == 'D')
        switchesAvailable = false;
      else
        break;
      myPrefs.begin("general", false);
      myPrefs.putBool("switchesavailable", switchesAvailable);
      myPrefs.end();
      break;

    case 'S': // stepper configuration
      stepperParameters();
      break;

    case 'A':
      BTSerial.print("\n Actuate a turnout motor\n Enter device number (1 - 4): ");
      devID = getNumber(1, 4);
      myStepper[devID - 1].setReady(!myStepper[devID - 1].getLastCommanded());
      returnToMenu = true;
      return;

    case 'Z':
      BTSerial.println(" Turning off Bluetooth, ground pin 5 to reconnect");
      BTSerial.println(" Continue? ('Y' or 'N')");
      myChar = getUpperChar(30000);
      if (myChar != 'Y')
        break;
      myPrefs.begin("general", false);
      myPrefs.putBool("BTon", false);
      myPrefs.end();
      BTSerial.end();
      BTSerial.disconnect();
      break;

    case 'D':
      BTSerial.println(" Turn debug display on ('Y') or off ('N') ");
      myChar = getUpperChar(30000);
      BTSerial.println(" Which detector (1-8)?");
      devID = getNumber(1, 8);
      // bod[devID - 1].setDisplayDetect(myChar = 'Y');
      break;

    case 'R':
      beenHereDoneThat = false;
      break;

    case 'B': // reboot
      BTSerial.println("\nDevice will now be rebooted...");
      delay(1000);
      ESP.restart();
      break;

    default:
      BTSerial.println("\nExiting to run mode");
      // delay(1000);
      return;
    }
  }
}

/*****************************************************************************/
// configure all of the stepper parameters
void stepperParameters()
{
  uint16_t devID;
  uint16_t paramVal;
  bool paramBool;

  while (true)
  {
    BTSerial.println("\nConfiguration menu for Speed, Throw, Force, Direction\n");
    BTSerial.println(" Enter S, T, F or D. Enter empty line to exit");

    switch (getUpperChar(millis()))
    {
    case 'S':
      BTSerial.println("\n Speed (rpm, valid value 100 - 2000, default = 1000)");
      showCurrentValues('S');
      BTSerial.println("\n Enter 'C' to change or empty line  to exit");
      if (getUpperChar(millis()) != 'C')
        break;
      BTSerial.println(" Enter device (1 - 4) or 0 for all");

      devID = getNumber(0, 4);
      BTSerial.print(" Enter value for ");
      if (devID == 0)
        BTSerial.println("all devices");
      else
      {
        BTSerial.print("device ");
        BTSerial.println(devID);
      }
      paramVal = getNumber(100, 2000);
      if (devID == 0)
        for (int i = 0; i < NUM_DEVICES; i++)
        {
          myPrefs.begin(deviceSpace[i], false);
          myPrefs.putUShort("speed", paramVal);
          myPrefs.end();
          myStepper[i].setSpeed(paramVal);
        }
      else
      {
        myPrefs.begin(deviceSpace[devID - 1], false);
        myPrefs.putUShort("speed", paramVal);
        myPrefs.end();
        myStepper[devID - 1].setSpeed(paramVal);
      }
      break;

    case 'T':
      BTSerial.println("\n Throw (steps, valid value 200 - 800, default = 600, 1000 steps = 7mm)");
      showCurrentValues('T');
      BTSerial.println("\n Enter 'C' to change or empty line  to exit");
      if (getUpperChar(millis()) != 'C')
        break;
      BTSerial.println(" Enter device (1 - 4) or 0 for all");

      devID = getNumber(0, 4);
      BTSerial.print(" Enter value for ");
      if (devID == 0)
        BTSerial.println("all devices");
      else
      {
        BTSerial.print("device ");
        BTSerial.println(devID);
      }
      paramVal = getNumber(200, 800);
      if (devID == 0)
        for (int i = 0; i < NUM_DEVICES; i++)
        {
          myPrefs.begin(deviceSpace[i], false);
          myPrefs.putUShort("throw", paramVal);
          myPrefs.end();
          myStepper[i].setStrokeSteps(paramVal);
        }
      else
      {
        myPrefs.begin(deviceSpace[devID - 1], false);
        myPrefs.putUShort("throw", paramVal);
        myPrefs.end();
        myStepper[devID - 1].setStrokeSteps(paramVal);
      }
      break;

    case 'F':
      BTSerial.println("\n Force (valid value 0 - 1500, default = 500)");
      BTSerial.println(" Larger values reduce force\n");
      showCurrentValues('F');
      BTSerial.println("\n Enter 'C' to change or empty line  to exit");
      if (getUpperChar(millis()) != 'C')
        break;
      BTSerial.println(" Enter device (1 - 4) or 0 for all");

      devID = getNumber(0, 4);
      BTSerial.print(" Enter value for ");
      if (devID == 0)
        BTSerial.println("all devices");
      else
      {
        BTSerial.print("device ");
        BTSerial.println(devID);
      }
      paramVal = getNumber(0, 1500);
      if (devID == 0)
        for (int i = 0; i < NUM_DEVICES; i++)
        {
          myPrefs.begin(deviceSpace[i], false);
          myPrefs.putUShort("force", paramVal);
          myPrefs.end();
          myStepper[i].setTorqueLimit(paramVal);
        }
      else
      {
        myPrefs.begin(deviceSpace[devID - 1], false);
        myPrefs.putUShort("force", paramVal);
        myPrefs.end();
        myStepper[devID - 1].setTorqueLimit(paramVal);
      }
      break;

    case 'D':
      BTSerial.println("\n Direction (0 or 1, 1 reverses the normal direction of throw\n");
      showCurrentValues('D');
      BTSerial.println("\n Enter 'C' to change or empty line  to exit");
      if (getUpperChar(millis()) != 'C')
        break;
      BTSerial.println(" Enter device (1 - 4) or 0 for all");

      devID = getNumber(0, 4);
      BTSerial.print(" Enter value for ");
      if (devID == 0)
        BTSerial.println("all devices");
      else
      {
        BTSerial.print("device ");
        BTSerial.println(devID);
      }
      paramBool = getNumber(0, 1);
      if (devID == 0)
        for (int i = 0; i < NUM_DEVICES; i++)
        {
          myPrefs.begin(deviceSpace[i], false);
          myPrefs.putBool("reversed", paramBool);
          myPrefs.end();
          myStepper[i].setReversed(paramBool);
        }
      else
      {
        myPrefs.begin(deviceSpace[devID - 1], false);
        myPrefs.putBool("reversed", paramBool);
        myPrefs.end();
        myStepper[devID - 1].setReversed(paramBool);
      }
      break;

    default:
      return;
    }
  }
}

/*****************************************************************************/
// shows stepper current values for the parameter passed in
void showCurrentValues(char myChar)
{
  BTSerial.println(" Current values");
  for (int i = 0; i < NUM_DEVICES; i++)
  {
    BTSerial.print("  Device ");
    BTSerial.print(i + 1);
    BTSerial.print("  ");
    switch (myChar)
    {
    case 'S':
      BTSerial.println(myStepper[i].getSpeed());
      break;

    case 'T':
      BTSerial.println(myStepper[i].getStrokeSteps());
      break;

    case 'F':
      BTSerial.println(myStepper[i].getTorqueLimit());
      break;

    case 'D':
      BTSerial.println(myStepper[i].getReversed());
      break;

    default:
      break;
    }
  }
}
/*****************************************************************************/
// configures wifi SSID and password
void setCredentials()
{
  String myString;
  String wifiString;

  BTSerial.print("\nEnter SSID: ");
  while (!BTSerial.available())
  {
  }
  myString = BTSerial.readString();
  myString.trim();
  if (myString.length() == 0)
    return;

  wifiString = myString;
  BTSerial.print("\nEnter WiFi password: ");
  while (!BTSerial.available())
  {
  }
  myString = BTSerial.readString();
  myString.trim();
  if (myString.length() == 0)
    return;

  myPrefs.begin("general", false);
  myPrefs.putString("SSID", wifiString);
  BTSerial.print("SSID changed to ");
  BTSerial.println(wifiString);
  myPrefs.putString("wifipassword", myString);
  myPrefs.end();
  BTSerial.print("Password changed to ");
  BTSerial.println(myString);
  BTSerial.println("\nReboot is required");
  return;
}

/*****************************************************************************/
// configure the MQTT server ip address
void setMQTT()
{
  String myString;
  BTSerial.print("\nEnter MQTT server IP address: ");
  while (!BTSerial.available())
  {
  }
  myString = BTSerial.readString();
  myString.trim();
  if (myString.length() == 0)
    return;
  myPrefs.begin("general", false);
  myPrefs.putString("mqttserver", myString);
  myPrefs.end();
  BTSerial.print("Changed to ");
  BTSerial.println(myString);
  BTSerial.println("\nReboot is required");
  return;
}

/*****************************************************************************/
// gets one character and converts to upper case, clears input buffer of C/R and newline
char getUpperChar(uint32_t invokeTime)
{
  char _myChar;

  while (true)
  {
    if (invokeTime > 0)
    {
      if (timeout(invokeTime))
        return ' '; // operator dozed off
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

/*****************************************************************************/
void print32(uint32_t Val)
{
  BTSerial.println(Val);
}
