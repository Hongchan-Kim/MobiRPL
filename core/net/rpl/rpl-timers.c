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
 *         RPL timer management.
 *
 * \author Joakim Eriksson <joakime@sics.se>, Nicolas Tsiftes <nvt@sics.se>
 */

/**
 * \addtogroup uip6
 * @{
 */

#include "contiki-conf.h"
#include "net/rpl/rpl-private.h"
#include "net/ipv6/multicast/uip-mcast6.h"
#include "lib/random.h"
#include "sys/ctimer.h"

#define DEBUG DEBUG_NONE
#include "net/ip/uip-debug.h"

/*---------------------------------------------------------------------------*/
static struct ctimer periodic_timer;

static void handle_periodic_timer(void *ptr);
static void new_dio_interval(rpl_instance_t *instance);
static void handle_dio_timer(void *ptr);

static uint16_t next_dis;

#if MOBIRPL_MOBILITY_DETECTION /* hckim mobirpl */
static uint8_t pp_change_flag;
static uint32_t pp_change_time_current;
static uint32_t pp_change_time_average;
static uint32_t pp_change_time_metric;
static uint32_t pp_change_time_window;
static uint8_t mobility_update_flag;
#endif

#if MOBIRPL_CONNECTIVITY_MANAGEMENT /* hckim mobirpl */
static uint16_t next_reactive_discovery;
static uint16_t mobirpl_reactive_discovery_num;
static uint8_t mobirpl_first_reactive_discovery = 1;
static uint16_t mobirpl_probe_interval;
static uint16_t mobirpl_probe_num;
static uint8_t mobirpl_proactive_discovery_flag;
static uint16_t next_proactive_discovery;
static uint16_t mobirpl_proactive_discovery_num;
#endif

/* dio_send_ok is true if the node is ready to send DIOs */
static uint8_t dio_send_ok;


/*---------------------------------------------------------------------------*/
/* hckim mobirpl */
/*---------------------------------------------------------------------------*/
static void
reset_mobirpl()
{
  printf("r:R\n");
#if MOBIRPL_MOBILITY_DETECTION /* hckim mobirpl */
  if(node_id != ROOT_ID) {
    mobirpl_mobility = MOBIRPL_MOBILE_NODE;
    pp_change_flag = MOBIRPL_UNJOINED_NODE;
    pp_change_time_current = 0;
    pp_change_time_average = (1UL << RPL_CONF_DIO_INTERVAL_MIN) / 1000 * 100;
    pp_change_time_metric = pp_change_time_average;
    pp_change_time_window = pp_change_time_metric / 100;
  } else {
    mobirpl_mobility = MOBIRPL_STATIC_NODE;
    pp_change_flag = MOBIRPL_ROOT_NODE;
  }
#endif

#if MOBIRPL_CONNECTIVITY_MANAGEMENT /* hckim mobirpl */
  if(node_id != ROOT_ID) {
    mobirpl_timeout_period_intcurr = MOBIRPL_LIFETIME_INITIAL_INTCURR;
    uint32_t time = 1UL << mobirpl_timeout_period_intcurr;
    mobirpl_timeout_period_current = time / 1000;
    mobirpl_probe_interval = mobirpl_timeout_period_current / MOBIRPL_PROBING_DENOMINATOR;

#if MOBIRPL_MOBILITY_DETECTION /* hckim mobirpl */
    printf("r:l|%u|%u|%u\n", 
      mobirpl_timeout_period_intcurr, 
      LINK_LOSS_THRESHOLD, 
      pp_change_flag);
#else
    printf("r:l|%u|%u\n", 
      mobirpl_timeout_period_intcurr, 
      mobirpl_probe_interval);
#endif
  }
#endif /* MOBIRPL CONNECTIVITY */


}
/*---------------------------------------------------------------------------*/
/* hckim mobirpl */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
#if MOBIRPL_MOBILITY_DETECTION /* hckim mobirpl */
/*---------------------------------------------------------------------------*/
void
mobirpl_set_pp_change_flag(uint8_t flag)
{
  pp_change_flag = flag;
}
/*---------------------------------------------------------------------------*/
void
mobirpl_detect_mobility()
{
  mobility_update_flag = 0;

  if(pp_change_flag == MOBIRPL_UNJOINED_NODE || pp_change_flag == MOBIRPL_ROOT_NODE) {
    /* unjoined node or root node */
  } else {  
    pp_change_time_current++;

    if(pp_change_flag == MOBIRPL_PARENT_SWITCH) {
      pp_change_time_average =
        ((uint32_t) pp_change_time_average * MOBIRPL_ALPHA 
        + (uint32_t) (pp_change_time_current * MOBIRPL_SCALE) * (MOBIRPL_SCALE - MOBIRPL_ALPHA)) 
        / MOBIRPL_SCALE;
      pp_change_time_metric = pp_change_time_average;
      pp_change_time_window = pp_change_time_metric / MOBIRPL_SCALE;
      pp_change_time_current = 0;

      mobility_update_flag = 1;

    } else if(pp_change_time_window > 0) {
      pp_change_time_window--;
      if(pp_change_time_window == 0) {
        pp_change_time_metric = 
          ((uint32_t) pp_change_time_average * MOBIRPL_ALPHA
          + (uint32_t) (pp_change_time_current * MOBIRPL_SCALE) * (MOBIRPL_SCALE - MOBIRPL_ALPHA))
          / MOBIRPL_SCALE;
        pp_change_time_window = pp_change_time_metric / MOBIRPL_SCALE;

        mobility_update_flag = 1;
      }
    }

    if(pp_change_time_metric < MOBIRPL_STABILITY_THRESHOLD) {
      mobirpl_mobility = MOBIRPL_MOBILE_NODE; /* mobile node */
    } else {
      mobirpl_mobility = MOBIRPL_STATIC_NODE; /* static node */
    }

    if(mobility_update_flag == 1) {
      printf("r:u|%u|%u|%u|%lu|%lu|%lu\n", 
        pp_change_flag,
        mobility_update_flag,
        mobirpl_mobility,
        (pp_change_time_average / 100), 
        (pp_change_time_metric  / 100), 
        pp_change_time_current);
    }
  }

  if(pp_change_flag == MOBIRPL_PARENT_SWITCH) {
    pp_change_flag = MOBIRPL_NO_PARENT_SWITCH;
  }
}
/*---------------------------------------------------------------------------*/
#endif
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
#if MOBIRPL_CONNECTIVITY_MANAGEMENT /* hckim mobirpl */
/*---------------------------------------------------------------------------*/
uint8_t mobirpl_non_black_parent_num()
{
  uint8_t non_black_parents = 0;
  rpl_parent_t *p;
  p = nbr_table_head(rpl_parents);
  while(p != NULL) {
    if(p->zone != 3) {
      ++non_black_parents;
    }
    p = nbr_table_next(rpl_parents, p);
  }
  return non_black_parents;
}
/*---------------------------------------------------------------------------*/
void
mobirpl_reset_lifetime(rpl_parent_t *p)
{
  p->lifetime = mobirpl_timeout_period_current;
  printf("r:rs|%u|%u\n", LOG_NODEID_FROM_IPADDR(rpl_get_parent_ipaddr(p)), 
          p->lifetime); 
}
/*---------------------------------------------------------------------------*/
void
mobirpl_expire_lifetime(rpl_parent_t *p)
{
  p->lifetime = 0;
  printf("r:ep|%u|%u\n", LOG_NODEID_FROM_IPADDR(rpl_get_parent_ipaddr(p)), 
          p->lifetime); 
}
/*---------------------------------------------------------------------------*/
void
mobirpl_set_proactive_discovery_flag(uint8_t flag)
{
#if MOBIRPL_PROACTIVE_DISCOVERY
  mobirpl_proactive_discovery_flag = flag;
#endif
}
/*---------------------------------------------------------------------------*/
void
mobirpl_manage_connectivity()
{
  if(node_id == ROOT_ID) {
    /* 
     * root node does not have entries in parent table
     * therefore does not perform connectivity management
    */
    return;
  }

  rpl_parent_t *p = NULL;
  rpl_parent_t *preferred_p = NULL;

  /* check N-consecutive link losses */
  p = nbr_table_head(rpl_parents);
  while(p != NULL) {
    if(p == p->dag->preferred_parent) {
      preferred_p = p;
    }
    if(p->link_loss_count >= LINK_LOSS_THRESHOLD) {
      if(p->zone < MOBIRPL_BLACK_ZONE) {
        printf("r:cl|%u\n", LOG_NODEID_FROM_IPADDR(rpl_get_parent_ipaddr(p)));
        p->zone = MOBIRPL_BLACK_ZONE;
        mobirpl_expire_lifetime(p);
        p->flags &= ~RPL_PARENT_FLAG_LINK_METRIC_VALID;
        p->flags |= RPL_PARENT_FLAG_UPDATED;
      }
    }
    p = nbr_table_next(rpl_parents, p);
  }
  p = NULL;

  /* update lifetime */
  p = nbr_table_head(rpl_parents);
  while(p != NULL) {
    if(p->lifetime >= 1) {
      p->lifetime--;
      if(p->lifetime == 0) {
        printf("r:to|%u\n", LOG_NODEID_FROM_IPADDR(rpl_get_parent_ipaddr(p))); 
        p->zone = MOBIRPL_BLACK_ZONE;
        p->flags &= ~RPL_PARENT_FLAG_LINK_METRIC_VALID;
        p->flags |= RPL_PARENT_FLAG_UPDATED;
      }
    }
    p = nbr_table_next(rpl_parents, p);
  }
  p = NULL;

  /* recalculate timeout period */
#if MOBIRPL_MOBILITY_DETECTION /* hckim mobirpl */
  if(mobility_update_flag) {
    uint8_t last_intcurr = mobirpl_timeout_period_intcurr;
    uint16_t last_timeout_period = mobirpl_timeout_period_current;

    if(mobirpl_mobility == MOBIRPL_MOBILE_NODE) {
      mobirpl_timeout_period_intcurr = MOBIRPL_LIFETIME_MINIMUM_INTCURR;
    } else {
      mobirpl_timeout_period_intcurr = 
        (mobirpl_timeout_period_intcurr < MOBIRPL_LIFETIME_MAXIMUM_INTCURR ? 
        mobirpl_timeout_period_intcurr + 1 : mobirpl_timeout_period_intcurr);
    }
    uint32_t time = 1UL << mobirpl_timeout_period_intcurr;
    mobirpl_timeout_period_current = time / 1000;

    /* recalculate probe interval */
    uint16_t last_probe_interval = mobirpl_probe_interval;                                                                                            
    mobirpl_probe_interval = mobirpl_timeout_period_current / MOBIRPL_PROBING_DENOMINATOR;

    printf("r:l|%u|%u\n", 
      last_intcurr,
      mobirpl_timeout_period_intcurr);

    /* adjust lifetime */
    p = nbr_table_head(rpl_parents);
    if(last_intcurr > mobirpl_timeout_period_intcurr) { // decrease lifetime
      uint8_t difference = last_intcurr - mobirpl_timeout_period_intcurr;
      while(p != NULL) {
        if(p->lifetime == 0) {
          /* do nothing */
        } else {
          /* new lifetime must be greater than or equal to 1 */
          uint16_t new_lifetime = (p->lifetime >> difference) + 1;
          p->lifetime = new_lifetime;
        }
        p = nbr_table_next(rpl_parents, p);
      }
    } else if(last_intcurr < mobirpl_timeout_period_intcurr) { //increase lifetime
      uint8_t difference = mobirpl_timeout_period_intcurr - last_intcurr;
      while(p != NULL) {
        if(p->lifetime == 0) {
          /* do nothing */
        } else {
          uint16_t new_lifetime = 
            (p->lifetime << difference) > mobirpl_timeout_period_current ?
            mobirpl_timeout_period_current : (p->lifetime << difference);
          p->lifetime = new_lifetime;
        }      
        p = nbr_table_next(rpl_parents, p);
      }
    } else {
      /* same interval: do nothing */
    }
  }
#endif

  /* unicast probing */
#if MOBIRPL_UNICAST_PROBING
  if(preferred_p != NULL) {
    if((mobirpl_timeout_period_current > preferred_p->lifetime) && 
       (preferred_p->lifetime > 0) &&
       ((mobirpl_timeout_period_current - preferred_p->lifetime) % mobirpl_probe_interval == 0)) {
      printf("r:p|%u\n", ++mobirpl_probe_num);
/*
      printf("r:p|%u|%u|%u|%u\n", 
        ++mobirpl_probe_num, 
        mobirpl_probe_interval, 
        mobirpl_timeout_period_current, 
        preferred_p->lifetime);
*/
      dis_output(rpl_get_parent_ipaddr(preferred_p), 0);
    }
  }
#endif
}
/*---------------------------------------------------------------------------*/
void
mobirpl_proactive_discovery()
{
  /* multicast discovery */
#if MOBIRPL_PROACTIVE_DISCOVERY
  if(node_id == ROOT_ID) {
    return;
  }

  if(next_proactive_discovery > 0) {
    next_proactive_discovery--;
    mobirpl_proactive_discovery_flag = 0;
    return;
  }

  /* proactive discovery */
  if(mobirpl_proactive_discovery_flag == 1) {

    ++mobirpl_proactive_discovery_num;
    printf("r:dc|p|%u|%u\n",
      mobirpl_proactive_discovery_num, mobirpl_reactive_discovery_num);

    dis_output(NULL, 1);

    next_proactive_discovery = mobirpl_probe_interval;
    mobirpl_proactive_discovery_flag = 0;
  }
#endif
}
/*---------------------------------------------------------------------------*/
#endif /* MOBIRPL CONNECTIVITY */
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
static void
handle_periodic_timer(void *ptr)
{
#if MOBIRPL_MOBILITY_DETECTION /* hckim mobirpl */
  mobirpl_detect_mobility();
#endif /* MOBIRPL STABILITY */

#if MOBIRPL_CONNECTIVITY_MANAGEMENT /* hckim mobirpl */
  mobirpl_manage_connectivity();
#endif /* MOBIRPL CONNECTIVITY */

  rpl_purge_routes();

  rpl_recalculate_ranks();

#if MOBIRPL_CONNECTIVITY_MANAGEMENT /* hckim mobirpl */
  mobirpl_proactive_discovery();
  /* reactive or periodic discovery */
  if(node_id != ROOT_ID) {

    if(mobirpl_non_black_parent_num() == 0) {
      if(mobirpl_first_reactive_discovery == 1) {

        ++mobirpl_reactive_discovery_num;
        printf("r:dc|r|%u|%u\n",
          mobirpl_proactive_discovery_num, mobirpl_reactive_discovery_num);

        dis_output(NULL, 0);
        mobirpl_first_reactive_discovery = 0;
        next_reactive_discovery = 0;
      } else {
        next_reactive_discovery++;
      }

      if(next_reactive_discovery >= mobirpl_probe_interval || next_reactive_discovery >= RPL_DIS_INTERVAL) {
        reset_mobirpl();
        next_reactive_discovery = 0;

        ++mobirpl_reactive_discovery_num;
        printf("r:dc|r|%u|%u\n",
          mobirpl_proactive_discovery_num, mobirpl_reactive_discovery_num);

        dis_output(NULL, 0);
      }
    } else {
      mobirpl_first_reactive_discovery = 1;
      next_reactive_discovery = 0;
    }
  }

#else /* MOBIRPL_CONNECTIVITY_MANAGEMENT */

  /* handle DIS */
#if RPL_DIS_SEND
  next_dis++;
  if(rpl_get_any_dag() == NULL && next_dis >= RPL_DIS_INTERVAL) {
    next_dis = 0;
    dis_output(NULL, 0);
  }
#endif
#endif /* MOBIRPL_CONNECTIVITY_MANAGEMENT */

  ctimer_reset(&periodic_timer);
}
/*---------------------------------------------------------------------------*/
static void
new_dio_interval(rpl_instance_t *instance)
{
  uint32_t time;
  clock_time_t ticks;

  /* TODO: too small timer intervals for many cases */
  time = 1UL << instance->dio_intcurrent;

  /* Convert from milliseconds to CLOCK_TICKS. */
  ticks = (time * CLOCK_SECOND) / 1000;
  instance->dio_next_delay = ticks;

  /* random number between I/2 and I */
  ticks = ticks / 2 + (ticks / 2 * (uint32_t)random_rand()) / RANDOM_RAND_MAX;

  /*
   * The intervals must be equally long among the nodes for Trickle to
   * operate efficiently. Therefore we need to calculate the delay between
   * the randomized time and the start time of the next interval.
   */
  instance->dio_next_delay -= ticks;
  instance->dio_send = 1;

#if RPL_CONF_STATS
  /* keep some stats */
  instance->dio_totint++;
  instance->dio_totrecv += instance->dio_counter;
  ANNOTATE("#A rank=%u.%u(%u),stats=%d %d %d %d,color=%s\n",
	   DAG_RANK(instance->current_dag->rank, instance),
           (10 * (instance->current_dag->rank % instance->min_hoprankinc)) / instance->min_hoprankinc,
           instance->current_dag->version,
           instance->dio_totint, instance->dio_totsend,
           instance->dio_totrecv,instance->dio_intcurrent,
	   instance->current_dag->rank == ROOT_RANK(instance) ? "BLUE" : "ORANGE");
#endif /* RPL_CONF_STATS */

  /* reset the redundancy counter */
  instance->dio_counter = 0;

  /* schedule the timer */
  PRINTF("RPL: Scheduling DIO timer %lu ticks in future (Interval)\n", ticks);
  ctimer_set(&instance->dio_timer, ticks, &handle_dio_timer, instance);
}
/*---------------------------------------------------------------------------*/
static void
handle_dio_timer(void *ptr)
{
  rpl_instance_t *instance;

  instance = (rpl_instance_t *)ptr;

  PRINTF("RPL: DIO Timer triggered\n");
  if(!dio_send_ok) {
    if(uip_ds6_get_link_local(ADDR_PREFERRED) != NULL) {
      dio_send_ok = 1;
    } else {
      PRINTF("RPL: Postponing DIO transmission since link local address is not ok\n");
      ctimer_set(&instance->dio_timer, CLOCK_SECOND, &handle_dio_timer, instance);
      return;
    }
  }

  if(instance->dio_send) {
    /* send DIO if counter is less than desired redundancy */
    if(instance->dio_redundancy != 0 && instance->dio_counter < instance->dio_redundancy) {
#if RPL_CONF_STATS
      instance->dio_totsend++;
#endif /* RPL_CONF_STATS */
      dio_output(instance, NULL);
    } else {
      PRINTF("RPL: Supressing DIO transmission (%d >= %d)\n",
             instance->dio_counter, instance->dio_redundancy);
    }
    instance->dio_send = 0;
    PRINTF("RPL: Scheduling DIO timer %lu ticks in future (sent)\n",
           instance->dio_next_delay);
    ctimer_set(&instance->dio_timer, instance->dio_next_delay, handle_dio_timer, instance);
  } else {
    /* check if we need to double interval */
    if(instance->dio_intcurrent < instance->dio_intmin + instance->dio_intdoubl) {
      instance->dio_intcurrent++;
      PRINTF("RPL: DIO Timer interval doubled %d\n", instance->dio_intcurrent);
    }
    new_dio_interval(instance);
  }

#if DEBUG
  rpl_print_neighbor_list();
#endif
}
/*---------------------------------------------------------------------------*/
void
rpl_reset_periodic_timer(void)
{
  reset_mobirpl();

  next_dis = RPL_DIS_INTERVAL / 2 +
    ((uint32_t)RPL_DIS_INTERVAL * (uint32_t)random_rand()) / RANDOM_RAND_MAX -
    RPL_DIS_START_DELAY;

  ctimer_set(&periodic_timer, CLOCK_SECOND, handle_periodic_timer, NULL);
}
/*---------------------------------------------------------------------------*/
/* Resets the DIO timer in the instance to its minimal interval. */
void
rpl_reset_dio_timer(rpl_instance_t *instance)
{
  static uint16_t reset_dio_timer_num;
  printf("r:r_d_t|%u\n", ++reset_dio_timer_num);

#if !RPL_LEAF_ONLY
  /* Do not reset if we are already on the minimum interval,
     unless forced to do so. */
  if(instance->dio_intcurrent > instance->dio_intmin) {
    instance->dio_counter = 0;
    instance->dio_intcurrent = instance->dio_intmin;
    new_dio_interval(instance);
  }
#if RPL_CONF_STATS
  rpl_stats.resets++;
#endif /* RPL_CONF_STATS */
#endif /* RPL_LEAF_ONLY */
}
/*---------------------------------------------------------------------------*/
static void handle_dao_timer(void *ptr);
static void
set_dao_lifetime_timer(rpl_instance_t *instance)
{
  if(rpl_get_mode() == RPL_MODE_FEATHER) {
    return;
  }

  /* Set up another DAO within half the expiration time, if such a
     time has been configured */
  if(instance->lifetime_unit != 0xffff && instance->default_lifetime != 0xff) {
    clock_time_t expiration_time;
    expiration_time = (clock_time_t)instance->default_lifetime *
      (clock_time_t)instance->lifetime_unit *
      CLOCK_SECOND / 2;
    PRINTF("RPL: Scheduling DAO lifetime timer %u ticks in the future\n",
           (unsigned)expiration_time);
    ctimer_set(&instance->dao_lifetime_timer, expiration_time,
               handle_dao_timer, instance);
  }
}
/*---------------------------------------------------------------------------*/
static void
handle_dao_timer(void *ptr)
{
  rpl_instance_t *instance;
#if RPL_CONF_MULTICAST
  uip_mcast6_route_t *mcast_route;
  uint8_t i;
#endif

  instance = (rpl_instance_t *)ptr;

  if(!dio_send_ok && uip_ds6_get_link_local(ADDR_PREFERRED) == NULL) {
    PRINTF("RPL: Postpone DAO transmission\n");
    ctimer_set(&instance->dao_timer, CLOCK_SECOND, handle_dao_timer, instance);
    return;
  }

  /* Send the DAO to the DAO parent set -- the preferred parent in our case. */
  if(instance->current_dag->preferred_parent != NULL) {
    PRINTF("RPL: handle_dao_timer - sending DAO\n");
    /* Set the route lifetime to the default value. */
    dao_output(instance->current_dag->preferred_parent, instance->default_lifetime);

#if RPL_CONF_MULTICAST
    /* Send DAOs for multicast prefixes only if the instance is in MOP 3 */
    if(instance->mop == RPL_MOP_STORING_MULTICAST) {
      /* Send a DAO for own multicast addresses */
      for(i = 0; i < UIP_DS6_MADDR_NB; i++) {
        if(uip_ds6_if.maddr_list[i].isused
            && uip_is_addr_mcast_global(&uip_ds6_if.maddr_list[i].ipaddr)) {
          dao_output_target(instance->current_dag->preferred_parent,
              &uip_ds6_if.maddr_list[i].ipaddr, RPL_MCAST_LIFETIME);
        }
      }

      /* Iterate over multicast routes and send DAOs */
      mcast_route = uip_mcast6_route_list_head();
      while(mcast_route != NULL) {
        /* Don't send if it's also our own address, done that already */
        if(uip_ds6_maddr_lookup(&mcast_route->group) == NULL) {
          dao_output_target(instance->current_dag->preferred_parent,
                     &mcast_route->group, RPL_MCAST_LIFETIME);
        }
        mcast_route = list_item_next(mcast_route);
      }
    }
#endif
  } else {
    PRINTF("RPL: No suitable DAO parent\n");
  }

  ctimer_stop(&instance->dao_timer);

  if(etimer_expired(&instance->dao_lifetime_timer.etimer)) {
    set_dao_lifetime_timer(instance);
  }
}
/*---------------------------------------------------------------------------*/
static void
schedule_dao(rpl_instance_t *instance, clock_time_t latency)
{
  clock_time_t expiration_time;

  if(rpl_get_mode() == RPL_MODE_FEATHER) {
    return;
  }

  expiration_time = etimer_expiration_time(&instance->dao_timer.etimer);

  if(!etimer_expired(&instance->dao_timer.etimer)) {
    PRINTF("RPL: DAO timer already scheduled\n");
  } else {
    if(latency != 0) {
      expiration_time = latency / 2 +
        (random_rand() % (latency));
    } else {
      expiration_time = 0;
    }
    PRINTF("RPL: Scheduling DAO timer %u ticks in the future\n",
           (unsigned)expiration_time);
    ctimer_set(&instance->dao_timer, expiration_time,
               handle_dao_timer, instance);

    set_dao_lifetime_timer(instance);
  }
}
/*---------------------------------------------------------------------------*/
void
rpl_schedule_dao(rpl_instance_t *instance)
{
  schedule_dao(instance, RPL_DAO_LATENCY);
}
/*---------------------------------------------------------------------------*/
void
rpl_schedule_dao_immediately(rpl_instance_t *instance)
{
  schedule_dao(instance, 0);
}
/*---------------------------------------------------------------------------*/
void
rpl_cancel_dao(rpl_instance_t *instance)
{
  ctimer_stop(&instance->dao_timer);
  ctimer_stop(&instance->dao_lifetime_timer);
}
/*---------------------------------------------------------------------------*/
#if RPL_WITH_PROBING
static rpl_parent_t *
get_probing_target(rpl_dag_t *dag)
{
  /* Returns the next probing target. The current implementation probes the current
   * preferred parent if we have not updated its link for RPL_PROBING_EXPIRATION_TIME.
   * Otherwise, it picks at random between:
   * (1) selecting the best parent not updated for RPL_PROBING_EXPIRATION_TIME
   * (2) selecting the least recently updated parent
   */

  rpl_parent_t *p;
  rpl_parent_t *probing_target = NULL;
  rpl_rank_t probing_target_rank = INFINITE_RANK;
  /* min_last_tx is the clock time RPL_PROBING_EXPIRATION_TIME in the past */
  clock_time_t min_last_tx = clock_time();
  min_last_tx = min_last_tx > 2 * RPL_PROBING_EXPIRATION_TIME
      ? min_last_tx - RPL_PROBING_EXPIRATION_TIME : 1;

  if(dag == NULL ||
      dag->instance == NULL ||
      dag->preferred_parent == NULL) {
    return NULL;
  }

  /* Our preferred parent needs probing */
  if(dag->preferred_parent->last_tx_time < min_last_tx) {
    probing_target = dag->preferred_parent;
  }

  /* With 50% probability: probe best parent not updated for RPL_PROBING_EXPIRATION_TIME */
  if(probing_target == NULL && (random_rand() % 2) == 0) {
    p = nbr_table_head(rpl_parents);
    while(p != NULL) {
      if(p->dag == dag && p->last_tx_time < min_last_tx) {
        /* p is in our dag and needs probing */
        rpl_rank_t p_rank = dag->instance->of->calculate_rank(p, 0);
        if(probing_target == NULL
            || p_rank < probing_target_rank) {
          probing_target = p;
          probing_target_rank = p_rank;
        }
      }
      p = nbr_table_next(rpl_parents, p);
    }
  }

  /* The default probing target is the least recently updated parent */
  if(probing_target == NULL) {
    p = nbr_table_head(rpl_parents);
    while(p != NULL) {
      if(p->dag == dag) {
        if(probing_target == NULL
            || p->last_tx_time < probing_target->last_tx_time) {
          probing_target = p;
        }
      }
      p = nbr_table_next(rpl_parents, p);
    }
  }

  return probing_target;
}
/*---------------------------------------------------------------------------*/
static void
handle_probing_timer(void *ptr)
{
  rpl_instance_t *instance = (rpl_instance_t *)ptr;
  rpl_parent_t *probing_target = RPL_PROBING_SELECT_FUNC(instance->current_dag);

  /* Perform probing */
  if(probing_target != NULL && rpl_get_parent_ipaddr(probing_target) != NULL) {
    PRINTF("RPL: probing %3u\n",
        nbr_table_get_lladdr(rpl_parents, probing_target)->u8[7]);
    /* Send probe, e.g. unicast DIO or DIS */
    RPL_PROBING_SEND_FUNC(instance, rpl_get_parent_ipaddr(probing_target));
/*
    static uint16_t probe_tx_num;
    printf("r:pr_o|%u|to|%d\n", 
            ++probe_tx_num, LOG_NODEID_FROM_IPADDR(rpl_get_parent_ipaddr(probing_target)));
            */
  }

  /* Schedule next probing */
  rpl_schedule_probing(instance);

#if DEBUG
  rpl_print_neighbor_list();
#endif
}
/*---------------------------------------------------------------------------*/
void
rpl_schedule_probing(rpl_instance_t *instance)
{
  ctimer_set(&instance->probing_timer, RPL_PROBING_DELAY_FUNC(),
                  handle_probing_timer, instance);
}
#endif /* RPL_WITH_PROBING */
/** @}*/
