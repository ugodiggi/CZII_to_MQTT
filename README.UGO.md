# 2018-11-03 19:07:28
## Done
* Put the library in my Arduino dir (~/Documents/Arduino)
* Renamed the library's folder to ComfortZone (apparently the .ino file needs to have the same name
  as the directory of the sketch.
* Opened the sketch in Arduino (1.8.5) and tried to compile.
* Added *a lot* of libraries (Sketch > Include Library > Manage Libraries...) that were missing.
  IIRC:
  * Adafruit MQTT
  * WiFiManager
  * ArduinoJson (had to downgrade this to version 5.something like 13 since version 6 is
    incompatible).
  * A library named Time that was listed in the manager under a different name.
* At this point, it is compiling.
## Next steps
Try to figure out what should be the wiring.

# 2018-11-03 19:51:31
## Done
Actually, I decided to dumb-down the code and to read through it.
I moved some code around, added the wifi password, and tried to see if I could get it to connect
and maybe show some sign of life.
I'm looking at the Serial Monitor, and it does not seem to be printing anything attention worthy.
I'm going to try and add some more logs.

# 2018-11-03 22:15:26
## Done
Actual connection:
* Use black cable to connect ground from 485 to ground in the esp8266.
* Use red cable to connect VCC from 485 to 3V in the esp8266.
* Put 220R resistor between pin A and B in 485. Some doc
  (http://pskillenrules.blogspot.com/2009/08/arduino-and-rs485.html) suggested a 150R but I don't
  have one :-/
* Connected DI (driver in) pin from 485 to D6 (SSerialTX in .ino) on esp8266
* Connected DE, RE (driver, receiver enable) pins from 485 together and to D3 (SSerialTxControl in
  .ino) on esp8266
* Connected RO (receiver out) pin from 485 to D5 (SSerialRX in .ino) on esp8266

Also, uncommented setupRs485Stream() and processRs485InputStream.
Let's see if it works right away.
## Next Steps
Try to actually run this, see that it does at least flash and start. Then maybe connect to the
actual CZII?

# 2018-11-04 10:20:02
## Done
* Established (reading i.e.
  https://store.chipkin.com/articles/rs485-rs485-cables-why-you-need-3-wires-for-2-two-wire-rs485
  http://www.elanportal.com/supportdocs/catalog/carrier_czii.pdf
  https://dms.hvacpartners.com/docs/1009/public/0f/zonekit-4si.pdf)
  what is the way I want to cable this:
  * R+ in the CZII to B in the 485
  * R- in the CZII to A in the 485
  * VG in the CZII to the ground in the 485 and to the esp8266
* One **major fuckup** - connected the 485 to the V+ in the CZII. That carries 12V (I did not check
  until later :-) ) and literally burned the 485 in ~3 sec. One down three more to go.

That said, with a new 485... SUCCESS in reading out some data.
I added it here in `sample_data/20181104_1021.txt`.
At a glance, I do not see successful frames, so I'll have to read more code.

## Next Steps
* I am planning to use the 12V from the CZII to power the whole thing.
* Actually read the serial output and try to figure out exactly what happened.

# 2018-11-12 22:31:04
## Done
* I figured out that most likely the data that I had acquired the last time was bit-inverted.
  This would make sense if the cables are inverted, so I tried inverting the R+/R- cabling.
  * R+ in the CZII to A in the 485
  * R- in the CZII to B in the 485
  * VG in the CZII to the ground in the 485 and to the esp8266
  I have no real idea why this would make sense, but the data now does work for me.
* A couple small issues since the MQTT queues were uninitialized but being sent-to.
  I installed https://github.com/me-no-dev/EspExceptionDecoder to symbolize the stack trace and it
  worked beautifully.
* Acquired some data - see `sample_data/20181112_1913.txt`. Have not had time to look at it yet,
  but it seemed like it has bee acquiring with no snags.

## Next Steps
* Look at the data
* Maybe re-enable the MQTT part of the code, since it is currently commented out and could be
  tested without the real data acquisition.

# 2018-11-23 12:36:32
## Current status:
* Acquiring data is working
* Comm with mqtt running on my laptop (in container) is working
* OpenHAB running on my laptop (in container) is working

## Next Steps
* Create WebServer for mesquitto + OpenHAB
  * Try using AWS or Google Cloud
* Create "Release" on the esp8266:
  * Enable data acquisition from 485
  * Enable mqtt connection
  * Add some mqtt pulse signal, and some mqtt input from OpenHAB
  * Enable polling of the CZII for info
* Test release before physically plugging to CZII
  * mqtt pulse signal should be functional
  * ensure that I have a reasonable way to log everything that is sent to mesquitto
* Plug in esp8266 to CZII:
  * Use CZII for power (should go through esp8266 directly, should check that the voltage is
    right).

The hope is that I will be able to:
* read the temperature for each of the three zones
* maybe read the program?
* hopefully, set the temperature and even better set the program.
