struct LoRaMessage {
  unsigned int recipient;                // destination address
  unsigned int sender;                   // sender address
  unsigned int ID;                       // message ID
  unsigned int bounces;                // Number of retransmissions
  unsigned int year;  //timestamp for NTP
  unsigned int month;  //timestamp for NTP
  unsigned int day;  //timestamp for NTP
  unsigned int hour;  //timestamp for NTP
  unsigned int minute;  //timestamp for NTP
  unsigned int second;  //timestamp for NTP
  unsigned int txPower;  
  String message;                // payload
};
