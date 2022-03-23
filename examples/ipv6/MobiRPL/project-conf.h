#ifndef __PROJECT_CONF_H__
#define __PROJECT_CONF_H__

#include "sys/node-id.h"

/* evaluation setting */
#define TESTBED_EVAL                        0
#define COOJA_EVAL                          0
#define COOJA_EVAL_2                        1

#if TESTBED_EVAL
/* topology */
#define ROOT_ID                             1
#define MAX_NODES                           31  /* Cooja: 14, Testbed: 31 nodes in total */
#define MAX_MEMORIES                        31
#define TESTBED_01                          0
#define TESTBED_10                          0
#define TESTBED_20                          0
#define SINGLE_SENDER_ID                    MAX_NODES
/* app layer */
#define UPWARD_TRAFFIC                      1
#define DOWNWARD_TRAFFIC				    1
#define APP_MAX_SEQNO                       1425 /* 120, 200, 1200, 23:45 */
#define CONF_START_DELAY                    180 /* Cooja: 30, Testbed: 180 */
#define CONF_SEND_INTERVAL                  60 /* 5, 30, 60 */
/* rdc layer */
#define ALWAYS_ON_RDC                       0
/* phy layer */
#define CC2420_CONF_RF_POWER			    11 /* Cooja: 31, testbed: 11 */
/* testbed end */

#elif COOJA_EVAL
/* topology */
#define ROOT_ID                             1
#define MAX_NODES                           14
#define MAX_MEMORIES                        35
#define TESTBED_01                          0
#define SINGLE_SENDER_ID                    MAX_NODES
/* app layer */
#define UPWARD_TRAFFIC                      1
#define DOWNWARD_TRAFFIC    				0
#define APP_MAX_SEQNO                       200 /* 1000, 200 */
#define CONF_START_DELAY                    30 /* Cooja: 30, Testbed: 180 */
#define CONF_SEND_INTERVAL                  30 /* 6, 30 */
/* rdc layer */
#define ALWAYS_ON_RDC                       0
/* phy layer */
#define CC2420_CONF_RF_POWER		    	31 /* Cooja: 31, testbed: 11 */
/* cooja end */

#elif COOJA_EVAL_2
/* topology */
#define ROOT_ID                             1
#define MAX_NODES                           25
#define MAX_MEMORIES                        25
#define TESTBED_01                          0
#define SINGLE_SENDER_ID                    MAX_NODES
/* app layer */
#define UPWARD_TRAFFIC                      1
#define DOWNWARD_TRAFFIC    				1
#define APP_MAX_SEQNO                       100 /* 1000, 200 */
#define CONF_START_DELAY                    60 /* Cooja: 30, Testbed: 180 */
#define CONF_SEND_INTERVAL                  60 /* 6, 30 */
/* rpl layer */
#define RPL_CONF_LEAF_ONLY                  1
#define CONF_MN_NO_ROUTING                  0
/* rdc layer */
#define ALWAYS_ON_RDC                       0
/* phy layer */
#define CC2420_CONF_RF_POWER		    	31 /* Cooja: 31, testbed: 11 */
/* cooja end */
#endif


/* ipv6 layer */
#undef NBR_TABLE_CONF_MAX_NEIGHBORS
#define NBR_TABLE_CONF_MAX_NEIGHBORS        MAX_MEMORIES
#undef UIP_CONF_MAX_ROUTES
#define UIP_CONF_MAX_ROUTES                 MAX_MEMORIES

/* rpl layer */
#define RPL_CONF_WITH_PROBING               0
#define RPL_CONF_DIO_INTERVAL_MIN           12 /* 12, 10 */
#define RPL_CONF_DIO_INTERVAL_DOUBLINGS     8  /*  8, 2 */


/* mobirpl - operations */
#define MOBIRPL_MOBILITY_DETECTION          0
#define MOBIRPL_CONNECTIVITY_MANAGEMENT     0
#define MOBIRPL_RH_OF 					    0
#if MOBIRPL_CONNECTIVITY_MANAGEMENT
#define MOBIRPL_NULLIFY	                    1
#define MOBIRPL_UNICAST_PROBING             1
#define MOBIRPL_PROACTIVE_DISCOVERY         1
#endif

/* mobirpl - configurable parameters */
#define MOBIRPL_LIFETIME_MINIMUM_INTCURR	(RPL_CONF_DIO_INTERVAL_MIN + 2) /* 12 + 2 */
#define LINK_LOSS_THRESHOLD		            2 /* N-consecutive link losses */
#define RSSI_LOW_THRESHOLD  				-83 /* -87, -84, -81, -78, -75, -72 */


/* mobirpl - mobility detection */
#define MOBIRPL_STABILITY_THRESHOLD         (60 * 2 * MOBIRPL_SCALE)
#define MOBIRPL_SCALE						100
#define MOBIRPL_ALPHA						70

/* mobirpl - connectivity management */
#define MOBIRPL_LIFETIME_MAXIMUM_INTCURR	(RPL_CONF_DIO_INTERVAL_MIN + RPL_CONF_DIO_INTERVAL_DOUBLINGS)
#define MOBIRPL_LIFETIME_INITIAL_INTCURR	MOBIRPL_LIFETIME_MINIMUM_INTCURR
#define MOBIRPL_PROBING_DENOMINATOR         (LINK_LOSS_THRESHOLD + 1)

/* mobirpl - rhof setting */
#define RPL_NOACK_RSSI 				    	-100
#define RSSI_DIFFERENCE_HYSTERESIS          4
#if MOBIRPL_RH_OF
#undef RPL_CONF_OF
#define RPL_CONF_OF 					    rpl_rhof
#define MOBIRPL_RANK_FILTER		            1
#else
#define RPL_CONF_OF 					    rpl_mrhof
#define CONF_PARENT_SWITCH_THRESHOLD_DIV    2 /* 2 vs 0 */
#endif


/* csma layer */
#define CSMA_CONF_MAX_NEIGHBOR_QUEUES               5 //3 /* max # of co-existing queues: 2 in default */
#undef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM                           8 /* max packets: 8 in default */
#define SICSLOWPAN_CONF_MAX_MAC_TRANSMISSIONS       5 /* max retx: 5 in default */

/* rdc layer */
#if ALWAYS_ON_RDC
#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC nullrdc_driver
#define NULLRDC_CONF_802154_AUTOACK                 1
#else
#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC contikimac_driver
#define REMOVE_STROBE_FROM_ONE_HOP_NODE_TO_SINK     0
#undef NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE
#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE        32 /* 8, 16 */
#endif

/* phy layer */
#define RF_CHANNEL  						26
#undef CC2420_CONF_CCA_THRESH
#define CC2420_CONF_CCA_THRESH  			-42 /* -45 + 3 -> -87 */

/* log */
#define LOG_MAGIC 0xcafebabe
#define LOG_NODEID_FROM_IPADDR(addr) ((addr) ? (addr)->u8[15] : 0)
#define LOG_NODEID_FROM_LINKADDR(addr) ((addr) ? (addr)->u8[7] : 0)

#endif /* __PROJECT_CONF_H__ */    
