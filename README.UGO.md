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
