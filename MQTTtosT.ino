

/*
  Pin assignments:
   01  TX0
   02  switch for configuration via bluetooth
   03  RX0
   04  GPIO
   05  GPIO
   12  det #1 E
   13  det #1 W
   14  det #2 E
   15  det #2 W
   16  det #3 E
   17  det #3 W
   18  det #4 E
   19  det #4 W
   21  det #5 E
   22  det #5 W
   23  det #6 E
   25  det #6 W
   26  det #7 E
   27  det #7 W
   32  det #8 E
   33  det #8 W
   34  input only, external pullup required
   35  input only, external pullup required
   36  input only, external pullup required
   39  input only, external pullup required
   */

#include <Preferences.h>
#include <PubSubClient.h>
#include "WiFi.h"
#include <BluetoothSerial.h>
#include "bod.h"
//#define testing

using namespace std;

// stringstream ss;

Preferences myPrefs;
char *deviceSpace[] = {"d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8"};

// const char *ssid = "Emmy Network";
// const char *password = "Rutabaga7";
// const char *mqtt_server = "192.168.0.109";

WiFiClient espClient;
String SSID;
String wifiPassword;

String mqttServer;
String mqttNode;
PubSubClient client(espClient);

BluetoothSerial BTSerial;
String BTname;
String BTpassword;
String pwCandidate;
String pwtest;

// constants and variables - detectors
const uint8_t numDevices = 8;
const byte WEST = 0;
const byte EAST = 1;
// String detName[numDevices];

// detector pins
const uint16_t pinDet1W = 13;
const uint16_t pinDet1E = 12;
const uint16_t pinDet2W = 15;
const uint16_t pinDet2E = 14;
const uint16_t pinDet3W = 17;
const uint16_t pinDet3E = 16;
const uint16_t pinDet4W = 19;
const uint16_t pinDet4E = 18;
const uint16_t pinDet5W = 22;
const uint16_t pinDet5E = 21;
const uint16_t pinDet6W = 25;
const uint16_t pinDet6E = 23;
const uint16_t pinDet7W = 27;
const uint16_t pinDet7E = 26;
const uint16_t pinDet8W = 33;
const uint16_t pinDet8E = 32;

detector bod[] =
    {detector(pinDet1W, pinDet1E),
     detector(pinDet2W, pinDet2E),
     detector(pinDet3W, pinDet3E),
     detector(pinDet4W, pinDet4E),
     detector(pinDet5W, pinDet5E),
     detector(pinDet6W, pinDet6E),
     detector(pinDet7W, pinDet7E),
     detector(pinDet8W, pinDet8E)};

/*****************************************************************************/
void setup()
{
  byte myVal;

  Serial.begin(115200);
  // pinMode(2, INPUT_PULLUP); // this is used for resetting Bluetooth

  myPrefs.begin("general");
  BTname = myPrefs.getString("BTname", "CANtosNode");
  BTpassword = myPrefs.getString("password", "IGNORE");
  SSID = myPrefs.getString("SSID", "none");
  wifiPassword = myPrefs.getString("wifipassword", "none");
  mqttServer = myPrefs.getString("mqttserver", "none");
  mqttNode = myPrefs.getString("mqttnode", "noname");
  myPrefs.end();

  // wifiPassword = "Bogus";

  // Bluetooth
  // myPrefs.begin("general");
  // if (myPrefs.getBool("BTon", true)) TBD always turn on BT on reboot?
  BTSerial.begin(BTname);
  // myPrefs.end();

  // WiFi
  setup_wifi();

  // mqtt
  char mqtt_server[mqttServer.length() + 1]; // converting from string to char array required for client parameter
  strcpy(mqtt_server, mqttServer.c_str());
  uint8_t ip[4];
  sscanf(mqtt_server, "%u.%u.%u.%u", &ip[0], &ip[1], &ip[2], &ip[3]);
  client.setServer(ip, 1883);
  client.setCallback(callback);

  // read the parameters from memory and apply to objects
  for (int i = 0; i < numDevices; i++)
  {
    myPrefs.begin(deviceSpace[i], true);
    bod[i].setBlockWest(myPrefs.getUShort("blockWest", 0)); // TBD the types
    bod[i].setBlockEast(myPrefs.getUShort("blockEast", 0));
    bod[i].setWestKeeper(myPrefs.getBool("westkeeper", false));
    bod[i].setEastKeeper(myPrefs.getBool("eastkeeper", false));
    // detName[i] = myPrefs.getString("name", "noname");
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
          setCredentials();
        // the device will be rebooted at this point after the operator resets credentials
      }
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
  // if (digitalRead(2) == LOW) //TBD fix this
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

  checkDetectors();
}

/*****************************************************************************/
// this is a callback from the mqtt object, made when a subscribed message comes in
void callback(char *topic, byte *message, unsigned int length)
{
  //   Serial.print("Message arrived on topic: ");
  //   Serial.print(topic);
  //   Serial.print(". Message: ");
  // string messageTemp;
  uint16_t blockID;

  for (int i = 0; i < length; i++)
  {
    // Serial.print((char)message[i]);
    // messageTemp += (char)message[i];
  }
  //   Serial.println();

  if (String(topic) == "mqmtTOS/looseBlock/increase")
  {
    // call processDetectors with the blockID and increase
    uint8_t _blockID = message[0];
    procDetectors((byte)_blockID, true);
    BTSerial.println("I processed my own message!");
  }

  if (String(topic) == "mqmtTOS/looseBlock/decrease")
  {
    // call processDetectors with the blockID and decrease
    uint8_t _blockID = message[0];
    procDetectors((byte)blockID, false);
    BTSerial.println("I processed my own message!");
  }
}

/*****************************************************************************/
void reconnect()
{
  bool flasher = false;

  char mqtt_node[mqttNode.length() + 1];
  strcpy(mqtt_node, mqttNode.c_str());

  // Loop until we're reconnected TBD change all Serial to BTSerial (maybe)
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // if (client.connect("mqmtTOS")) // TBD this must be unique among nodes
    if (client.connect(mqtt_node)) // TBD this must be unique among nodes
    {
      Serial.println("connected");
      client.subscribe("mqmtTOS/looseBlock/#"); // accept all mqmtTOS/looseBlock topics
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
          setMQTT();
        // the device will be rebooted at this point after the operator resets mqtt server
      }
      delay(2000);
    }
  }
}

/*****************************************************************************/
// Check each detector to see if it has something to process
// Called from each iteration of the main loop
void checkDetectors()
{
  uint8_t _blockID;
  uint8_t _buf[8];
  bool _result;
  const char topicIncrease[] = "mqmtTOS/looseBlock/increase";
  const char topicDecrease[] = "mqmtTOS/looseBlock/decrease";

  // for (int i = 0; i < 1; i++) // TBD remove this
  for (int i = 0; i < numDevices; i++)
  {
    if (bod[i].check())
    {
      // look for block on destination side which will be increased
      if (bod[i].direction() == WEST)
        _blockID = bod[i].getBlockWest();
      else
        _blockID = bod[i].getBlockEast();
      _result = procDetectors(_blockID, true);

      if ((!_result) && (_blockID > 0))
      {
        // send message to colleagues
        client.publish(topicIncrease, &_blockID, 1);
        printMsg("unkept block increased notice sent");
      }

      // look for block on source side which will be decreased
      if (bod[i].direction() == WEST)
        _blockID = bod[i].getBlockEast();
      else
        _blockID = bod[i].getBlockWest();
      _result = procDetectors(_blockID, false);

      if ((!_result) && (_blockID > 0))
      {
        // send message to colleagues
        client.publish(topicDecrease, &_blockID, 1);
        printMsg("unkept block decreased notice sent");
      }
    }
  }
}

/*****************************************************************************/
// Called from checkDetectors or from incoming message from other nodes
// Returns true if the incoming block was handled here as a block keeper, msg sent to JMRI
//  false if not, so a local call to this proc returning false requires a msg to be sent to other nodes by the caller
//  to find a keeper elsewhere
// A call to this proc from other nodes will not care about the return value as the data is already distributed
bool procDetectors(byte blockID, bool increase)
{
  //   uint8_t _buf[8];
  int _eventID;
  uint8_t _buf[8];
  char numstr[4];
  const char topic[] = "mymqtt/track/sensor/BODblock";
  char fullTopic[100];

  sprintf(numstr, "%d", blockID); 
  strcpy(fullTopic, topic);
  strcat(fullTopic, numstr);

  for (int i = 0; i < numDevices; i++)
  {
    if (bod[i].getBlockWest() == blockID)
    {
      if (bod[i].westKeeper)
      {
        if (increase)
        {
          bod[i].westCount++;
          BTSerial.print("westcount ");
          BTSerial.println(bod[i].westCount);
          if (bod[i].westCount == 1) // went from zero to one so notify JMRI this block now occupied
          {
            // send a message to JMRI
            client.publish(fullTopic, "ACTIVE");
            printMsg("Send to JMRI - WEST occupied");
          }
        }
        else
        {
          if (bod[i].westCount > 0)
          {
            bod[i].westCount--;
            BTSerial.print("westcount ");
            BTSerial.println(bod[i].westCount);
            if (bod[i].westCount == 0) // went from one to zero so notify JMRI this block now unoccupied
            {
              // send a message to JMRI
              client.publish(fullTopic, "INACTIVE");
              printMsg("Send to JMRI - WEST vacant");
            }
          }
        }
        return true;
      }
    }
    if (bod[i].getBlockEast() == blockID)
    {
      if (bod[i].eastKeeper)
      {
        if (increase)
        {
          bod[i].eastCount++;
          BTSerial.print("eastcount ");
          BTSerial.println(bod[i].eastCount);
          if (bod[i].eastCount == 1)
          {
            client.publish(fullTopic, "ACTIVE");
            printMsg("Send to JMRI - EAST occupied");
          }
        }
        else
        {
          if (bod[i].eastCount > 0)
          {
            bod[i].eastCount--;
            BTSerial.print("eastcount ");
            BTSerial.println(bod[i].eastCount);
            if (bod[i].eastCount == 0)
            {
              // send a message to JMRI
              client.publish(fullTopic, "INACTIVE");
              printMsg("Send to JMRI - EAST vacant");
            }
          }
        }
        return true;
      }
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
  BTSerial.println(" 'B' - Set Bluetooth node name");
  BTSerial.println(" 'X' - Set Bluetooth password");
  BTSerial.println(" 'W' - Set WiFi credentials");
  BTSerial.println(" 'M' - Set MQTT server IP address");
  BTSerial.println(" 'N' - Set node name");
  // BTSerial.println(" 'O' - Set optical detector name(s)");
  BTSerial.println(" 'I' - Set block IDs and keeper status");
  BTSerial.println(" 'G' - Ghostbuster");
  BTSerial.println(" 'Z' - Turn off Bluetooth until reset from pin 2");
  BTSerial.println(" 'D' - Debug display on/off");

  BTSerial.println("\n Enter 'R' to return to run mode (automatic after 30 sec of inactivity)");
}

/*****************************************************************************/
// business end of the menu
void configure()
{
  uint16_t devID;
  uint16_t paramVal;
  uint16_t enteredVal;
  uint16_t _detectorNumber;
  bool paramBool;
  String pw;
  String myString;
  bool beenHereDoneThat = false;
  char myChar;

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
    case 'B': // set node name
      BTSerial.print("\nEnter node name (device must be rebooted to take effect): ");
      while (!BTSerial.available())
      {
      }
      BTname = BTSerial.readString();
      BTname.trim();
      myPrefs.begin("general", false);
      myPrefs.putString("BTname", BTname);
      myPrefs.end();
      BTSerial.print("Changed to ");
      BTSerial.println(BTname);
      BTSerial.print("\nReboot now?");
      if (getUpperChar(0) == 'Y')
        delay(3000);
      ESP.restart();
      break;

    // case 'O': // detector names
    //   BTSerial.print("\n Enter detector number (1 - 8), 'Q' to quit: ");
    //   _detectorNumber = getNumber(0, numDevices);
    //   if (_detectorNumber <= 0)
    //     break;
    //   BTSerial.print("\nEnter name: ");
    //   while (!BTSerial.available())
    //   {
    //   }
    //   myString = BTSerial.readString();
    //   myString.trim();
    //   detName[_detectorNumber - 1] = myString;
    //   myPrefs.begin(deviceSpace[_detectorNumber - 1], false);
    //   myPrefs.putString("name", myString);
    //   myPrefs.end();
    //   BTSerial.print("Changed to ");
    //   BTSerial.println(myString);
    //   break;

    case 'I': // block IDs
      wcDetectorConfiguration();
      break;

    case 'G': // ghostbuster
      BTSerial.println("\nEnter block ID to be cleared of ghosts (set to zero occupancy), 'Q' to quit");
      enteredVal = getNumber(0, 255);
      if (ghostBuster(enteredVal))
        BTSerial.println("Ghosts removed");
      else
        BTSerial.println("Block not found on this node");
      break;

    case 'P':
    {
      BTSerial.println("\nCurrent configuration");
      IPAddress ipAdr;
      ipAdr = WiFi.localIP();
      BTSerial.print("Local IP address = ");
      BTSerial.println(ipAdr);
      BTSerial.print("SSID = ");
      BTSerial.println(SSID);
      BTSerial.print("MQTT server = ");
      BTSerial.println(mqttServer);
      BTSerial.print("MQTT node name = ");
      BTSerial.println(mqttNode);
      BTSerial.print("Bluetooth node name = ");
      BTSerial.println(BTname);
      // for (int i = 0; i < numDevices; i++)
      // {
      //   BTSerial.print("Device ");
      //   BTSerial.print(i + 1);
      //   BTSerial.print(" name = ");
      //   BTSerial.println(detName[i]);
      // }
    }
    break;

    case 'X':
      BTSerial.print("\nEnter password: ");
      while (!BTSerial.available())
      {
      }
      pw = BTSerial.readString();
      pw.trim();
      myPrefs.begin("general", false);
      myPrefs.putString("password", pw);
      myPrefs.end();
      BTpassword = pw;
      BTSerial.print("Changed to ");
      BTSerial.println(pw);
      break;

    case 'W':
      setCredentials(); // also called explicitly
      break;

    case 'M':
      setMQTT(); // also called explicitly
      break;

    case 'N':
      BTSerial.println("Enter a name for this node. Used by MQTT, must be unique: ");
      while (!BTSerial.available())
      {
      }
      myString = BTSerial.readString();
      myString.trim();
      myPrefs.begin("general", false);
      myPrefs.putString("mqttnode", myString);
      myPrefs.end();
      BTSerial.print("Changed to ");
      BTSerial.println(myString);
      BTSerial.println("\nDevice will now be rebooted...");
      delay(3000);
      ESP.restart();
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
      bod[devID - 1].setDisplayDetect(myChar = 'Y');
      break;

    case 'R':
      BTSerial.println("\nBack to run mode");
      BTSerial.disconnect();
      return;

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
// assign block IDs and keeper status
void wcDetectorConfiguration()
{
  byte _adr;
  byte _detectorNumber;
  byte _westBlock;
  byte _eastBlock;
  bool _westKeeper;
  bool _eastKeeper;
  bool _saved;

  BTSerial.println("\nDetector configuration menu");
  while (true)
  {
    BTSerial.println(" Current values...");
    for (int i = 0; i < numDevices; i++)
    {
      BTSerial.print("D=");
      BTSerial.print(i + 1);
      BTSerial.print(" West block ID=");
      BTSerial.print(bod[i].getBlockWest());
      BTSerial.print(" ");
      if (bod[i].westKeeper)
        BTSerial.print("K ");
      else
        BTSerial.print("  ");
      BTSerial.print(" East block ID=");
      BTSerial.print(bod[i].getBlockEast());
      BTSerial.print(" ");
      if (bod[i].eastKeeper)
        BTSerial.print("K ");
      BTSerial.println(" ");
    }

    BTSerial.print("\n Enter detector number (1 - 8), 'Q' to quit: ");
    _detectorNumber = getNumber(0, numDevices);
    if (_detectorNumber <= 0)
      return;

    _westBlock = bod[_detectorNumber - 1].getBlockWest();
    _westKeeper = bod[_detectorNumber - 1].westKeeper;
    _eastBlock = bod[_detectorNumber - 1].getBlockEast();
    _eastKeeper = bod[_detectorNumber - 1].eastKeeper;

    BTSerial.print("\n Detector number ");
    BTSerial.println(_detectorNumber);

    _saved = false;

    do
    {
      BTSerial.println(" Enter (W) assign west block, (E) assign east block, (S) save, (Q) quit");
      BTSerial.println(" Enter 0 (zero) for block ID if no block assigned");

      switch (getUpperChar(0))
      {
      case 'W':
        BTSerial.println(" West block ID?");
        _westBlock = getNumber(0, 255);
        BTSerial.println(" Is this detector the aggregator for the assigned block? Y or N");
        _westKeeper = (getUpperChar(0) == 'Y');
        if (_westKeeper)
        {
          BTSerial.print(" Assigned as aggregator for block ");
          BTSerial.println(_westBlock);
          BTSerial.print('\n');
        }
        break;

      case 'E':
        BTSerial.println(" East block ID?");
        _eastBlock = getNumber(0, 255);
        BTSerial.println(" Is this detector the aggregator for the assigned block? Y or N");
        _eastKeeper = (getUpperChar(0) == 'Y');
        if (_eastKeeper)
        {
          BTSerial.print(" Assigned as aggregator for block ");
          BTSerial.println(_eastBlock);
          BTSerial.print('\n');
        }
        break;

      case 'S':
        BTSerial.println(" Saving");
        bod[_detectorNumber - 1].setBlockWest(_westBlock);
        bod[_detectorNumber - 1].setBlockEast(_eastBlock);
        bod[_detectorNumber - 1].setWestKeeper(_westKeeper);
        bod[_detectorNumber - 1].setEastKeeper(_eastKeeper);
        myPrefs.begin(deviceSpace[_detectorNumber - 1], false);
        myPrefs.putUShort("blockWest", (uint16_t)_westBlock); // TBD check types
        myPrefs.putUShort("blockEast", (uint16_t)_eastBlock);
        myPrefs.putBool("westkeeper", _westKeeper);
        myPrefs.putBool("eastkeeper", _eastKeeper);
        myPrefs.end();
        _saved = true;
        break;
        ;

      case 'Q':
        BTSerial.println(F(" Exiting"));
        return;

      default:
        BTSerial.println(F(" Exiting"));
        return;
      }
    } while (_saved == false);
  }
}

/*****************************************************************************/
// scare away ghosts from a block
bool ghostBuster(uint8_t blockID)
{
  for (int i = 0; i < numDevices; i++)
  {
    if ((bod[i].getBlockWest() == blockID) && (bod[i].westKeeper))
    {
      bod[i].westCount = 0;
      return true;
    }
    else if ((bod[i].getBlockEast() == blockID) && (bod[i].eastKeeper))
    {
      bod[i].eastCount = 0;
      return true;
    }
  }
  return false;
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
