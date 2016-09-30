/*
 * vmem_ltc.c
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */


#include <stdint.h>
#include <string.h>

#include <vmem.h>

#include "vmem_ltc.h"

#include <ltc4281.h>

void vmem_ltc_read(vmem_t * vmem, uint16_t addr, void * dataout, int len) {
	ltc4281_read_data(((vmem_ltc_driver_t *) vmem->driver)->device_id, addr, dataout, len);
}

void vmem_ltc_write(vmem_t * vmem, uint16_t addr, void * datain, int len) {
	if ((addr >= 0x20) && (addr <= 0x31)) {
		if ((addr + len) > 0x32) {
			return;
		}
		ltc4281_write_eeprom(((vmem_ltc_driver_t *) vmem->driver)->device_id, addr, datain, len);
	} else if ((addr >= 0x4C) && (addr <= 0x4F)) {
		if ((addr + len) > 0x50) {
			return;
		}
		ltc4281_write_eeprom(((vmem_ltc_driver_t *) vmem->driver)->device_id, addr, datain, len);
	} else {
		ltc4281_write_data(((vmem_ltc_driver_t *) vmem->driver)->device_id, addr, datain, len);
	}
}



