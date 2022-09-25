/* $Id: sht4x.h $
 * Functions for reading the SHT 31 temperature / humidity sensor
 */

#ifndef _SHT4X_H_
#define _SHT4X_H_

struct sht4xdata {
  uint16_t temp;
  uint16_t hum;
  uint8_t valid;
};

/* Initialize (software-)I2C and sht4x */
void sht4x_init(void);

/* Start measurement */
void sht4x_startmeas(void);

/* Read result of measurement. Needs to be called no earlier than 15 ms
 * after starting. */
void sht4x_read(struct sht4xdata * d);

#endif /* _SHT4X_H_ */
