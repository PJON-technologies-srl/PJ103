#include <avr/sleep.h>

// PJ103 relay module

// SoftwareBitBang timing configuration
#define SWBB_READ_DELAY         6
// PJ103 software version
#define MODULE_VERSION          1
// PJ103 by default accepts configuratio change
#define MODULE_ACCEPT_CONFIG true

// PJON configuration
// Do not use internal packet buffer (reduces memory footprint)
#define PJON_MAX_PACKETS        0

#include <PJONSoftwareBitBang.h>
#include <EEPROM.h>

// Instantiate PJON
PJONSoftwareBitBang bus;

uint8_t recipient_id;
bool accept_config_change;
uint16_t interval;
uint32_t time;
bool state;

void setup() {
  adc_disable(); // The relay does not require ADC ~320uA spared
  // Writing default configuration in EEPROM
  if(
    EEPROM.read(4) != 'P' ||
    EEPROM.read(5) != 'J' ||
    EEPROM.read(6) != '1' ||
    EEPROM.read(7) != '0' ||
    EEPROM.read(8) != '3' ||
    EEPROM.read(9) != MODULE_VERSION
  ) EEPROM_write_default_configuration();
  EEPROM_read_configuration();
  // Use pin 1 for PJON communicaton
  bus.strategy.set_pin(1);
  // Begin PJON communication
  bus.begin();
  // Register the receiver callback called when a packet is received
  bus.set_receiver(receiver_function);
  // Setup pin which controls the LED
  pinMode(0, OUTPUT);
  // 1 second LED blink to showcase nominal startup
  digitalWrite(0, HIGH);
  delay(1000);
  digitalWrite(0, LOW);
  time = millis();
}

void EEPROM_read_configuration() {
  bus.set_id(EEPROM.read(0));
  recipient_id = EEPROM.read(1);
  interval = EEPROM.read(2) << 8 | EEPROM.read(3) & 0xFF;
  accept_config_change = EEPROM.read(10);
};

void EEPROM_write_default_configuration() {
  // PJ103 ID
  EEPROM.update(0, PJON_NOT_ASSIGNED);
  // Recipient ID
  EEPROM.update(1, PJON_MASTER_ID);
  // Default interval
  EEPROM.update(2, 0);
  EEPROM.update(3, 0);
  // Module name
  EEPROM.update(4, 'P');
  EEPROM.update(5, 'J');
  EEPROM.update(6, '1');
  EEPROM.update(7, '0');
  EEPROM.update(8, '3');
  EEPROM.update(9, MODULE_VERSION);
  // Accept incoming configuration
  EEPROM.update(10, MODULE_ACCEPT_CONFIG);
};

void loop() {
  bus.receive(100);
  if(interval && ((millis() - time) > interval)) {
    digitalWrite(0, state = !state);
    time = millis();
  }
}

void receiver_function(uint8_t *payload, uint16_t length, const PJON_Packet_Info &info) {
  bool is_master =
    (info.tx.id == PJON_MASTER_ID) || (info.tx.id == recipient_id);

  // RELAY ON
  if(is_master && (payload[0] == 'H')) digitalWrite(0, HIGH);
  // RELAY OFF
  if(is_master && (payload[0] == 'L')) digitalWrite(0, LOW);
  // INFO REQUEST
  if(payload[0] == '?') {
    uint8_t module_name[6] = {
      EEPROM.read(5),
      EEPROM.read(6),
      EEPROM.read(7),
      EEPROM.read(8),
      EEPROM.read(9),
      EEPROM.read(10)
    };
    bus.send_packet(recipient_id, module_name, 6);
  }

  if(!accept_config_change) return;

  // RELAY INTERVAL STATE CHANGE
  if(is_master && (payload[0] == 'T')) {
    interval = payload[1] << 8 | payload[2] & 0xFF;
    EEPROM.update(2, payload[1]);
    EEPROM.update(3, payload[2]);
  }
  // DEVICE ID UPDATE
  if(is_master && (payload[0] == 'I')) {
    bus.set_id(payload[1]);
    EEPROM.update(0, payload[1]);
  }
  // RECIPIENT ID UPDATE
  if(is_master && (payload[0] == 'R')) {
    recipient_id = payload[1];
    EEPROM.update(1, recipient_id);
  }
  // DANGER ZONE
  // Attention when X is received configuration is set to default
  if(is_master && (payload[0] == 'X')) {
    EEPROM_write_default_configuration();
    EEPROM_read_configuration();
  }
  // Attention when Q is received the module will stop to accept commands
  if(is_master && (payload[0] == 'Q')) {
    accept_config_change = false;
    EEPROM.update(10, 0);
  }
};
