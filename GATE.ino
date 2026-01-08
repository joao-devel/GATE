#include <RTCZero.h>
#include <SPI.h>
#include <LoRa.h>
#include <HttpClient.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include <FlashAsEEPROM.h>
#include "LoRaData.h"
#include "ArduinoLowPower.h"
#include <Arduino_PMIC.h>
//                         ____________
const String initString = "A8610AAE58BBFFFF120701c0a84478c0a844ffffffff00c0a844845100000000";
//                         0001020304050607080910111213141516171819202122232425262728293031
const bool isDebug = false;
const float cBatt = 0;

// For battery measurements
int LiPo_BATTERY = A1;
float BatteryLevel;
bool Charging = false;

const int DIGILED = 2;//LED_BUILTIN;
const unsigned int maxBounces = 1;
unsigned int localAddress;
byte syncWord;
byte sf = 7;

unsigned long loraFreq = 860E6;//Base Frequency
const int pwr = 20;

//Ethernet
unsigned int isDHCP = 1;
byte mac[] = {0xA8, 0x61, 0x0A, 0xAE, 0x45, 0x35 };
byte ip[4];
byte gw[4];
byte subnet[4];
EthernetServer server(80);
EthernetClient client;
EthernetClient clientPost;
byte serverIP[4];//{ 192, 168, 68, 117 }; // (IP)

byte val; //usata spesso. Così non la devo ridefinire ogni volta e risparmio memoria
int varInt;

unsigned int interval = 2000;          // interval between sends
unsigned long lastSendTime = 0;        // time of last packet send

byte incomingLength;//per verificare lunghezza messaggio.
RTCZero rtc;
bool justOnce = true;
String dummySendTime = "";
void setup()
{
  pinMode(DIGILED, OUTPUT);
  digitalWrite(DIGILED, HIGH);
  Serial.begin(115200);
  if (isDebug)
  {
    while (!Serial) ; // Wait for serial port to be available
  }

  if (!EEPROM.isValid()) { //Inizializzo con un minimo di valori validi
    //Serial.println("Initializing EEPROM");
    char firstByte[2];

    for (int i = 0; i <= 32; i++)
    {
      //Serial.print(i);
      //Serial.print(" ");
      //Serial.println(initString.substring(i * 2, i * 2 + 2));
      initString.substring(i * 2, i * 2 + 2).toCharArray(firstByte, 3);
      val = strtol(firstByte, NULL, 16);
      EEPROM.write(i, val);
    }
    EEPROM.commit();
  }

  // LOAD FIX VALUES
  //load MAC address - Questo valore va PREIMPOSTATO
  //Serial.print("MAC ");
  for (int i = 0; i < 6; i++) {
    mac[i] = EEPROM.read(i);
    //Serial.print(String(mac[i], HEX));
  }

  //Load Local Address.
  localAddress = EEPROM_readint(6);
  //Serial.print("LOCAL ");
  //Serial.println(localAddress);

  //LOAD VARIABLE VALUES
  //SyncWord
  syncWord = EEPROM.read(8);
  //Serial.print("SyncWord:  ");
  //Serial.println(syncWord);

  //SF
  sf = EEPROM.read(9);
  //Serial.print("SF:  ");
  //Serial.println(sf);
  //Load isDHCP
  isDHCP = (int)EEPROM.read(10);

  //GET Network Adapter data
  if (isDHCP == 0)
  {
    //LOAD OTHER IP PARAMETERS
    for (int i = 0; i < 4; i++)
    {
      ip[i] = EEPROM.read(i + 11);
      gw[i] = EEPROM.read(i + 15);
      subnet[i] = EEPROM.read(i + 19);
    }
    Ethernet.begin(mac, ip, gw, gw, subnet);
  }
  else
  {
    //Serial.println(" with DHCP");
    Ethernet.begin(mac);
  }

  //Load remote server IP
  //Serial.print("Remote server IP: ");
  for (int i = 0; i < 4; i++)
  {
    serverIP[i] = EEPROM.read(i + 23);
    //Serial.print(serverIP[i]);
    //Serial.print(" ");
  }
  //Serial.println();

  //Load Frequency If not set, loads default of 868.1 MHz
  //Serial.println("Frequency: ");
  long varInt = EEPROM.read(27);
  //Serial.println(varInt * 100000);
  if (varInt == 0)
  {
    loraFreq = 868100000;
  }
  else
  {
    loraFreq += varInt * 100000;
  }
  //Serial.println(loraFreq);
  //End EEPROM Load

  for (int dx = 0; dx < 1000; dx++) {
    //Serial.print("@");
    delay(10);
  }

  serialEEPROMupdate();
  digitalWrite(DIGILED, HIGH);

  // detach USB so the chip is asleep
  /*if (!isDebug)
    {
    USBDevice.detach(); //NON STACCARE SENNO NON LEGGE PM25
    }*/

  //Serial.print("Local IP: ");
  //Serial.println(Ethernet.localIP());
  if (String(Ethernet.localIP()) == "255.255.255.255")
  {
    fastLedError();
  }
  //Start tcp
  server.begin();

  //LORA init
  //LoRa.setPins(10, 9, 2);// Richiede una modifica hardware
  if (!LoRa.begin(loraFreq)) {
    //Serial.println("ERRORE LORA BEGIN");
    fastLedError();
    while (true);
  }

  LoRa.enableCrc();
  LoRa.setSyncWord(syncWord);
  LoRa.setSpreadingFactor(sf);           // ranges from 6-12,default 7 see API docs
  LoRa.setTxPower(pwr);

  //setup battery
  analogReference(AR_DEFAULT);
  if (!PMIC.begin()) {
    //Serial.println("Failed to initialize PMIC!");
    fastLedError();
  }

  PMIC.enableBATFET();
  // Set the input current limit to 2 A and the overload input voltage to 3.88 V
  if (!PMIC.setInputCurrentLimit(2.0)) {
    ////Serial.println("Error in set input current limit");
    fastLedError();
  }
  if (!PMIC.setInputVoltageLimit(3.88)) {
    ////Serial.println("Error in set input voltage limit");
    fastLedError();
  }
  // set the minimum voltage used to feeding the module embed on Board
  if (!PMIC.setMinimumSystemVoltage(3.3)) {
    ////Serial.println("Error in set minimum system volage");
    fastLedError();
  }
  // Set the desired charge voltage to 4.2 V
  if (!PMIC.setChargeVoltage(4.1)) {
    ////Serial.println("Error in set charge voltage");
    fastLedError();
  }
  // Set the charge current to less than half of capacity
  if (!PMIC.setChargeCurrent(0.125)) {
    ////Serial.println("Error in set charge current");
    fastLedError();
  }
  if (!PMIC.disableCharge()) {
    ////Serial.println("Error in set charge current");
    fastLedError();
  }
  rtc.begin(); // initialize RTC 24H format
  rtc.setTime(0, 0, 0);
  rtc.setDate(1, 1, 21);

  //Finalizing
  //LoRa.onReceive(onReceive);
  LoRa.receive();
  //Serial.println("Setup completed");
  blinkLed(2);
}

void loop() {
  //Simple web server to manage remote server IP setting
  client = server.available();
  if (client)
  {
    //Serial.println("new client");
    String readString = ""; //used by server to capture GET request
    boolean currentLineIsBlank = true;
    while (client.connected() && !client.available()) delay(1); //waits for data
    while (client.connected() && client.available()) { //connected or data available
      char c = client.read();
      //Serial.print(c);
      if (readString.length() < 100)
      {
        readString += c;
      }
      if (readString.endsWith("|"))
      {
        //Serial.print("MANAGE: ");
        String caso = readString.substring(0, 2);
        //Serial.println(caso);
        int dl = sf * 300 - 1600;
        char firstByte[2];
        if (caso.toInt() == 1) { //EEPROM WRITE
          readString = readString.substring(2, readString.indexOf("|"));
          int k = 6;
          for (int i = 0; i < 64; i += 2)
          {
            readString.substring(i, i + 2).toCharArray(firstByte, 3);
            val = strtol(firstByte, NULL, 16);
            //Serial.print(k);
            //Serial.print(" - ");
            //Serial.println(val);
            EEPROM.write(k, val);
            k++;
          }
          EEPROM.commit();
          //Serial.println("RESET");
          ledReset();
          NVIC_SystemReset();
        }
        else if (caso.toInt() == 2) { //SEND MSG 02DDddidMESSAGE|
          LoRaMessage outMsg;
          byte bb[4];
          readString.substring(2, 4).toCharArray(firstByte, 3);
          bb[0] = strtol(firstByte, NULL, 16);
          readString.substring(4, 6).toCharArray(firstByte, 3);
          bb[1] = strtol(firstByte, NULL, 16);
          outMsg.recipient = Byte2Long(bb);
          outMsg.sender =  localAddress;
          outMsg.ID = readString.substring(6, 8).toInt();
          outMsg.message = readString.substring(8, readString.length() - 1);
          delay(dl);
          sendLora(outMsg);
        }
        else if (caso.toInt() == 3) { //REBOOT
          LoRaMessage outMsg;
          byte bb[4];
          readString.substring(2, 4).toCharArray(firstByte, 3);
          bb[0] = strtol(firstByte, NULL, 16);
          readString.substring(4, 6).toCharArray(firstByte, 3);
          bb[1] = strtol(firstByte, NULL, 16);
          outMsg.recipient = Byte2Long(bb);
          outMsg.sender =  localAddress;
          outMsg.ID = readString.substring(6, 8).toInt();
          outMsg.message = "";
          delay(dl);
          sendLora(outMsg);
        }
        else if (caso.toInt() == 11) { //Remote Server
          readString = readString.substring(2, readString.indexOf("|"));
          int k = 23;
          for (int i = 0; i < 8; i += 2)
          {
            readString.substring(i, i + 2).toCharArray(firstByte, 3);
            val = strtol(firstByte, NULL, 16);
            //Serial.print(k);
            //Serial.print(" - ");
            //Serial.println(val);
            EEPROM.write(k, val);
            k++;
          }
        }
        else if (caso.toInt() == 99) { //DUMP
          for (int i = 6; i < 32; i++)
          {
            //Serial.print(i);
            //Serial.print(": 0x");
            //Serial.println(String(EEPROM.read(i), HEX));
            client.println(String(EEPROM.read(i), HEX));
          }
        }
        client.println("OK");

        digitalWrite(DIGILED, LOW);    // turn the LED off  */
        readString = "";
        currentLineIsBlank = true;
        break;
      }
      if (c == '\n') {
        // you're starting a new line
        currentLineIsBlank = true;
      } else if (c != '\r') {
        // you've gotten a character on the current line
        currentLineIsBlank = false;
      }
    }
    delay(1);
    client.stop();
  }

  //gestione lora
  onReceive(LoRa.parsePacket());

  //BATTERY CHARGER
  if (rtc.getMinutes() % 15 == 0 && justOnce)
  {
    justOnce = false;
    //Serial.println("Entro");
    if (isDHCP == 1) {
      Ethernet.maintain();
    }
    BatteryLevel = getBatteryLevel();
    //Serial.print("Battery: ");
    //Serial.println(BatteryLevel);
    //Serial.print("isPowerGood: ");
    //Serial.println(PMIC.isPowerGood());
    //Serial.print("PMICcheck: ");
    //Serial.println(PMICcheck());
    int PMICval = 0;
    String pc = PMICcheck(PMICval);
    if (PMICval == 1)
    {
      delay(100);
      NVIC_SystemReset();
    }
    if ( PMICval == 2 || BatteryLevel < 3.4f) //Se scendo sotto i 3.3, spengo
    {
      delay(100);
      shutdownDevice();
    }


    LoRaMessage dumpMsg;
    dumpMsg.recipient = localAddress;
    dumpMsg.sender = localAddress;
    dumpMsg.bounces = 0;
    dumpMsg.ID = 96;
    dumpMsg.year = rtc.getYear();
    dumpMsg.month = rtc.getMonth();
    dumpMsg.day = rtc.getDay();
    dumpMsg.hour = rtc.getHours();
    dumpMsg.minute = rtc.getMinutes();
    dumpMsg.second = rtc.getSeconds();
    dumpMsg.message = padZeroHex(BatteryLevel * 100);
    //Serial.println("SendPOST Batteria");
    sendPOST(dumpMsg, dummySendTime, true); //TOLTO PERCHE IN VATICANO NON FUNZIONA. NOn SO COME MAI...

    /*Serial.print("Battery: ");
      Serial.println(BatteryLevel);
      Serial.print("Charging: ");
      Serial.println(Charging);
      Serial.print("PMIC.chargeStatus: ");
      Serial.println(PMIC.chargeStatus());
      Serial.print("PMICcheck: ");
      Serial.println(PMICval);
      Serial.println();*/
    if (!Charging)
    {
      if (BatteryLevel < 3.8f)// && PMIC.isPowerGood() && PMIC.isBattConnected()) //Se USB Attaccato, carico mentre lavoro
      {
        // Enable the Charger
        //Serial.println("Enable Charge mode");
        //Serial.println(BatteryLevel);
        //Serial.println(PMIC.isPowerGood());
        //Serial.println(PMIC.isBattConnected());

        //Serial.println(Charging);
        if (!PMIC.enableCharge()) {
          //Serial.println("Error enabling Charge mode");
          fastLedError();
        }
        Charging = true;
      }
    }
    else
    {
      if (PMICval == 3 || PMIC.chargeStatus() == CHARGE_TERMINATION_DONE || BatteryLevel > 4.2f)//|| !PMIC.isPowerGood() || !PMIC.isBattConnected())
      {
        // Disable the charger
        //Serial.println("Disable Charge mode");
        //Serial.println(BatteryLevel);
        //Serial.println(PMIC.isPowerGood());
        //Serial.println(PMIC.isBattConnected());
        //Serial.println(PMIC.chargeStatus());
        //Serial.println(Charging);
        if (!PMIC.disableCharge()) {
          //Serial.println("Error disabling Charge mode");
          fastLedError();
        }
        Charging = false;
      }
    }
  }
  if (rtc.getMinutes() % 15 != 0 && !justOnce)
  {
    justOnce = true;
  }
}

String PMICcheck(int &retVal) {
  int fault  = NO_CHARGE_FAULT;
  retVal = 0; //just message
  // getChargeFault() returns the charge fault state, the fault could be:
  //   - Thermal shutdown: occurs if internal junction temperature exceeds
  //     the preset limit;
  //   - input over voltage: occurs if VBUS voltage exceeds 18 V;
  //   - charge safety timer expiration: occurs if the charge timer expires.
  fault = PMIC.getChargeFault();
  // getChargeFault() returns charge fault status
  switch (fault) {
    case INPUT_OVER_VOLTAGE:
      retVal = 3; //Suspend charge
      return "Input over voltage fault occurred";
      break;
    case THERMAL_SHUTDOWN:
      retVal = 2; //Shutdown
      return "Thermal shutdown occurred";
      break;
    case CHARGE_SAFETY_TIME_EXPIRED:
      retVal = 1; //Reset
      return "Charge safety timer expired";
      break;
    case NO_CHARGE_FAULT:
      //Serial.println("No Charge fault");
      break;
    default :
      break;
  }

  // The isBatteryInOverVoltage() returns if battery over-voltage fault occurs.
  // When battery over voltage occurs, the charger device immediately disables
  // charge and sets the battery fault bit, in fault register, to high.
  if (PMIC.isBatteryInOverVoltage()) {
    retVal = 2; //Shutdown
    return "Error: battery over voltage fault";
  }

  fault = PMIC.hasBatteryTemperatureFault();
  switch (fault) {
    case NO_TEMPERATURE_FAULT:
      //return "No temperature fault";
      break;
    case LOWER_THRESHOLD_TEMPERATURE_FAULT:
      retVal = 2; //Shutdown
      return "Lower threshold Battery temperature fault";
      break;
    case HIGHER_THRESHOLD_TEMPERATURE_FAULT:
      retVal = 2; //Shutdown
      return "Higher threshold Battery temperature fault";
      break;
    default: break;
  }
  return "";
}

void shutdownDevice()
{
  //Serial.println("THE END");
  //Serial.println();
  LoRa.end();
  digitalWrite(LORA_RESET, LOW);
  digitalWrite(LORA_BOOT0, LOW);
  pinMode(LORA_RESET, INPUT);
  pinMode(LORA_DEFAULT_SS_PIN, INPUT);
  pinMode(LORA_IRQ_DUMB, INPUT);
  pinMode(LORA_BOOT0, INPUT);
  // if you really want to detach the battery call
  //PMIC.disableBATFET(); //QUANDO LA BATTERIA E' IN TAMPONE NON LO DISABILITO, IN MODO CHE ALIMENTATORE COMUNQUE QUALCOSA CARICHI
}
void sendLora(LoRaMessage msg) {
  //while (millis() - lastSendTime < interval);
  msg.txPower = pwr;
  msg.message = strRev(msg.message);
  byte bb[2];
  LoRa.idle();
  LoRa.beginPacket();
  bb[0] = highByte(msg.recipient);
  bb[1] = lowByte(msg.recipient);
  LoRa.write(bb[0]);
  LoRa.write(bb[1]);
  bb[0] = highByte(msg.sender);
  bb[1] = lowByte(msg.sender);
  LoRa.write(bb[0]);
  LoRa.write(bb[1]);
  LoRa.write(msg.ID);
  LoRa.write(msg.bounces);
  LoRa.write(msg.year);//rtc.getYear());
  LoRa.write(msg.month);//rtc.getMonth());
  LoRa.write(msg.day);//rtc.getDay());
  LoRa.write(msg.hour);//rtc.getHours());
  LoRa.write(msg.minute);//rtc.getMinutes());
  LoRa.write(msg.second);//rtc.getSeconds());
  LoRa.write(msg.txPower);
  LoRa.write(msg.message.length());
  LoRa.print(msg.message);
  LoRa.endPacket(true);
  int dl = sf * 300 - 1900;
  delay(dl);//delay necessario per gestire relay
  LoRa.receive();
  lastSendTime = millis();

  //interval = (50 + msg.message.length()) * 100; // 41,2 + 1.6 ms per byte @ sf7
  //interval = (50 + msg.message.length()) * 50 * 2 ^(sf-6);
  //interval = (50 + (3/2*msg.message.length())) * 50 * 2 ^ (sf - 6);
  interval = 2000; //serve soltanto per dare tempo ai device di elaborare la prima risposta rispetto alla seconda
  //Serial.print("ID Sent: ");
  //Serial.println(msg.ID);
  //Serial.print("Recipient: ");
  //Serial.println(msg.recipient);
  //Serial.print("Data Sent: ");
  //Serial.println(msg.message);
  //Serial.print("Interval: ");
  //Serial.println(interval);
}

bool sendPOST(LoRaMessage msg, String& serverTime, bool isBattery) //client function to send and receive GET data from external server.
{
  String ethMsg = "{\"D\":\"";
  ethMsg += String(msg.sender) + "\"," +
            "\"G\":\"" + String(msg.recipient) + "\"," +
            "\"I\":\"" + String(msg.ID) + "\"," +
            "\"B\":\"" + String(msg.bounces) + "\"," +
            "\"T\":\"" + pad0(msg.year) + pad0(msg.month) + pad0(msg.day) + pad0(msg.hour) + pad0(msg.minute) + pad0(msg.second) + "\"," +
            "\"M\":\"";
  ethMsg += msg.message;
  ethMsg += "\",\"R\":" + String(LoRa.packetRssi()) + "," +
            "\"P\":" + String(msg.txPower) + "," +
            "\"N\":" + String(LoRa.packetSnr(), 4) + "}";
  //Serial.println(ethMsg);
  //Inizializzo http client
  HttpClient http(clientPost);
  if (clientPost.connect(serverIP, 80))
  {
    clientPost.println("POST /data/new HTTP/1.1");
    clientPost.println("Host: " + String(serverIP[0]) + String(".") +
                       String(serverIP[1]) + String(".") +
                       String(serverIP[2]) + String(".") +
                       String(serverIP[3]));
    clientPost.println("Connection: close");
    clientPost.println("content-type: application/json");
    clientPost.print("content-length: " );
    clientPost.println(ethMsg.length());
    clientPost.println();
    clientPost.println(ethMsg);
  }
  else {
    //Serial.println("TORNO STRANO");
    clientPost.stop();
    blinkLed(3);
    //resetto!
    if (!isBattery) {
      //Serial.println("RESET");
      ledReset();
      NVIC_SystemReset();
      return false;
    }
  }
  int status = 0;
  String readString = ""; //used by server to capture GET request
  String Time = "  ";
  while (clientPost.connected() && !clientPost.available()) delay(1); //waits for data
  while (clientPost.connected() || clientPost.available()) { //connected or data available
    char c = clientPost.read();
    //Serial.print(c);
    readString.concat(c);
    if (c == '\n')
    {
      if (status == 0)
      {
        status = readString.substring(readString.indexOf(" ") + 1, readString.indexOf(" ", readString.indexOf(" ") + 1)).toInt();
      }
      readString = "";
    }
    if (c == '|' || c == 0xFFFF)
    {
      Time = readString.substring(readString.indexOf("|") - 12, readString.indexOf("|"));
      Time.trim();
      break;
    }
  }

  serverTime = Time;
  rtc.setDate(serverTime.substring(4, 6).toInt(), serverTime.substring(2, 4).toInt(), serverTime.substring(0, 2).toInt() + 2000);
  rtc.setTime(serverTime.substring(6, 8).toInt(), serverTime.substring(8, 10).toInt(), serverTime.substring(10, 12).toInt());
  //Serial.print("TIME: ");
  //Serial.println(serverTime);
  //Serial.print("Status: ");
  //Serial.println(status);
  readString = "";
  clientPost.stop();
  if (status == 200 || status == 208)
  {
    return true;
  } else {
    return false;
  }
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;
  //Serial.println();
  //Serial.print("Incoming Message from: ");
  byte bb[2];
  LoRaMessage incomingMsg;
  bb[0] = LoRa.read();
  bb[1] = LoRa.read();
  incomingMsg.recipient = Byte2Long(bb);
  bb[0] = LoRa.read();
  bb[1] = LoRa.read();
  incomingMsg.sender = Byte2Long(bb);
  incomingMsg.ID = LoRa.read();
  incomingMsg.bounces = LoRa.read();
  incomingMsg.year = LoRa.read();
  incomingMsg.month = LoRa.read();
  incomingMsg.day = LoRa.read();
  incomingMsg.hour = LoRa.read();
  incomingMsg.minute = LoRa.read();
  incomingMsg.second = LoRa.read();
  incomingMsg.txPower = LoRa.read();
  incomingLength = LoRa.read();

  int i = 0;
  while (LoRa.available()) {
    incomingMsg.message += (char)LoRa.read();
    i += 1;
  }
  incomingMsg.message = strRev(incomingMsg.message);

  if (incomingLength != incomingMsg.message.length()) {
    //Serial.println("error message length");
    blinkLed(3);
    return;
  }

  // N.B. IL GATEWAY NON FA RITRASMISSIONI!!!
  if (incomingMsg.recipient != localAddress && incomingMsg.recipient != 0xFF) {
    //Serial.println("This message not for me.");
    /* if (incomingMsg.bounces<maxBounces) {
       incomingMsg.bounces +=1;
       sendLora(incomingMsg);
      }*/
    return;
  }
  /*//Serial.println("Received from: 0x" + String(incomingMsg.sender, HEX));
    //Serial.println("Sent to: 0x" + String(incomingMsg.recipient, HEX));
    //Serial.println("Message ID: " + String(incomingMsg.ID));
    //Serial.println("Message length: " + incomingMsg.message.length());
    //Serial.println("Message Time: " + String(incomingMsg.day) + "/" + String(incomingMsg.month) + "/" +  String(incomingMsg.year) + " " + String(incomingMsg.hour) + ":" + String(incomingMsg.minute) + ":" + String(incomingMsg.second));
    //Serial.println("Message: " + incomingMsg.message);
    //Serial.println("RSSI: " + String(LoRa.packetRssi()));
    //Serial.println("Snr: " + String(LoRa.packetSnr(), 4));
    //Serial.println();*/
  //Serial.println(incomingMsg.sender);

  //Invio conferma ricezione
  LoRaMessage outgoingMsg;
  outgoingMsg.recipient = incomingMsg.sender;
  outgoingMsg.sender = localAddress;
  outgoingMsg.bounces = 0;
  if ((int)incomingMsg.ID > 0 ) //TOLTO PERCHE' NON RICEVE PIU DATA E ORA, E NON LOGGA
  {
    outgoingMsg.message = "";
    if (sendPOST(incomingMsg, outgoingMsg.message, false))
    {
      outgoingMsg.ID = 00;
    } else {
      outgoingMsg.ID = 255;
    }
  }
  else
  {
    //ping
    //Serial.println("INVIO: " + DisplayAddress(Ethernet.localIP()));
    outgoingMsg.message = DisplayAddress(Ethernet.localIP());
    outgoingMsg.ID = 00;
  }
  outgoingMsg.year = outgoingMsg.message.substring(0, 2).toInt();
  outgoingMsg.month = outgoingMsg.message.substring(2, 4).toInt();
  outgoingMsg.day = outgoingMsg.message.substring(4, 6).toInt();
  outgoingMsg.hour = outgoingMsg.message.substring(6, 8).toInt();
  outgoingMsg.minute = outgoingMsg.message.substring(8, 10).toInt();
  outgoingMsg.second = outgoingMsg.message.substring(10, 12).toInt();

  sendLora(outgoingMsg);
  //Serial.print("**** Fine onreceive ***");
  blinkLed(1);
  //}
}
String DisplayAddress(IPAddress address)
{
  return String(address[0]) + "." +
         String(address[1]) + "." +
         String(address[2]) + "." +
         String(address[3]);
}
long Byte2Long(byte bb[]) {
  unsigned int word = word(bb[0], bb[1]);
  return word;
}

void EEPROM_writeint(int address, int value) {
  EEPROM.write(address, highByte(value));
  EEPROM.write(address + 1 , lowByte(value));
}

unsigned int EEPROM_readint(int address) {
  unsigned int word = word(EEPROM.read(address), EEPROM.read(address + 1));
  return word;
}

String pad0(int val)
{
  String ret = "";
  if (val < 10)
  {
    ret = "0";
  }
  ret = ret + String(val);
  return ret;
}

void blinkLed(int l)
{
  for (int i = 0; i < l; i++)
  { digitalWrite(DIGILED, HIGH);   // turn the LED on
    delay(50);
    digitalWrite(DIGILED, LOW);    // turn the LED off
    delay(100);
  }
}
bool acceso = true;
void fastLedError()
{
  while (1) {
    if (acceso)
    {
      digitalWrite(DIGILED, HIGH);
    }
    else
    {
      digitalWrite(DIGILED, LOW);
    }
    acceso = !acceso;
    delay(100);
  }
}
float getBatteryLevel()
{
  //return analogRead(LiPo_BATTERY) * 3.3f / 1005.0f / 1.0f * (1.0f + 0.3f);
  return cBatt + analogRead(LiPo_BATTERY) * 3.3f / 1030.0f / 1.0f * (1.0f + 0.3f);
}
String strRev(String args) {
  String s1 = "";
  for (int i = args.length() - 1; i >= 0; i--)
  {
    s1 += args.charAt(i);
  }
  return s1;
}
String pad0Hex(byte val)
{
  String ret = "";
  if (val <= 0x0F)
  {
    ret = "0" ;
  }
  ret = ret + String(val, HEX);
  return ret;
}
String padZeroHex(int val)
{
  String message = "";
  String ret = String(val, HEX);
  for (int x = 0; x < 8 - ret.length(); x++)
  {
    message += "0";
  }
  message += ret;
  return message;
}
void ledReset()
{
  pinMode(DIGILED, OUTPUT);
  // put your setup code here, to run once:
  for (int i = 0; i < 4; i++)
  { digitalWrite(DIGILED, HIGH);   // turn the LED on
    delay(50);
    digitalWrite(DIGILED, LOW);    // turn the LED off
    delay(75);
  }
  delay(1000);
}
void serialEEPROMupdate()
{
  Serial.println("GATE");
  Serial.print("Local Addr:");
  Serial.println(localAddress);
  Serial.print("Frequency:");
  Serial.println(loraFreq);
  Serial.print("SyncWord:");
  Serial.println(syncWord);
  Serial.print("SF:");
  Serial.println(sf);
  Serial.print("IP:");
  Serial.println(Ethernet.localIP());
  Serial.print("Server:");
  for (int i = 0; i < 4; i++)
  {
    Serial.print(serverIP[i]);
    if (i < 3) Serial.print(".");
  }
  Serial.println();
  Serial.print("MAC:");
   for (int i = 0; i < 6; i++) {
    Serial.print(String(mac[i], HEX));
  }
  Serial.println();
  if (Serial.available() > 0) {
    //Serial.println("Serial Available");

    String inStr = Serial.readString();  //read until timeout
    inStr.trim();                        // remove any \r \n whitespace at the end of the String

    //Serial.print("inStr= "); //Serial.println(inStr.length());
    if (inStr.length() == 52) {
      char firstByte[2];
      for (int i = 0; i <= 26; i++)
      {
        //Serial.print(i);
        //Serial.print(" ");
        //Serial.println(inStr.substring(i * 2, i * 2 + 2));
        inStr.substring(i * 2, i * 2 + 2).toCharArray(firstByte, 3);
        val = strtol(firstByte, NULL, 16);
        EEPROM.write(i, val);
      }
      EEPROM.commit();
      //Serial.println("EEPROM UPDATED VIA SERIAL");
      ledReset();
      NVIC_SystemReset();
    }
  }
  //Serial.println("FINE serial Update");
}
