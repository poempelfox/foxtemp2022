/* $Id: rfm69.h $
 * Functions for communication with RF12 module
 */

#ifndef _RFM69_H_
#define _RFM69_H_

void rfm69_initport(void);
void rfm69_initchip(void);
void rfm69_clearfifo(void);
void rfm69_settransmitter(uint8_t e);
void rfm69_sendbyte(uint8_t data);
void rfm69_sendarray(uint8_t * data, uint8_t length);
void rfm69_setsleep(uint8_t s);

#endif /* _RFM69_H_ */
