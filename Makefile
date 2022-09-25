# $Id: Makefile $
# Makefile for Foxtemp2022

CC	= avr-gcc
OBJDUMP	= avr-objdump
OBJCOPY	= avr-objcopy
AVRDUDE	= avrdude
INCDIR	= .
# There are a few additional defines that en- or disable certain features,
# mainly to save space in case you are running out of flash.
# You can add them here.
#  -DRFM_DATARATE=9579.0 or 17241.0. This defaults to 17241
ADDDEFS	= 

# The port on which the programmer is connected?
PRPORT = /dev/ttyUSB1

# target mcu (atmega 328p)
MCU	= atmega328p
# Since avrdude is generally crappy software (I liked uisp a lot better, too
# bad the project is dead :-/), it cannot use the MCU name everybody else
# uses, it has to invent its own name for it. So this defines the same
# MCU as above, but with the name avrdude understands.
AVRDMCU	= m328p

# Some more settings
# Clock Frequency of the AVR. Needed for various calculations.
CPUFREQ		= 1000000UL

SRCS	= adc.c eeprom.c main.c rfm69.c sht4x.c
PROG	= foxtemp2022

# compiler flags
CFLAGS	= -g -Os -Wall -Wno-pointer-sign -std=c99 -mmcu=$(MCU) $(ADDDEFS)

# linker flags
LDFLAGS = -g -mmcu=$(MCU) -Wl,-Map,$(PROG).map -Wl,--gc-sections

CFLAGS += -DCPUFREQ=$(CPUFREQ) -DF_CPU=$(CPUFREQ)

OBJS	= $(SRCS:.c=.o)

all: compile dump text eeprom
	@echo -n "Compiled size: " && ls -l $(PROG).bin

compile: $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROG).elf $(OBJS)

dump: compile
	$(OBJDUMP) -h -S $(PROG).elf > $(PROG).lst

%o : %c 
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

# Create the flash contents
text: compile
	$(OBJCOPY) -j .text -j .data -O ihex $(PROG).elf $(PROG).hex
	$(OBJCOPY) -j .text -j .data -O srec $(PROG).elf $(PROG).srec
	$(OBJCOPY) -j .text -j .data -O binary $(PROG).elf $(PROG).bin

# Rules for building the .eeprom rom images
eeprom: compile
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $(PROG).elf $(PROG)_eeprom.hex
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O srec $(PROG).elf $(PROG)_eeprom.srec
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O binary $(PROG).elf $(PROG)_eeprom.bin

clean:
	rm -f $(PROG) hostreceiverforjeelink *~ *.elf *.rom *.bin *.eep *.o *.lst *.map *.srec *.hex

hostreceiverforjeelink: hostreceiverforjeelink.c
	gcc -o hostreceiverforjeelink -Wall -Wno-pointer-sign -O2 -DBRAINDEADOS hostreceiverforjeelink.c

fuses:
	@echo "If you want to be safe, the fuses should be set for a BODlevel"
	@echo "of 2.7 volts. Something along the lines of:"
	@echo "  avrdude ... -U hfuse:w:0xd5:m"
	@echo "However, that uses up 0,03 mA of power, in other words, it almost"
	@echo "doubles the power consumption. To disable BODlevel, set hfuse"
	@echo "to 0xd7 instead, but be warned that the longterm stability of that"
	@echo "is untested/unknown."

upload: uploadflash uploadeeprom

uploadflash:
	$(AVRDUDE) -c arduino -p $(AVRDMCU) -P $(PRPORT) -U flash:w:$(PROG).hex

uploadeeprom:
	@echo "Note: Directly uploading EEPROM is not possible due to the optiboot bootloader"
	@echo "not supporting it in the version on the Canique MK2."
	@echo "This can be circumvented by flashing a small program that will change EEPROM contents"
	@echo "but this will overwrite your current flash and you need to reprogram it afterwards."
	@echo "To do that, use the flasheepromtool target."

flasheeprom:
	xxd -i $(PROG)_eeprom.bin |sed -e 's/$(PROG)/my/' > eepromflasher.h
	$(CC) $(LDFLAGS) -o eepromflasher.elf eepromflasher.c
	$(OBJCOPY) -j .text -j .data -O ihex eepromflasher.elf eepromflasher.hex
	$(AVRDUDE) -c arduino -p $(AVRDMCU) -P $(PRPORT) -U flash:w:eepromflasher.hex
	rm -f eepromflasher.h eepromflasher.elf eepromflasher.hex

