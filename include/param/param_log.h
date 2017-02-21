/*
 * param_log.h
 *
 *  Created on: Feb 21, 2017
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_PARAM_PARAM_LOG_H_
#define LIB_PARAM_INCLUDE_PARAM_PARAM_LOG_H_

/**
 * Initialize the log system
 *
 * @param _log_vmem pointer to vmem
 * @param _log_page_size page size of vmem
 *
 */
void param_log_init(vmem_t * _log_vmem, int _log_page_size);

#endif /* LIB_PARAM_INCLUDE_PARAM_PARAM_LOG_H_ */
