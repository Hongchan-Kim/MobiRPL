/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *         An implementation of RPL's objective function 0.
 *
 * \author Joakim Eriksson <joakime@sics.se>, Nicolas Tsiftes <nvt@sics.se>
 */

/**
 * \addtogroup uip6
 * @{
 */

#include "net/rpl/rpl-private.h"
#include "net/nbr-table.h"

#define DEBUG DEBUG_NONE
#include "net/ip/uip-debug.h"

static void reset(rpl_dag_t *);
static void neighbor_link_callback(rpl_parent_t *, int, int);
static rpl_parent_t *best_parent(rpl_parent_t *, rpl_parent_t *);
static rpl_dag_t *best_dag(rpl_dag_t *, rpl_dag_t *);
static rpl_rank_t calculate_rank(rpl_parent_t *, rpl_rank_t);
static void update_metric_container(rpl_instance_t *);

static uint16_t preferred_parent_callback_num;
static uint16_t non_preferred_parent_callback_num;

rpl_of_t rpl_rhof = {
  reset,
  neighbor_link_callback,
  best_parent,
  best_dag,
  calculate_rank,
  update_metric_container,
  0
};

#define DEFAULT_RANK_INCREMENT  RPL_MIN_HOPRANKINC
#define MIN_DIFFERENCE          RPL_MIN_HOPRANKINC

static void
reset(rpl_dag_t *dag)
{
  PRINTF("RPL: Resetting RH-OF\n");
}
/*---------------------------------------------------------------------------*/
static void
neighbor_link_callback(rpl_parent_t *p, int status, int rssi)
{
  uip_ds6_nbr_t *nbr = NULL;
  nbr = rpl_get_nbr(p);
  if(nbr == NULL) {
    /* No neighbor for this parent - something bad has occurred */
    return;
  }

  /* count the number of link metric update */
  if(p == p->dag->preferred_parent) {
    preferred_parent_callback_num++;
  } else {
    non_preferred_parent_callback_num++;
  }

  int16_t rssi_old = p->rssi;
  int16_t packet_rssi = rssi;

  if(status == MAC_TX_OK || status == MAC_TX_NOACK) {
    if(status == MAC_TX_NOACK) {
      packet_rssi = p->rssi; /* reuse last RSSI value */
    }

    p->rssi = packet_rssi;

    /* determine zone considering hysteresis */
    if(p->zone >= MOBIRPL_GRAY_ZONE) {
      if(p->rssi >= RSSI_LOW_THRESHOLD + RSSI_DIFFERENCE_HYSTERESIS) {
        p->zone = MOBIRPL_WHITE_ZONE;
      } else {
        p->zone = MOBIRPL_GRAY_ZONE;
      }
    } else {
      if(p->rssi >= RSSI_LOW_THRESHOLD) {
        p->zone = MOBIRPL_WHITE_ZONE;
      } else {
        p->zone = MOBIRPL_GRAY_ZONE;
      }
    }

    if(!(p->flags & RPL_PARENT_FLAG_LINK_METRIC_VALID)) {
      /* Set link metric as valid */
      p->flags |= RPL_PARENT_FLAG_LINK_METRIC_VALID;
    }

    /* update the link metric for this nbr */
    //nbr->link_metric = -(p->rssi);
    nbr->link_metric = RPL_MIN_HOPRANKINC;

#if MOBIRPL_CONNECTIVITY_MANAGEMENT
    if(status == MAC_TX_OK) {
      p->link_loss_count = 0;
    } else if(status == MAC_TX_NOACK) {
      p->link_loss_count++;
    }
#endif

    printf("r:a_cb|%u|%u|%u||%d|%u|%u|%u|%u|%u\n", 
            LOG_NODEID_FROM_IPADDR(rpl_get_parent_ipaddr(p)),
            preferred_parent_callback_num,
            non_preferred_parent_callback_num,
            p->rssi,
            p->link_loss_count,
            p->zone,
            p->lifetime, 
            p->mobility, 
            calculate_flag(p));
  }
}
/*---------------------------------------------------------------------------*/
void
mobirpl_rx_callback(rpl_parent_t *p, int16_t rssi)
{
  uip_ds6_nbr_t *nbr = NULL;
  nbr = rpl_get_nbr(p);
  if(nbr == NULL) {
    /* No neighbor for this parent - something bad has occurred */
    return;
  }

  /* count the number of link metric update */
  if(p == p->dag->preferred_parent) {
    preferred_parent_callback_num++;
  } else {
    non_preferred_parent_callback_num++;
  }

  int16_t rssi_old = p->rssi;
  p->rssi = rssi;

  /* determine zone considering hysteresis */
  if(p->zone >= MOBIRPL_GRAY_ZONE) {
    if(p->rssi >= RSSI_LOW_THRESHOLD + RSSI_DIFFERENCE_HYSTERESIS) {
      p->zone = MOBIRPL_WHITE_ZONE;
    } else {
      p->zone = MOBIRPL_GRAY_ZONE;
    }
  } else {
    if(p->rssi >= RSSI_LOW_THRESHOLD) {
      p->zone = MOBIRPL_WHITE_ZONE;
    } else {
      p->zone = MOBIRPL_GRAY_ZONE;
    }
  }

  if(!(p->flags & RPL_PARENT_FLAG_LINK_METRIC_VALID)) {
    /* Set link metric as valid */
    p->flags |= RPL_PARENT_FLAG_LINK_METRIC_VALID;
  }

  /* update the link metric for this nbr */
  //nbr->link_metric = -(p->rssi);
  nbr->link_metric = RPL_MIN_HOPRANKINC;

#if MOBIRPL_CONNECTIVITY_MANAGEMENT
  p->link_loss_count = 0;
#endif

  printf("r:r_cb|%u|%u|%u||%d|%u|%u|%u|%u|%u\n", 
          LOG_NODEID_FROM_IPADDR(rpl_get_parent_ipaddr(p)),
          preferred_parent_callback_num,
          non_preferred_parent_callback_num,
          p->rssi,
          p->link_loss_count,
          p->zone,
          p->lifetime, 
          p->mobility, 
          calculate_flag(p));
}
/*---------------------------------------------------------------------------*/
static rpl_rank_t
calculate_rank(rpl_parent_t *p, rpl_rank_t base_rank)
{
  rpl_rank_t increment;
  if(base_rank == 0) {
    if(p == NULL) {
      return INFINITE_RANK;
    }
    base_rank = p->rank;
  }

  increment = p != NULL ?
                p->dag->instance->min_hoprankinc :
                DEFAULT_RANK_INCREMENT;

  if((rpl_rank_t)(base_rank + increment) < base_rank) {
    PRINTF("RPL: OF0 rank %d incremented to infinite rank due to wrapping\n",
        base_rank);
    return INFINITE_RANK;
  }
  return base_rank + increment;

}
/*---------------------------------------------------------------------------*/
static rpl_dag_t *
best_dag(rpl_dag_t *d1, rpl_dag_t *d2)
{
  if(d1->grounded) {
    if (!d2->grounded) {
      return d1;
    }
  } else if(d2->grounded) {
    return d2;
  }

  if(d1->preference < d2->preference) {
    return d2;
  } else {
    if(d1->preference > d2->preference) {
      return d1;
    }
  }

  if(d2->rank < d1->rank) {
    return d2;
  } else {
    return d1;
  }
}
/*---------------------------------------------------------------------------*/
#if MOBIRPL_MOBILITY_DETECTION /* hckim mobirpl */
uint8_t
calculate_flag(rpl_parent_t *p)
{
  if(mobirpl_mobility == MOBIRPL_MOBILE_NODE) {
    if(p->zone <= MOBIRPL_WHITE_ZONE) {
      if(p->mobility == 0) {
        return MOBIRPL_FLAG_1;
      } else {
        return MOBIRPL_FLAG_2;
      }
    } else {
      if(p->mobility == 0) {
        return MOBIRPL_FLAG_3;
      } else {
        return MOBIRPL_FLAG_4;
      }
    }
  } else {
    if(p->mobility == 0) {
      if(p->zone <= MOBIRPL_WHITE_ZONE) {
        return MOBIRPL_FLAG_1;
      } else {
        return MOBIRPL_FLAG_2;
      }
    } else {
      if(p->zone  <= MOBIRPL_WHITE_ZONE) {
        return MOBIRPL_FLAG_3;
      } else {
        return MOBIRPL_FLAG_4;
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
static rpl_parent_t *
best_parent(rpl_parent_t *p1, rpl_parent_t *p2)
{
  rpl_rank_t r1, r2;
  rpl_dag_t *dag;  
  uip_ds6_nbr_t *nbr1, *nbr2;
  nbr1 = rpl_get_nbr(p1);
  nbr2 = rpl_get_nbr(p2);

  dag = (rpl_dag_t *)p1->dag; /* Both parents must be in the same DAG. */

  if(nbr1 == NULL || nbr2 == NULL) {
    return dag->preferred_parent;
  }

  PRINTF("RPL: Comparing parent ");
  PRINT6ADDR(rpl_get_parent_ipaddr(p1));
  PRINTF(" (confidence %d, rank %d) with parent ",
        nbr1->link_metric, p1->rank);
  PRINT6ADDR(rpl_get_parent_ipaddr(p2));
  PRINTF(" (confidence %d, rank %d)\n",
        nbr2->link_metric, p2->rank);

  r1 = DAG_RANK(p1->rank, p1->dag->instance) * RPL_MIN_HOPRANKINC;
  r2 = DAG_RANK(p2->rank, p1->dag->instance) * RPL_MIN_HOPRANKINC;

  uint8_t flag1 = calculate_flag(p1);
  uint8_t flag2 = calculate_flag(p2);

  if(flag1 < flag2) {
    return p1;
  } else if(flag2 < flag1) {
    return p2;
  } else {
    if(r1 < r2) {
      return p1;
    } else if(r2 < r1) {
      return p2;
    } else {
      if(p1->rssi < p2->rssi + RSSI_DIFFERENCE_HYSTERESIS &&
        p2->rssi < p1->rssi + RSSI_DIFFERENCE_HYSTERESIS) {
        if(p1 == dag->preferred_parent || p2 == dag->preferred_parent) {
          return dag->preferred_parent;
        } else {
#if MOBIRPL_CONNECTIVITY_MANAGEMENT
          if(p1->lifetime >= p2->lifetime) {
            return p1;
          } else {
            return p2;
          }
#else
          return p1;
#endif
        }
      } else if(p1->rssi > p2->rssi) {
        return p1;
      } else {
        return p2;
      }
    }
  }
}
#else /* MOBIRPL STABILITY */
static rpl_parent_t *
best_parent(rpl_parent_t *p1, rpl_parent_t *p2)
{
  rpl_rank_t r1, r2;
  rpl_dag_t *dag;  
  uip_ds6_nbr_t *nbr1, *nbr2;
  nbr1 = rpl_get_nbr(p1);
  nbr2 = rpl_get_nbr(p2);

  dag = (rpl_dag_t *)p1->dag; /* Both parents must be in the same DAG. */

  if(nbr1 == NULL || nbr2 == NULL) {
    return dag->preferred_parent;
  }

  PRINTF("RPL: Comparing parent ");
  PRINT6ADDR(rpl_get_parent_ipaddr(p1));
  PRINTF(" (confidence %d, rank %d) with parent ",
        nbr1->link_metric, p1->rank);
  PRINT6ADDR(rpl_get_parent_ipaddr(p2));
  PRINTF(" (confidence %d, rank %d)\n",
        nbr2->link_metric, p2->rank);

  r1 = DAG_RANK(p1->rank, p1->dag->instance) * RPL_MIN_HOPRANKINC;
  r2 = DAG_RANK(p2->rank, p1->dag->instance) * RPL_MIN_HOPRANKINC;

  if(p1->zone < p2->zone) {
    return p1;
  } else if(p2->zone < p1->zone) {
    return p2;
  } else {
    if(r1 < r2) {
      return p1;
    } else if(r2 < r1) {
      return p2;
    } else {
      if(p1->rssi < p2->rssi + RSSI_DIFFERENCE_HYSTERESIS &&
        p2->rssi < p1->rssi + RSSI_DIFFERENCE_HYSTERESIS) {
        if(p1 == dag->preferred_parent || p2 == dag->preferred_parent) {
          return dag->preferred_parent;
        } else {
#if MOBIRPL_CONNECTIVITY_MANAGEMENT
          if(p1->lifetime >= p2->lifetime) {
            return p1;
          } else {
            return p2;
          }
#else
          return p1;
#endif
        }
      } else if(p1->rssi > p2->rssi) {
        return p1;
      } else {
        return p2;
      }
    }
  }
}
#endif /* MOBIRPL STABILITY */
/*---------------------------------------------------------------------------*/
static void
update_metric_container(rpl_instance_t *instance)
{
  instance->mc.type = RPL_DAG_MC_NONE;
}

/** @}*/
