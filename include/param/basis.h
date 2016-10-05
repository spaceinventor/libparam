/*
 * basis.h
 *
 *  Created on: Oct 5, 2016
 *      Author: johan
 */

#ifndef LIBPARAM_SETS_BASIS_H_
#define LIBPARAM_SETS_BASIS_H_

#include <param.h>
#include <vmem.h>

extern vmem_t *vmem_basis;
extern vmem_t vmem_basis_instance;

typedef struct {
	param_t csp_node;
	param_t csp_can_speed;
	param_t csp_rtable;
} basis_param_t;

extern basis_param_t basis_param;

void basis_param_init(void);
void basis_param_fallback(void);

#endif /* LIBPARAM_SETS_BASIS_H_ */
