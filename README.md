# MQTTtosT
# MQTTtosO
This software provides a means of block occupancy detection on a model railroad. It must be used in conjunction with JMRI software and an MQTT server.

## Important considerations
All configuration is done using the supplied menuing system via Bluetooth. A Bluetooth Serial app is required on your phone or tablet. I recommend BlueTooth Serial on Android by Kai Morich. Others I have tried all had issues. Connect your device to the ESP32 as soon as the software has been booted.

This software was developed using VSCode and Arduino avrisp with 'ESP32 Dev Module' board configuration. Initially it will fail to load into the host because of lack of memory. This is alleviated by modifying the partition scheme. Access that setting from the Arduino Tools menu. Select some scheme that does not use OTA (Over the Air software loading). Choosing this reduced the size from 101% of memory to 42%.

On first start the code will attemp to connect to the default WiFi SSID and will fail. At that point connect the Bluetooth Serial app to the device and enter a blank line. The menu should then display. Using the menu the WiFi SSID and password can be entered. At the same menu enter the MQTT server address. Provide a unique name for the node. This will be used for both MQTT and Bluetooth.

A Bluetooth password can be supplied to prevent local hackers from tinkering with the device. However they can still connect. There is a menu choice to turn off Bluetooth completely. To turn it back on ground pin 5 briefly. This also sets the password to the default 'IGNORE' which is in effect no password so the operator must either ignore the threat, set a new password or turn off Bluetooth again.

After configuring the above, reboot the device using the menu.

The left hand part of the MQTT topic set in the device must match the corresponding setting in JMRI. The default in JMRI is "/trains/track/sensor/". The leading slash is incorrect, remove it in JMRI. The default topic left hand part in the device is "trains/track/sensor", without the slash. This string can be changed using the menu, but it must match the equivalent string in JMRI.

The topic string must be specified in a certain way in JMRI. The inbound sensor topic must be left as is, namely 'track/sensor/'. The outbound sensor topic must be changed to 'track/sensor/send/' to be compatible with the block zeroing (ghostbuster) feature. The system name in JMRI for a sensor must be of the form 'BOD/block/\<blockID\>'. Topic strings are case sensitive. Resulting data sent from the device will be 'trains/track/sensor/BOD/block/\<blockID\>' which is a little wordy but helps to avoid confusion when troubleshooting. It also allows for other kinds of sensors with names other than 'BOD' to be configured. 'blockID' must be a number.

Each block must be assigned one and only one 'keeper'. This is very important. A detector can be a keeper for 0, 1 or 2 blocks. The keeper collects all wheel counts for the block and is responsible for reporting to JMRI. Only the keeper reports to JMRI. Detectors that are not keepers send 'looseblock' messages to all other detectors when a wheel passes with data that indicates whether the affected block is increased or decreased. The keeper detector will receive such a message and affect its count for the block based on the message content. This is all setup in the code and no configuration is required of the operator. Note that a node will check all of its 8 possible blocks internally before sending out an MQTT 'looseblock' message so these messages are relatively rare. JMRI does not see these messages as it is not subscribed to them.

Detectors are assigned block IDs from the menu choice 'I'. A block is assigned to both the west and the east side of the detector. If status is not required on one or the other side, enter '0' for the block ID. Whether the detector is to be the block's keeper is assigned from the same menu. Good practice requires that a schematic of the track plan be drawn showing detectors and blocks.

Beginning in version 2.0 a speedometer function was added. This was not bugfree until version 3.0. The speedometer can be turned on or off globally from the menu. If on, the device will send speed messages once per second when a train movement is detected. The MQTT topic will include 'speed/\<detector name\>/\<direction\>. The detector name is configured from the menu. The direction is 'east' or 'west'. The message part will be the speed in miles/hour. The value is highly dependent on detector placement. As of version 4.0 the code still needs a mechanism for calibration of this value.

## Versioning
Any change that affects operation will increase the version number on the left side of the decimal point. Any change that does not affect operation will increase the version number on the right side of the decimal.
