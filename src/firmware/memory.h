/* Private scalar flags need no byte-addressable storage on the 8051.
 * Do not use RAM_BOOL for wire/EEPROM layouts or values whose address is taken.
 */
#ifndef _MEMORY_H_
#define _MEMORY_H_
#include <stdbool.h>

#if defined(__SDCC_mcs51)
#define RAM_BOOL __bit
#else
#define RAM_BOOL bool
#endif

#endif
