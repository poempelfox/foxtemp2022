
#include <avr/eeprom.h>
#include <avr/wdt.h>

#include "eepromflasher.h"

/* We need to disable the watchdog very early, because it stays active
 * after a reset with a timeout of only 15 ms. */
void dwdtonreset(void) __attribute__((naked)) __attribute__((section(".init3")));
void dwdtonreset(void) {
  MCUSR = 0;
  wdt_disable();
}

int main() {
  eeprom_update_block(&my_eeprom_bin[0], (void *)0, my_eeprom_bin_len);
  /* That's really all there is, we're done. */
  while (1) {
  }
}

