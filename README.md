# FoxTemp 2022

## Intro

This repository contains info about, firmware and a case for the
FoxTemp-Device version 2022.

This is very close to its predecessor, the "FoxTemp 2016",
the main changes are:
- uses a SHT41 (or SHT45) as a sensor instead of a SHT31/35. The
  SHT41 boasts with improved accuracy over its predecessor:
  a typical accuracy of &plusmn;1.8% RH (SHT31: &plusmn;2%)
  and &plusmn;0.2&deg;C (SHT31: &plusmn;0.3&deg;C) in the
  relevant ranges.
- uses a "Canique MK2" as the microprocessor base board, which
  in itself is a "Moteino V4" clone, so you could use that one as
  well. Unfortunately, there is no variant with a boost-converter
  onboard, like there was with the "JeeNode Micro". The boost
  converter needs to be an external "Canique Boost".

One thing did not change: The over-the-air protocol. It is not
possible to distinguish a foxtemp2022 from a foxtemp2016 at the
receiving end, both transmit exactly the same data format.

Note that you will also need a way to receive the wirelessly transmitted
data. I simply used a JeeLink v3c,
which is a simple USB transceiver for 868 MHz that is commonly used in
non-commercial home-automation systems. As such it is well supported by
FHEM, a popular non-commercial and cloud-free home-automation software.
A FHEM module for using it to feedg temperature data from foxtemp2016
or foxtemp2022 devices into FHEM can be found in the foxtemp2016 repository.

## Hardware

### Intro

This has been designed for low power usage, and when not transmitting the circuitry
uses only a fraction of a miliampere (around 0.05 mA at 2.8 volts input voltage,
this depends on the input voltage obviously). This should guarantee a long
battery life, but it's still unknown how long exactly this will be.
It also could be better if we weren't needlessly using a 16 MHz crystal
oscillator.
It will probably be slightly shorter than the one foxtemp2016 achieved,
but in the same order of magnitude.

### BOM

The hardware uses as many "ready made" parts as possible and just slaps them
together. The bill of materials therefore is short:

* [Canique MK2](https://www.canique.de/mk2), which is a "Moteino"
  clone and an ATtiny328P based microcontroller module with an
  RFM69HW 868 MHz wireless module. There are multiple versions of
  that, the one used here is the one without onboard voltage
  converter or flash.
* [Canique Boost](https://www.canique.de/boost), a very efficient
  step-up/boost converter. I can be powered by anything from 0.8
  to 3.3 volts and will generate the 3.3 volts the microcontroller
  and peripherals use from that.
* SHT41 temperature sensor, in the form of a
  [random SHT41 breakout board](https://www.soselectronic.com/products/various/sht-41-bob-373380).
  This is used because the SHT41 is very tiny, so soldering it directly
  without damaging it would not be an easy task, and because the breakout
  board serves perfectly as what would be called a "shield" in the Arduino
  world: It's plugged/stacked on top of the microcontroller board.
* Goobay 78467 battery tray. This is simply a battery holder for two
  AA / mignon batteries that has cables attached.
* 6pin angled or non-angled pin strip for the FTDI connector of the Canique MK2.
* 4pin (non-angled) pin strips male and female for connecting and mounting the
  SHT41 on top of the Canique MK2.
* 2pin (non-angled) pin strips male and female for connecting and mounting the
  Canique Boost on top of the Canique MK2

### Putting it together

Wanting to keep things as simple and compact as possible, I soldered the few
parts directly together as a stack. In the following description,
"top" means the side of the Canique MK2 / Moteino with the microcontroller,
the radio module is at the "bottom".

(to be continued)

## Case

A reference design for a laser-cut case that will fit the sensor construction
and the batteries (just like for foxtemp2016) is in the works. It is not
finished yet.

