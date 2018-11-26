// vi: tabstop=2
// Copyright - Jared Gaillard - 2016
// MIT License
//
// Arduino CZII Project
//
//   https://github.com/jwarcd/CZII_to_MQTT
//
//   Sketch to connect to Carrier Comfort Zone II (CZII) RS485 serial bus and send data to
//   and from a MQTT feed
//
//   Uses a MAX485 RS-485 TTL to RS485 MAX485CSA Converter Module For Arduino
//      Wiring should be:
//      CZII RS+ = RS-485 B+
//      CZII RS- = RS-485 A-
//      CZII VG  = RS-485 ground
//      NOTE(ugo): For me the current wiring is:
//      CZII RS+ = RS-485 A
//      CZII RS- = RS-485 B
//      CZII VG  = RS-485 ground
//      I have no explanation why but it works.
//
//   Must use ESP8266 Arduino from:
//      https://github.com/esp8266/Arduino

#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>

#include "Action.h"
#include "MqttController.h"
#include "RingBuffer.h"
#include "ComfortZoneII.h"
#include "Util.h"

// WlanCredentials.h is not in git (see also .gitignore).
// It must contain your SSID credentials, i.e.
//   #ifndef _WLANCREDENTIALS__H_
//   #define _WLANCREDENTIALS__H_
//   #define WLAN_SSID         "YOUR_SSID"     // WiFi SSID here
//   #define WLAN_PASS         "YOUR_PASSWORD" // WiFi password here
//   #endif  // _WLANCREDENTIALS__H_
#include "WlanCredentials.h"

MqttController mqttController;

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;                              // or... use WiFiClientSecure for SSL

const char*              hostName = "CZII";     //
ESP8266WebServer         webServer(80);         // Http server we will be providing
ESP8266HTTPUpdateServer  httpUpdater(false);    // A OverTheAir update service.


// RS485 Software Serial
#define SSerialRX         D5                    // RS485 Serial Receive pin
#define SSerialTX         D6                    // RS485 Serial Transmit pin
#define SSerialTxControl  D3                    // RS485 Direction control
#define RS485Transmit     HIGH
#define RS485Receive      LOW

SoftwareSerial* rs485;

// CZII Configuration
#define NUM_ZONES 3
ComfortZoneII CzII((byte)NUM_ZONES);

// CZII Commands
#define COMMAND_TIME_PERIOD     10000
#define DEVICE_ADDRESS          99
byte REQUEST_INFO_TEMPLATE[]          = {1, 0, DEVICE_ADDRESS, 0, 3, 0, 0, 11, 0, 255, 255, 0, 0};  // Note: Replace the table and row values, and calc checksum before sending out request
byte SET_ZONE_TEMPERATURE_TEMPLATE[]  = {
  // 0  1  2               3  4   5  6  7   8  9  10  11  12  13  14  15  16  17  18  19  20  21
     1, 0, DEVICE_ADDRESS, 0, 19, 0, 0, 12, 0, 1, 16, 76, 76, 76, 76, 76, 76, 76, 76, 68, 68, 68,
  // 22  23  24  25  26  27   28
     68, 68, 68, 68, 68, 255, 255};

byte TABLE1_POLLING_ROWS[] = {6, TABLE1_TEMPS_ROW, TABLE1_TIME_ROW};
byte rowIndex = 0;

unsigned long lastSendTimeMillis = 0;
unsigned long lastPollingTimeMillis = COMMAND_TIME_PERIOD;   // Delay 10 seconds on startup before sending commands
unsigned long lastReceivedMessageTimeMillis = 0;

// Input/output ring buffers
RingBuffer rs485InputBuf;
RingBuffer rs485OutputBuf;
RingBuffer serialInputBuf;
String serialInputByte;

void webServerHandleRoot();
void configModeCallback();
void setupRs485Stream();

// ensure wifi connection.
void ensureWifiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  // Connect to WiFi access point.
  Serial.println();
  Serial.print(F("Connecting to WiFi SSID: "));
  Serial.println(WLAN_SSID);
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  // Establish a connection with our configured access point
  Serial.print(F("Connecting."));
  while(WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println(F("."));

  Serial.println(F("WiFi connected"));
  IPAddress ip = WiFi.localIP();
  Serial.println("IP address: " + ip.toString());

  webServer.on("/", webServerHandleRoot);

  // Add OTA update service provided by library "/update" command
  httpUpdater.setup(&webServer);
  webServer.begin();

  MDNS.begin(hostName);
  MDNS.addService("http", "tcp", 80);
}

void applyAction(Action *a) {
  if (a->zone < CzII.numZones()) {
    bool sendData = false;
    for (int i = 0; i < CzII.numZones(); ++i) {
      Zone* zone =  CzII.getZone(i);
      byte zoneHeatSetpoint = zone->getHeatSetpoint();
      byte zoneCoolSetpoint = zone->getCoolSetpoint();
      if (a->zone == i) {
        zoneHeatSetpoint = a->heatSetpoint;
        zoneCoolSetpoint = a->coolSetpoint;
        sendData = true;
      }
      // Update temperatures
      SET_ZONE_TEMPERATURE_TEMPLATE[11 + i] = zoneCoolSetpoint;  // 11, 12, 13, ...
      SET_ZONE_TEMPERATURE_TEMPLATE[19 + i] = zoneHeatSetpoint;  // 19, 20, 21, ...
    }

    if (sendData) {
      rs485_EnqueFrame(SET_ZONE_TEMPERATURE_TEMPLATE, array_len(SET_ZONE_TEMPERATURE_TEMPLATE));

      // Request latest data
      REQUEST_INFO_TEMPLATE[9] = 1;   // Table = 1
      REQUEST_INFO_TEMPLATE[10] = 16;
      rs485_EnqueFrame(REQUEST_INFO_TEMPLATE, array_len(REQUEST_INFO_TEMPLATE));
    }
  }
}

void processMqttInput() {
  mqttController.ensureConnected();
  Action *a;
  while ((a = mqttController.processMqttInput()) != NULL) {
    applyAction(a);
  }
}

///////////////////////////////////////////////////////////////////
//
// RS485 Serial Stream
//
///////////////////////////////////////////////////////////////////

void setupRs485Stream() {
  pinMode(SSerialTxControl, OUTPUT);
  digitalWrite(SSerialTxControl, RS485Receive);  // Init Transceiver
  rs485 = new SoftwareSerial(SSerialRX, SSerialTX, false, 256);
  rs485->begin(9600);
}

//  Process input data from the rs485 Serial stream.
//
//  We look for valid CZII data frames, convert to JSON and then send to the MQTT server.
void processRs485InputStream() {
  // Process input data
  while (rs485->available() > 0) {
    if (!rs485InputBuf.add((byte)rs485Read())) {
      info_println(F("ERROR: INPUT BUFFER OVERRUN!"));
    }
    if (processInputFrame()) {
      debug_println(F("FOUND GOOD CZII FRAME!"));
    }
    lastReceivedMessageTimeMillis = millis();
  }
}

int rs485Read() {
  delay(0);   // So we don't get watchdog resets
  return rs485->read();
}

//
//  Process input data from the Serial stream. This data is converted from ASCII to byte values and
//  written out on the rs485 stream.
//
//  Data is expected to be in the form of string representing byte values:
//       "1.0  99.0  19  0.0.12   0.1.16. 78.77.76.76.76.76.76.76. 68.67.68.68.68.68.68.68. "
//
//       - Valid delimiters between bytes are  ' ', '.', or ','
//       - Each line (frame) must be terminated with a '|', '\n', or '\r'
//       - The checksum and will be automatically calculated and therefore should not be included
//
//      (This can be used to send test message frames to the CZII from an external source via the Serial port)
//
void processSerialInputStream() {
  while (Serial.available() > 0) {
    byte input = Serial.read();
    bool processByteString = false;

    if (input == ' ' || input == '.' || input == ',') { // space, dot, or comma = value delimiter
      processOutputByteString();
      continue;
    } else if (input == '|' || input == '\n' || input == '\r') { // message delimiter: new line, or line feed
      processOutputByteString();
      if (processSerialInputFrame()) {
        debug_println(F("FOUND GOOD FRAME!"));
      }
      serialInputBuf.reset();
      continue;
    }

    serialInputByte = serialInputByte + String(input - 48);
  }
}

void processOutputByteString() {
  if (serialInputByte.length() != 0) {
    // convert to byte and add to buffer
    byte value = (byte)serialInputByte.toInt();
    serialInputBuf.add(value);
    serialInputByte = "";
  }
}

//
//  Process the bytes received from the serial port.  Calculate checksum before sending data
//  out the rs485 port on the CZII bus.
//
bool processSerialInputFrame()
{
  short bufferLength = serialInputBuf.length();

  // Figure out length of buffer
  if (bufferLength < ComfortZoneII::MIN_MESSAGE_SIZE - 2) {
    debug_println("serialInputBuf bufferLength < MIN_MESSAGE_SIZE");
    return false;
  }

  byte source = serialInputBuf.peek(ComfortZoneII::SOURCE_ADDRESS_POS);
  byte destination = serialInputBuf.peek(ComfortZoneII::DEST_ADDRESS_POS);
  byte dataLength = serialInputBuf.peek(ComfortZoneII::DATA_LENGTH_POS);
  byte function = serialInputBuf.peek(ComfortZoneII::FUNCTION_POS);

  debug_println("serialInputBuf: source=" + String(source) + ", destination=" + String(destination) + ", dataLength=" + String(dataLength) + ", function=" + String(function));

  byte frameLength = (byte)(ComfortZoneII::DATA_START_POS + dataLength + 2);

  if (frameLength != (bufferLength + 2)) {
    debug_println("serialInputBuf: **frameLength != bufferLength" + String(frameLength) + ", bufferLength = " + String(bufferLength));
    return false;
  }

  // Add checksum
  byte checksum1 = frameLength - 2;
  unsigned short crc = ModRTU_CRC(serialInputBuf, checksum1);
  serialInputBuf.add(lowByte(crc));
  serialInputBuf.add(highByte(crc));

  dumpFrame(serialInputBuf);

  rs485TransmitFrame(serialInputBuf);
  return true;
}

//
//  Send the next queued output frame
//
void sendOutputFrame() {
  if (rs485OutputBuf.length() == 0) {
    return;
  }

  unsigned long now = millis();
  int sendTimeDtMillis = now - lastSendTimeMillis;
  int lastMessageTimeDtMillis = now - lastReceivedMessageTimeMillis;

  // Try to reduce bus contention by delaying 300 ms since last received message (frame) before
  // we send anything out.
  if (rowIndex == 0 && lastMessageTimeDtMillis < 300) {
    return;
  }

  if (sendTimeDtMillis < 100) {
    return;
  }

  rs485TransmitFrame(rs485OutputBuf);

  lastSendTimeMillis = now;
}

void rs485_EnqueFrame(byte values[], byte size) {
  if (rs485OutputBuf.length() + size > RingBuffer::MAX_BUFFER_SIZE) {
    info_println("ERROR: rs485_EnqueFrame: skipping frame, rs485OutputBuf not large enough");
    return;
  }

  // update checksum
  byte checksum1 =  size - 2;
  unsigned short crc = ModRTU_CRC(values, checksum1);
  values[checksum1] = lowByte(crc);
  values[checksum1 + 1] = highByte(crc);

  for (byte i = 0; i < size; i++) {
    byte value = values[i];
    rs485OutputBuf.add(value);
  }
}

void rs485TransmitFrame(RingBuffer& ringBuffer) {
  short bufferLength = ringBuffer.length();
  if (bufferLength == 0) {
    return;
  }

  info_print("OUTPUT: ");

  if (bufferLength < ComfortZoneII::DATA_LENGTH_POS) {
    info_println("rs485TransmitFrame: not enough data");
    return;
  }

  byte dataLength = ringBuffer.peek(ComfortZoneII::DATA_LENGTH_POS);
  byte frameLength = (byte)(ComfortZoneII::DATA_START_POS + dataLength + 2);

  if (bufferLength < frameLength) {
    info_println("rs485TransmitFrame: not enough data");
    return;
  }

  dumpFrame(ringBuffer);

  digitalWrite(SSerialTxControl, RS485Transmit);

  int index = 0;
  while (index < frameLength) {
    rs485->write(ringBuffer.read());
    index++;
  }

  digitalWrite(SSerialTxControl, RS485Receive);
}

void sendPollingCommands() {
  unsigned long now = millis();
  int dtMillis = now - lastPollingTimeMillis;

  // Try to reduce bus contention by delaying 1000ms since last received message (frame) before
  // we send anything out.
  // If it hasn't been at least COMMAND_TIME_PERIOD milliseconds since last command
  // or the last message received time is less than a second then return
  if (dtMillis < COMMAND_TIME_PERIOD) {
    return;
  }

  for (int i = 0; i < array_len(TABLE1_POLLING_ROWS); i++) {
    REQUEST_INFO_TEMPLATE[9] = 1;   // Table = 1
    REQUEST_INFO_TEMPLATE[10] = TABLE1_POLLING_ROWS[i];
    rs485_EnqueFrame(REQUEST_INFO_TEMPLATE, array_len(REQUEST_INFO_TEMPLATE));
  }

  lastPollingTimeMillis = now;
}

//
//  This method detects if the current buffer has a valid data frame.  If none is found the buffer is shifted
//  and we return false.
//
//  Carrier Comfort Zone || (CZII) data frame structure:
//    For more info see: https://github.com/jwarcd/CZII_to_MQTT/wiki/CZII-Serial-Protocol
//    (Note: Similar to the Carrier Infinity protocol: https://github.com/nebulous/infinitude/wiki/Infinity-serial-protocol)
//
//   |-----------------------------------------------------------------------------------|
//   |                                       Frame                                       |
//   |-----------------------------------------------------------------------------------|
//   |                      Header                          |           |                |
//   |-------------------------------------------------------           |                |
//   | 2 bytes | 2 bytes | 1 byte |  2 bytes  | 1 byte      |   Data    |   Checksum     |
//   |-----------------------------------------------------------------------------------|
//   | Dest    | Source  | Data   | Reserved  | Function    |  0-255    |    2 bytes     |
//   | Address | Address | Length |           |             |  bytes    |                |
//   |-----------------------------------------------------------------------------------|
//
//    Example Data: 9 0   1 0   3   0 0 11   0 9 1   213 184
//    Destination = 9
//    Source      = 1
//    Data Length = 3
//    Function    = 11         (Read Request)
//    Data        = 0 9 1      (Table 9, Row 1)
//    Checksum    = 213 184
//
//   CZII Function Codes:
//    6 (0x06) Response
//       1 Byte Length, Data=0x00 – Seems to be an ACK to a write
//       Variable Length > 3 bytes – a response to a read request
//    11 (0x0B) Read Request
//       3 byte Length, Data=Table and row of data to get
//    12 (0x0C) Write Request
//       Variable Length > 3 bytes
//       First 3 bytes of data are table and row to write to
//       Following bytes are data to write
//    21 (0x15) Error
//       1 Byte Length, Data=0x00
//
bool processInputFrame() {
  digitalWrite(BUILTIN_LED, LOW);  // Flash LED to indicate a frame is being processed

  short bufferLength = rs485InputBuf.length();

  // see if the buffer has at least the minimum size for a frame
  if (bufferLength < ComfortZoneII::MIN_MESSAGE_SIZE) {
    //debug_println("rs485InputBuf: bufferLength < MIN_MESSAGE_SIZE");
    return false;
  }

  byte source = rs485InputBuf.peek(ComfortZoneII::SOURCE_ADDRESS_POS);
  byte destination = rs485InputBuf.peek(ComfortZoneII::DEST_ADDRESS_POS);
  byte dataLength = rs485InputBuf.peek(ComfortZoneII::DATA_LENGTH_POS);
  byte function = rs485InputBuf.peek(ComfortZoneII::FUNCTION_POS);

  //debug_println("rs485InputBuf: source=" + String(source) + ", destination=" + String(destination) + ", dataLength=" + String(dataLength) + ", function=" + String(function));

  short checksum1Pos = ComfortZoneII::DATA_START_POS + dataLength;
  short checksum2Pos = checksum1Pos + 1;
  short frameLength = checksum2Pos + 1;

  // Make sure we have enough data for this frame
  short frameBufferDiff =  frameLength - bufferLength;
  if (frameBufferDiff > 0 && frameBufferDiff < 30) {
    // Don't have enough data yet, wait for another byte...
    debug_print(".");
    return false;
  }

  debug_println();

  byte checkSum1 = rs485InputBuf.peek(checksum1Pos);
  byte checkSum2 = rs485InputBuf.peek(checksum2Pos);

  unsigned short crc = ModRTU_CRC(rs485InputBuf, checksum1Pos);
  byte high = highByte(crc);
  byte low = lowByte(crc);

  if (checkSum2 != high || checkSum1 != low) {
    info_println(F("CRC failed, shifting buffer..."));
    info_println("rs485InputBuf: checkSum1=" + String(checkSum1) + ", checkSum2=" + String(checkSum2) + ", crc=" + String(crc) + ", high=" + String(high) + ", low=" + String(low));
    rs485InputBuf.dump(bufferLength);
    info_println();
    rs485InputBuf.shift(1);
    return false;
  }

  publishCZIIData(rs485InputBuf);

  rs485InputBuf.shift(frameLength);

  digitalWrite(BUILTIN_LED, HIGH);  // Flash LED while processing frames

  return true;
}

//  Publish CZII data to the MQTT feed
void publishCZIIData(RingBuffer ringBuffer) {
  info_print("RS485: ");
  dumpFrame(ringBuffer);

  CzII.update(ringBuffer);

  if (CzII.isZoneModified()) {
    CzII.clearZoneModified();

    String output = CzII.toZoneJson();
    mqttController.publishToZoneFeed(output);
  }

  if (CzII.isStatusModified()) {
    CzII.clearStatusModified();

    String output = CzII.toStatusJson();
    mqttController.publishToStatusFeed(output);
  }
}

//
//   Debug dump of the current frame including the checksum bytes.  Spaces are inserted for
//   readability between the major sections of the frame.
//
void dumpFrame(RingBuffer ringBuffer) {

  if (ringBuffer.length() == 0) {
    return;
  }

  // Destination
  Serial.print(String(ringBuffer.peek(ComfortZoneII::DEST_ADDRESS_POS)) + "." + String(ringBuffer.peek(ComfortZoneII::DEST_ADDRESS_POS + 1)));

  // Source
  Serial.print("  " + String(ringBuffer.peek(ComfortZoneII::SOURCE_ADDRESS_POS)) + "." + String(ringBuffer.peek(ComfortZoneII::SOURCE_ADDRESS_POS + 1)));

  // Data Size
  byte dataLength = ringBuffer.peek(ComfortZoneII::DATA_LENGTH_POS);
  Serial.print("  " + String(dataLength));

  // Function
  if (dataLength < 10) {
    Serial.print(" ");  // add extra space
  }
  byte function = ringBuffer.peek(ComfortZoneII::FUNCTION_POS);
  Serial.print("  " + String(ringBuffer.peek(ComfortZoneII::FUNCTION_POS - 2)) + "." + String(ringBuffer.peek(ComfortZoneII::FUNCTION_POS - 1)) + "." + String(function));

  // Data
  if (function < 10) {
    Serial.print(" ");  // add extra space
  }

  delay(0);

  Serial.print("  ");
  short totalDataTextLength = 0;
  for (byte pos = ComfortZoneII::DATA_START_POS; pos < (ComfortZoneII::DATA_START_POS + dataLength); pos++) {
    String text = String(ringBuffer.peek(pos)) + ".";
    totalDataTextLength += text.length();
    Serial.print(text);
  }
  for (byte i = 0; i < (60 - totalDataTextLength); i++) {
    Serial.print(" ");
  }

  // Checksum
  byte crcHighByte = ringBuffer.peek(ComfortZoneII::DATA_START_POS + dataLength);
  byte crcLowByte = ringBuffer.peek(ComfortZoneII::DATA_START_POS + dataLength + 1);
  Serial.print("  " + String(crcHighByte) + "." + String(crcLowByte));

  Serial.println();
}

///////////////////////////////////////////////////////////////////
//
// Web Server section
//
///////////////////////////////////////////////////////////////////
void webServerHandleRoot()
{
  String message;
  message += hostName;

  message += " \n";
  message += "Commands : /update \n";

  webServer.send(200, "text/plain", message);
}

///////////////////////////////////////////////////////////////////
//
// Actual Arduino entrypoints: setup and loop.
//
void setup() {
  pinMode(BUILTIN_LED, OUTPUT);

  ensureWifiConnected();
  Serial.begin(115200);
  /*while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }*/

  setupRs485Stream();

  Serial.println();
  Serial.println(F("Starting..."));

  mqttController.setup(&client);
}

// Main application loop
void loop() {
  ensureWifiConnected();
  webServer.handleClient();
  processMqttInput();
  mqttController.publishPulse();
  processRs485InputStream();
  processSerialInputStream();
  sendPollingCommands();
  sendOutputFrame();
}
