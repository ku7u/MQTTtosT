# MQTTtosT
This software provides a means to control track switches (turnouts) on a model railroad. It should be used in conjunction with JMRI software and an MQTT server although the system will work standalone. The software runs on an ESP32 36 pin devkit device.

The ESP32 device will control four micro stepper motors through four motor driver boards mounted on the mother board. Four inputs are provided to allow local control of the motors using push button switches. An output to drive a string of four neoPixel LED devices is provided. These LEDs will display locally the status of each of the turnouts. The colors are red for thrown, green for closed and flashing yellow while in transition. The LEDs initially display blue until the setup routine has run.

The micro stepper actuators are extremely small, on the order of 3/4" square with a height of less than 1/4". They can be placed on the surface of the layout, under the turnout throw bar, and hidden with scenery materials. They will self adjust for throw in this application.

Primary control of the turnout motors is by using MQTT over WiFi. The source of commands is expected to be JMRI software, however other MQTT capable sources may be used, such as an app on a cell phone or a tablet. The software will send turnout status data back to JMRI at the end of each commanded movement.

A gerber file for the required motherboard PCB is included in this distribution. I can be sent as is to a PCB fabrication shop such as JLCPCB.

## Important considerations
All configuration is done using the supplied menuing system via Bluetooth. A Bluetooth Serial app is required on a phone or tablet. I recommend BlueTooth Serial on Android by Kai Morich. Others I have tried all had issues. Connect your device to the ESP32 as soon as the software has been booted.

This software was developed using VSCode and Arduino avrisp with 'ESP32 Dev Module' board configuration. Initially it will fail to load into the host because of lack of memory. This is alleviated by modifying the partition scheme. Access that setting from the Arduino Tools menu. Select some scheme that does not use OTA (Over the Air software loading). Choosing this reduced the size from 101% of memory to 42%.

On first start the code will attemp to connect to the default WiFi SSID and will fail. At that point connect the Bluetooth Serial app to the device and enter a blank line. The menu should then display. Using the menu the WiFi SSID and password can be entered. At the same menu enter the MQTT server address. Provide a unique name for the node. This will be used for both MQTT and Bluetooth.

A Bluetooth password can be supplied to prevent local hackers from tinkering with the device. However they can still connect. There is a menu choice to turn off Bluetooth completely. To turn it back on ground pin 5 briefly. This also sets the password to the default 'IGNORE' which is in effect no password so the operator must either ignore the threat, set a new password or turn off Bluetooth again.

After configuring the above, reboot the device using the menu.

The left hand part of the MQTT topic set in the device must match the corresponding setting in JMRI. The default in JMRI is "/trains/track/turnout/". The leading slash is incorrect, remove it in JMRI. The default topic left hand part in the device is "trains/track/turnout/. This string can be changed using the menu, but it must match the equivalent string in JMRI.

The topic string must be specified in a certain way in JMRI. The inbound sensor topic must be left as is, namely 'track/sensor/'. The outbound sensor topic must be changed to 'track/sensor/send/' to be compatible with the block zeroing (ghostbuster) feature. The system name in JMRI for a sensor must be of the form 'BOD/block/\<blockID\>'. Topic strings are case sensitive. Resulting data sent from the device will be 'trains/track/sensor/BOD/block/\<blockID\>' which is a little wordy but helps to avoid confusion when troubleshooting. It also allows for other kinds of sensors with names other than 'BOD' to be configured. 'blockID' must be a number.

## Versioning
Any change that affects operation increase the version number most significant digit. Any change that corrects an error will increase the version number middle digit. A change that does neither such as correcting a typo will increase the least significant digit.
