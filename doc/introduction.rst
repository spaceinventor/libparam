Space Inventor Parameter System
----------------------------------

Summary
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
libparam is designed to enable easy access to configuration as well as telemetry on modules and software that is communicating using CSP.

The library allows access to RAM variables as well as persistent configuration and even direct access to hardware peripherals on a module. Parameters can be read and modified using the Space Inventor Command Shell, CSH, which is available as open source along with the Parameter System C implementation.

Organization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Virtual memory
********************
The library implementation is based on virtual memory that either maps directly to RAM, or to a driver interfacing with an external storage, peripheral device or a file. It is possible to access the VMEM either by a parameter definition, or directly using a memory address and a data size. A virtual memory is directly read- and writable over CSP when using CSH.

Parameter list
********************
Upon request, a module can provide a list of parameters available. As CSH is agnostic and do not know about capabilities of any module, that list is required before operating a module, either by requesting the list from the module itself, or by preloading the parameter list into CSH prior to communicating with the module.

Parameter service
********************
A client, being CSH or another module, can pull and push either a single parameter, a queue of parameters or a complete category of parameters. In CSH, reading and modifying single commands are done by using the commands 'get' and 'set'.

Each parameter is defined by a type, an array size and a list of categories (mask). Furthermore, a parameter can contain an inline documentation string and a unit definition. On the server side, each parameter is registered and mapped to a VMEM or directly to a RAM address, and a push callback can optionally be registered to trigger whenever a parameter value is modified.

Server side implementation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
On the server side, each parameter is created by use of a macro.

A custom variable array can be accessed as a parameter by applying the following macro:

.. code-block:: c

    uint8_t _state[2];
    void state_cb(param_t * param, int index);
    PARAM_DEFINE_STATIC_RAM(PARAMID_STATE, state, PARAM_TYPE_UINT8, \
        sizeof(_state)/sizeof(_state[0]), sizeof(_state[0]), PM_TELEM, state_cb, NULL, \
        &_state[0], "0 = idle, 1 = waiting, 2 = running");

This example defines a parameter representing an array variable for general access from within the same application. The parameter has a callback to be triggered whenever the same, or another executable does a write access to the parameter. Each parameter in an executable is defined by a distinct index, in this example represented by PARAMID_STATE.

For a parameter that has a value stored in an FRAM configuration memory, the following VMEM area and parameter can be defined like follows:

.. code-block:: c

    VMEM_DEFINE_FRAM(fram_cfg, "fram_cfg", PHYS_ADDR, PHYS_SIZE, VIRT_ADDR);
    PARAM_DEFINE_STATIC_VMEM(PARAMID_counter, counter, PARAM_TYPE_UINT16, 0, 0, PM_SYSINFO, NULL, \
      "", fram_cfg, 0x06, "Event counter");

First, a VMEM area is defined, specifically an FRAM-base configuration storage. The particular VMEM belongs to the first 0x100 bytes of a physical address space in an FRAM memory chip. The parameter is defined to be located 6 bytes into that VMEM aread.

For the paramethttps://github.com/spaceinventor/libparam
.. code-block:: c

    int idx = 0;

    if (param_get_uint8_array(&state, idx) == 0)
        param_set_uint8_array(&state, idx, 1);

In case the parameter belongs to another executable, the parameter must be defined in the client executable to be accessible. No matter if the parameter on server-side is stored in VMEM or RAM, the client needs a RAM variable to cache the parameter when reading and modifying. A complete example of the same routine as above looks like

.. code-block:: c

    int INDEX_ALL = -1; /* Pull/push all indices */
    int VERBOSE = 0; /* Do not print additional debug output */
    int TIMEOUT = 1000; /* Timeout for remote access [ms] */
    int VERSION = 2; /* Current param interface version */

    uint8_t _state[2];
    uint16_t node;
    PARAM_DEFINE_REMOTE(PARAMID_STATE, state, &node, PARAM_TYPE_UINT8, \
    sizeof(_state)/sizeof(_state[0]), sizeof(_state[0]), PM_TELEM, &_state[0], NULL);



and then, to access the remote parameter

.. code-block:: c

    if (param_pull_single(&state, INDEX_ALL, CSP_PRIO_NORM, VERBOSE, *state.node, TIMEOUT, 2) < 0)
        printf("Retrieving parameter value failed\n");
    
    if (param_get_uint8_array(&state, idx) == 0)
        param_set_uint8_array(&state, idx, 1);

    if (param_push_single(&state, idx, CSP_PRIO_NORM, NULL, VERBOSE, *state.node, TIMEOUT, VERSION) < 0)
        printf("Storing parameter value failed\n");

When modifying multiple remote parameters, a queue can be built to efficiently retrieve or store multiple parameters in a single CSP packet.

.. code-block:: c

    param_queue_t queue;
    uint8_t queue_buf[PARAM_SERVER_MTU-2];
    param_queue_init(&queue, queue_buf, PARAM_SERVER_MTU-2, 0, PARAM_QUEUE_TYPE_GET, VERSION);

    param_queue_add(&queue, &state, idx, NULL);
    param_queue_add(&queue, &counter, INDEX_ALL, NULL);

    /* Trigger CSP to request value from parameter server */
    packet->length = queue.used + 2;
    if (param_pull_queue(&queue, CSP_PRIO_NORM, VERBOSE, &state.node, TIMEOUT) < 0)
        printf("Retrieving multiple parameter values failed\n");

    /* Modify parameters */
    if (param_get_uint8_array(&state, idx) == 0)
        param_set_uint8_array(&state, idx, 1);

    param_set_uint16(&counter, param_get_uint16(&counter) + 1);

    /* Allocate new CSP packet and rebuild queue */
    param_queue_init(&queue, queue_buf, PARAM_SERVER_MTU-2, 0, PARAM_QUEUE_TYPE_SET, VERSION);

    param_queue_add(&queue, &state, idx, NULL);
    param_queue_add(&queue, &counter, INDEX_ALL, NULL);

    /* Trigger CSP to push queue values */
    if (param_push_queue(&queue, CSP_PRIO_NORM, VERBOSE, &state.node, TIMEOUT, 0) < 0)
        printf("Storing multiple parameter values failed\n");



Parameter properties
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Datatypes
********************

The Parameter System can use a variaty of datatypes to represent data. 

.. list-table:: 
    :widths: 15 30
    :header-rows: 1

    * - Type name
      - Description

    * - PARAM_TYPE_INTx
      - Signed integer values with size of 8, 16, 32 or 64 bits

    * - PARAM_TYPE_UINTx
      - Unsigned integer values with size of 8, 16, 32 or 64 bits

    * - PARAM_TYPE_XINTx
      - Unsigned values visually represented as hexadecimal with size of 8, 16, 32 or 64 bits

    * - PARAM_TYPE_FLOAT
      - Four byte IEEE floating point value

    * - PARAM_TYPE_DOUBLE
      - Eight byte IEEE floating point value

    * - PARAM_TYPE_DATA
      - String

    * - PARAM_TYPE_STRING
      - Binary data

When transferred between executables, the values are serialized using MessagePack for a size and performance efficient coding. The transfer is protected by CRC to avoid bit errors in a noisy transmission channel.

Masks
********************

A parameter can be flagged using one or more masks, each represented by a bit in a 32 bit flag property of each parameter definition. The first 16 mask bits are reserved for system-wide definitions, while the upper 16 are available for user-defined masks.

.. list-table::
   :widths: 5 10 30 7 
   :header-rows: 1

   * - Bit ID
     - Name
     - Description
     - Character

   * - 0
     - PM_READONLY
     - The parameter is read-only
     - r

   * - 1
     - PM_REMOTE
     - The parameter is remote
     - R

   * - 2
     - PM_CONF                 
     - Configuration: to be modified by a human
     - c

   * - 3
     - PM_TELEM                
     - Ready-to-use telemetry, converted to human readable
     - t

   * - 4
     - PM_HWREG                
     - Raw-bit-values in external chips
     - h

   * - 5
     - PM_ERRCNT               
     - Rarely updated error counters (hopefully)
     - e

   * - 6
     - PM_SYSINFO              
     - Boot information, time
     - i

   * - 7
     - PM_SYSCONF              
     - Network and time configuration
     - C

   * - 8
     - PM_WDT                  
     - Critical watchdog
     - w

   * - 9
     - PM_DEBUG                
     - Debug flag
     - d

   * - 10
     - PM_CALIB
     - Calibration gains and offsets
     - q

.. raw:: pdf

    PageBreak 

Publishing parameters
~~~~~~~~~~~~~~~~~~~~~

As of **libparam** version X.X.X, it is possible to easily *publish* parameters in order to make it easier for interested parties to be notified
about parameter values without resorting to network (and CPU) intensive polling operations.

The publish feature is a build-time configuration that can be turned on using the ``num_publishqueues`` Meson option, whose value shall be an integer in the range 0-4.
Setting it to 0 (the default value) is equivalent to turning the publish feature completely OFF.

Values from 1 to 4 will create the corresponding number of publishing queues, allowing a module to publish on, at most, 4 queues.

Publishing parameter example
****************************

During the initialisation process, you will need to configure the queues you want to use by calling the ``param_publish_configure`` API and initialise the publish system.
Configuring queues means:

- indicating wich CSP node will receive the published parameters
- how often the parameters will be published
- the CSP priority that the queue will be assigned (see the CSP documentation)

Initialising means calling the initialisation function with a callback (optional).

This callback will be called by the publishing system to decide whether to perform a publish operation for a given queue:

.. code-block:: c

  static bool shall_publish(uint8_t q) {
    return q == 1 || telemetry_is_valid();
  }

  void hook_init(void) {
    /* High freq data for AOCS */
    param_publish_configure(PARAM_PUBLISHQUEUE_0, param_get_uint16_array(&param_publish_destination, PARAM_PUBLISHQUEUE_0), 200, CSP_PRIO_HIGH);

    /* Low freq data for HK */
    param_publish_configure(PARAM_PUBLISHQUEUE_1, param_get_uint16_array(&param_publish_destination, PARAM_PUBLISHQUEUE_1), 5000, CSP_PRIO_LOW);

    param_publish_init(shall_publish);
  }

Then you need to indicate which parameters shall be published, using the ``PARAM_ADD_PUBLISH`` macro.

For example, to publish the ``state`` parameter defined previously on queue 0:

.. code-block:: c

  PARAM_ADD_PUBLISH(state, PARAM_PUBLISHQUEUE_0);


And finally, you need to frequently call the ``param_publish_periodic`` API which will take care of publishing the registered parameters to the correct queues.
The frequency of the calls will determine how accurately the periodicity of the configured publishing queue will be respected.

This function can be called as part of a sampling loop function for example:

.. code-block:: c

  void my_sample(void) 
  {
    /* Sampling and parameter update code goes here */
    /* and finally */
    param_publish_periodic();
  }