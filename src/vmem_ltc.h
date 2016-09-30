/*
 * vmem_ltc.h
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_VMEM_LTC_H_
#define SRC_PARAM_VMEM_LTC_H_

typedef struct {
	int device_id;
} vmem_ltc_driver_t;

#define VMEM_DEFINE_LTC(name_in, strname, device_id_in) \
	static vmem_ltc_driver_t vmem_##name_in##_driver = { \
		.device_id = device_id_in, \
	}; \
	static vmem_t vmem_##name_in##_instance __attribute__ ((section("vmem"), used)) = { \
		.name = strname, \
		.size = 0x50, \
		.read = vmem_ltc_read, \
		.write = vmem_ltc_write, \
		.big_endian = 1, \
		.driver = &vmem_##name_in##_driver, \
	}; \
	vmem_t * vmem_##name_in = &vmem_##name_in##_instance;

void vmem_ltc_read(vmem_t * vmem, uint16_t addr, void * dataout, int len);
void vmem_ltc_write(vmem_t * vmem, uint16_t addr, void * datain, int len);


#endif /* SRC_PARAM_VMEM_LTC_H_ */
