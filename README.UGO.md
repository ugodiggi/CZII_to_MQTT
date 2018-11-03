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
