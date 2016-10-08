/*
 * basis.h
 *
 *  Created on: Oct 5, 2016
 *      Author: johan
 */

#ifndef LIBPARAM_SETS_BASIS_H_
#define LIBPARAM_SETS_BASIS_H_

typedef struct {
	param_t csp_node;
	param_t csp_can_speed;
	param_t csp_rtable;
} basis_param_t;

extern basis_param_t basis_param;

#endif /* LIBPARAM_SETS_BASIS_H_ */
