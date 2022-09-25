/* $Id: main.c $
 * main for JeeNode Weather Station, tieing all parts together.
 * (C) Michael "Fox" Meier 2016
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <stdio.h>
#include <string.h>
#include <util/delay.h>

#include "adc.h"
#include "eeprom.h"
#include "rfm69.h"
#include "sht4x.h"

/* We need to disable the watchdog very early, because it stays active
 * after a reset with a timeout of only 15 ms. */
void dwdtonreset(void) __attribute__((naked)) __attribute__((section(".init3")));
void dwdtonreset(void) {
  MCUSR = 0;
  wdt_disable();
}

/* The values last measured */
/* temperature. 0xffff on error, 14 useful bits. */
uint16_t temp = 0;
/* humidity. 0xffff on error, 14 useful bits */
uint16_t hum = 0;
/* Battery level. Range 0-255, 255 = our supply voltage = 3,3V */
uint8_t batvolt = 0;
/* How often did we send a packet? */
uint32_t pktssent = 0;

/* This is just a fallback value, in case we cannot read this from EEPROM
 * on Boot */
uint8_t sensorid = 3; // 0 - 255 / 0xff

/* The frame we're preparing to send. */
static uint8_t frametosend[10];

static uint8_t calculatecrc(uint8_t * data, uint8_t len)
{
  uint8_t i, j;
  uint8_t res = 0;
  for (j = 0; j < len; j++) {
    uint8_t val = data[j];
    for (i = 0; i < 8; i++) {
      uint8_t tmp = (uint8_t)((res ^ val) & 0x80);
      res <<= 1;
      if (0 != tmp) {
        res ^= 0x31;
      }
      val <<= 1;
    }
  }
  return res;
}

/* Fill the frame to send with out collected data and a CRC.
 * The protocol we use is that of a "CustomSensor" from the
 * FHEM LaCrosseItPlusReader sketch for the Jeelink.
 * So you'll just have to enable the support for CustomSensor in that sketch
 * and flash it onto a JeeNode and voila, you have your receiver.
 *
 * Byte  0: Startbyte (=0xCC)
 * Byte  1: Sensor-ID (0 - 255/0xff)
 * Byte  2: Number of data bytes that follow (6)
 * Byte  3: Sensortype (=0xf7 for FoxTemp)
 * Byte  4: temperature MSB (raw value from SHT31)
 * Byte  5: temperature LSB
 * Byte  6: humidity MSB (raw value from SHT31)
 * Byte  7: humidity LSB
 * Byte  8: Battery voltage
 * Byte  9: CRC
 */
void prepareframe(void)
{
  frametosend[ 0] = 0xCC;
  frametosend[ 1] = sensorid;
  frametosend[ 2] = 6; /* 6 bytes of data follow (CRC not counted) */
  frametosend[ 3] = 0xf7; /* Sensor type: FoxTemp */
  frametosend[ 4] = (temp >> 8) & 0xff;
  frametosend[ 5] = (temp >> 0) & 0xff;
  frametosend[ 6] = (hum >> 8) & 0xff;
  frametosend[ 7] = (hum >> 0) & 0xff;
  frametosend[ 8] = batvolt;
  frametosend[ 9] = calculatecrc(frametosend, 9);
}

void loadsettingsfromeeprom(void)
{
  uint8_t e1 = eeprom_read_byte(&ee_sensorid);
  uint8_t e2 = eeprom_read_byte(&ee_invsensorid);
  if ((e1 ^ 0xff) == e2) { /* OK, the 'checksum' matches. Use this as our ID */
    sensorid = e1;
  }
}

/* This is just to wake us up from sleep, it doesn't really do anything. */
ISR(WDT_vect)
{
  /* Nothing to do here. */
}

int main(void)
{
  /* Initialize stuff */
  /* Clock down to 1 MHz. */
  CLKPR = _BV(CLKPCE);
  CLKPR = _BV(CLKPS2);
  
  _delay_ms(1000); /* Wait. */

  rfm69_initport();
  adc_init();
  sht4x_init();
  loadsettingsfromeeprom();
  
  _delay_ms(1000); /* The RFM12 needs some time to start up */
  
  rfm69_initchip();
  rfm69_setsleep(1);
  
  /* Enable watchdog timer interrupt with a timeout of 8 seconds */
  WDTCSR = _BV(WDCE) | _BV(WDE);
  WDTCSR = _BV(WDE) | _BV(WDIE) | _BV(WDP0) | _BV(WDP3);

  /* Prepare sleep mode */
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  
  /* Disable unused chip parts and ports */
  /* We do not use any of the timers */
  PRR |= _BV(PRTIM0) | _BV(PRTIM1) | _BV(PRTIM2);
  /* we don't use hardware TWI */
  PRR |= _BV(PRTWI);
  /* Disable analog comparator */
  ACSR |= _BV(ACD);
  /* Disable unneeded digital input registers for all unused ADC pins
   * (they can cause significant power drain in some conditions).
   * Do not disable them for AIN0/1 aka PD6/7, they are in use! */
  DIDR0 |= _BV(ADC5D) | _BV(ADC4D) | _BV(ADC3D) | _BV(ADC2D) | _BV(ADC1D) | _BV(ADC0D);
  /* PD2 is the IRQ line from the RFM69. We don't use it. Make sure that pin
   * is tristated on our side (it won't float, the RFM69 pulls it) */
  PORTD &= (uint8_t)~_BV(PD2);
  DDRD &= (uint8_t)~_BV(PD2);

  /* there is a LED connected to PB1 */
  PORTB &= (uint8_t)~_BV(PB1);
  DDRB |= _BV(PB1);
  /* All set up, enable interrupts and go. */
  sei();

  sht4x_startmeas();

  uint16_t transmitinterval = 2; /* this is in multiples of the watchdog timer timeout (8S)! */
  uint8_t mlcnt = 0;
  uint8_t readerrcnt = 0;
  while (1) { /* Main loop, we should never exit it. */
#ifdef BLINKLED
    PORTB |= _BV(PB1);
#endif /* BLINKLED */
    mlcnt++;
    if (mlcnt > transmitinterval) {
      rfm69_setsleep(0);  /* This mainly turns on the oscillator again */
      adc_power(1);
      adc_start();
      /* Fetch values from PREVIOUS measurement */
      struct sht4xdata hd;
      sht4x_read(&hd);
      temp = 0xffff;
      hum = 0xffff;
      if (hd.valid) {
        readerrcnt = 0;
        temp = hd.temp;
        hum = hd.hum;
      } else {
        readerrcnt++;
        if (readerrcnt > 5) {
          /* We could not read the SHT31 5 times in a row?! */
          /* Then force reset through watchdog timer. */
          while (1) {
            sleep_cpu();
          }
        }
      }
      sht4x_startmeas();
      /* read voltage from ADC */
      uint16_t adcval = adc_read();
      adc_power(0);
      /* we have the battery pack directly connected without a
       * voltage divider, while foxtemp2016 did have a voltage
       * divider, returning 10/11 of the real voltage. However,
       * it was also operating at 3.0V, while we run at 3.3V.
       * The factor between that is 11/10, which means that by
       * pure accident our reported voltage values are exactly
       * compatible with foxtemp2016 without any conversion. */
      batvolt = adcval >> 2;
      prepareframe();
      rfm69_sendarray(frametosend, 10);
      pktssent++;
      /* Semirandom delay: the lowest bits from the ADC are mostly noise, so
       * we use that */
      transmitinterval = 3 + (adcval & 0x0001);
      rfm69_setsleep(1);
      mlcnt = 0;
    }
#ifdef BLINKLED
    PORTB &= (uint8_t)~_BV(PB1);
#endif /* BLINKLED */
    wdt_reset();
    sleep_cpu(); /* Go to sleep until the watchdog timer wakes us */
    /* We should only reach this if we were just woken by the watchdog timer.
     * We need to re-enable the watchdog-interrupt-flag, else the next watchdog
     * -reset will not just trigger the interrupt, but be a full reset. */
    WDTCSR = _BV(WDCE) | _BV(WDE);
    WDTCSR = _BV(WDE) | _BV(WDIE) | _BV(WDP0) | _BV(WDP3);
  }
}
