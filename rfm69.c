/* $Id: rfm69.h $
 * Functions for communication with RF69 module.
 *
 * This code was heavily copy+pasted from RFMxx.cpp in the FHEM LaCrosseIT+
 * Jeenode Firmware Code.
 */

#include <avr/io.h>
#include <math.h>
#include "rfm69.h"

/* Pin mappings for Moteino / Canique MK2:
 *  SS     PB2
 *  MOSI   PB3
 *  MISO   PB4
 *  SCK    PB5
 *  RESET  ???
 *  OUROWNSS PB2 (not used!)
 *  INT    PD2=int0
 */
#define RFMDDR   DDRB
#define RFMPORT  PORTB

#define RFMPIN_SS    PB2
#define RFMPIN_MOSI  PB3
#define RFMPIN_MISO  PB4
#define RFMPIN_SCK   PB5

#define RFM_FREQUENCY 868300ul
#ifndef RFM_DATARATE
#define RFM_DATARATE 17241.0
#endif /* RFM_DATARATE not defined externally */

#define PAYLOADSIZE 64

/* Note: Internal use only. Does not set the SS pin, the calling function
 * has to do that! */
static uint8_t rfm69_spi8(uint8_t value) {
  SPDR = value;
  /* busy-wait for transmission. this loop spins 4 usec with a 2 MHz SPI clock */
  while (!(SPSR & _BV(SPIF))) { }

  return SPDR;
}

uint16_t rfm69_spi16(uint16_t value) {
  RFMPORT &= (uint8_t)~_BV(RFMPIN_SS);
  uint16_t reply = rfm69_spi8(value >> 8) << 8;
  reply |= rfm69_spi8(value);
  RFMPORT |= _BV(RFMPIN_SS);
  
  return reply;
}

/* Add two helpers for accessing the RFM69s registers. */
uint8_t rfm69_readreg(uint8_t reg) {
  return rfm69_spi16(((uint16_t)(reg & 0x7f) << 8) | 0x00) & 0xff;
}

static void rfm69_writereg(uint8_t reg, uint8_t val) {
  rfm69_spi16(((uint16_t)(reg | 0x80) << 8) | val);
}

void rfm69_clearfifo(void) {
  /* There is no need for reading / ORing the register here because all
   * bits except the FiFoOverrun-bit we set to clear the FIFO are read-only */
  rfm69_writereg(0x28, (1 << 4)); /* RegIrqFlags2 */
}

void rfm69_settransmitter(uint8_t e) {
  if (e) {
    /* RegOpMode => TRANSMIT */
    rfm69_writereg(0x01, (rfm69_readreg(0x01) & 0xE3) | 0x0C);
  } else {
    /* RegOpMode => STANDBY */
    rfm69_writereg(0x01, (rfm69_readreg(0x01) & 0xE3) | 0x04);
  }
}

void rfm69_setsleep(uint8_t s) {
  if (s) {
    /* RegOpMode => SLEEP */
    rfm69_writereg(0x01, (rfm69_readreg(0x01) & 0xE3) | 0x00);
    /* Disable SPI and USART0 */
    PRR |= _BV(PRSPI) | _BV(PRUSART0);
  } else {
    /* RegOpMode => STANDBY */
    PRR &= (uint8_t)~(_BV(PRSPI) | _BV(PRUSART0));
    /* Manual says we should reinitialize USART0 and SPI after waking it */
    /* set master mode with rate clk/16 = 1 MHz */
    SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);
    SPSR = 0x00; /* To set SPI2X to 0, rest is read-only anyways */
    rfm69_writereg(0x01, (rfm69_readreg(0x01) & 0xE3) | 0x04);
    while (!(rfm69_readreg(0x27) & 0x80)) { /* Wait until ready */ }
  }
}

static uint16_t rfm69_readstatus(void) {
  return rfm69_spi16(0x0000);
}

/* Wait for ready to send */
static void rfm69_waitforrts(void) {
  while (!(rfm69_readstatus() & 0x8000)) { }
}

void rfm69_sendbyte(uint8_t data) {
  rfm69_waitforrts();
  rfm69_spi16(0xB800 | data);
}

void rfm69_sendarray(uint8_t * data, uint8_t length) {
  /* Set the length of our payload */
  rfm69_writereg(0x38, length);
  rfm69_clearfifo(); /* Clear the FIFO */
  /* Now fill the FIFO. We manually set SS and use spi8 because this
   * is the only "register" that is larger than 8 bits. */
  RFMPORT &= (uint8_t)~_BV(RFMPIN_SS);
  rfm69_spi8(0x80); /* Select RegFifo (0x00) for writing (|0x80) */
  for (int i = 0; i < length; i++) {
    rfm69_spi8(data[i]);
  }
  RFMPORT |= _BV(RFMPIN_SS);
  /* FIFO has been filled. Tell the RFM69 to send by just turning on the transmitter. */
  rfm69_settransmitter(1);
  /* Wait for transmission to finish, visible in RegIrqFlags2. */
  uint8_t reg28 = 0x00;
  uint16_t maxreps = 10000;
  while (!(reg28 & 0x08)) {
    reg28 = rfm69_readreg(0x28);
    maxreps--;
    if (maxreps == 0) { /* Give up */
      break;
    }
  }
  rfm69_settransmitter(0);
}

void rfm69_initport(void) {
  /* Configure Pins for output / input */
  RFMDDR |= _BV(RFMPIN_MOSI);
  RFMDDR &= (uint8_t)~_BV(RFMPIN_MISO);
  RFMDDR |= _BV(RFMPIN_SCK);
  RFMPORT |= _BV(RFMPIN_SS);
  RFMDDR |= _BV(RFMPIN_SS);
  /* Enable hardware SPI, no need to manually do it.
   * set master mode with rate clk/16 = 1 MHz (maximum of RFM69 is unknown) */
  SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);
  SPSR = 0x00; /* To set SPI2X to 0, rest is read-only anyways */
}

void rfm69_initchip(void) {
  /* RegOpMode -> standby. */
  rfm69_writereg(0x01, 0x00 | 0x04);
  /* RegDataModul -> PacketMode, FSK, Shaping 0 */
  rfm69_writereg(0x02, 0x00);
  /* RegFDevMsb / RegFDevLsb -> 0x05C3 (90 kHz). */
  rfm69_writereg(0x05, 0x05);
  rfm69_writereg(0x06, 0xC3);
  /* RegPaLevel -> Pa0=0 Pa1=1 Pa2=1 Outputpower=28 -> 14 dbM */
  /* The Canique has a RFM69HW which could actually do 20 dbM, but
   * unfortunately, at 868.3 MHz only 25 mW/14 dbM are permitted in
   * Europe. */
  rfm69_writereg(0x11, 0x40 | 0x20 | 28);
  /* RegOcp -> Over-Current-Protection: permit 120 mA */
  rfm69_writereg(0x13, 0x1f);
  /* RegRxBw -> DccFreq 010   Mant 16   Exp 2 - this is a receiver-register,
   * we do not really care about it */
  rfm69_writereg(0x19, 0x42);
  /* RegDioMapping2 -> disable clkout (but thats the default anyways) */
  rfm69_writereg(0x26, 0x07);
  /* RegIrqFlags2 (0x28): some status flags, writing a 1 to FIFOOVERRUN bit
   * clears the FIFO. This is what clearfifo() does. */
  rfm69_clearfifo();
  /* RegRssiThresh -> 220 */
  rfm69_writereg(0x29, 220);
  /* RegPreambleMsb / Lsb - we want 3 bytes of preamble (0xAA) */
  rfm69_writereg(0x2C, 0x00);
  rfm69_writereg(0x2D, 0x03);
  /* RegSyncConfig -> SyncOn FiFoFillAuto SyncSize=2 SyncTol=0 */
  rfm69_writereg(0x2E, 0x88);
  /* RegSyncValue1/2 (3-8 exist too but we only use 2 so do not need to set them) */
  rfm69_writereg(0x2F, 0x2D);
  rfm69_writereg(0x30, 0xD4);
  /* RegPacketConfig1 -> FixedPacketLength CrcOn=0 */
  rfm69_writereg(0x37, 0x00);
  /* RegPayloadLength -> 0
   * This selects between two different modes: "0" means "Unlimited length
   * packet format", any other value "Fixed Length Packet Format" (with that
   * length). We set 0 for now, but actually fill the register before sending.
   */
  rfm69_writereg(0x38, 0x0c);
  /* RegFifoThreshold -> TxStartCond=1 value=0x0f */
  rfm69_writereg(0x3C, 0x8F);
  /* RegPacketConfig2 -> AesOn=0 and AutoRxRestart=1 even if we do not care about RX */
  rfm69_writereg(0x3D, 0x12);
  /* Set Frequency */
  /* The datasheet is horrible to read at that point, never stating a clear
   * formula ready for use. */
  /* F(Step) = F(XOSC) / (2 ** 19)      2 ** 19 = 524288
   * F(forreg) = FREQUENCY_IN_HZ / F(Step) */
  uint32_t freq = round((1000.0 * RFM_FREQUENCY) / (32000000.0 / 524288.0));
  /* (Alternative calculation from JeeNode library:)
   * Frequency steps are in units of (32,000,000 >> 19) = 61.03515625 Hz
   * use multiples of 64 to avoid multi-precision arithmetic, i.e. 3906.25 Hz
   * due to this, the lower 6 bits of the calculated factor will always be 0
   * this is still 4 ppm, i.e. well below the radio's 32 MHz crystal accuracy */
  /* uint32_t freq = (((RFM_FREQUENCY * 1000) << 2) / (32000000UL >> 11)) << 6; */
  rfm69_writereg(0x07, (freq >> 16) & 0xff);
  rfm69_writereg(0x08, (freq >>  8) & 0xff);
  rfm69_writereg(0x09, (freq >>  0) & 0xff);
  /* Set Datarate - the whole floating point mess seems to be optimized out 
   * by gcc at compile time, thank god. */
  uint16_t dr = (uint16_t)round(32000000.0 / RFM_DATARATE);
  rfm69_writereg(0x03, (dr >> 8));
  rfm69_writereg(0x04, (dr & 0xff));

  rfm69_clearfifo();
}
