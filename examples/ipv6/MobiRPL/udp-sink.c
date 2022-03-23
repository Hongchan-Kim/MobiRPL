/*
 * Copyright (c) 2014, Swedish Institute of Computer Science.
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
 */
/**
 * \file
 *         Example file using RPL for a data collection.
 *         Can be deployed in the Indriya or Twist testbeds.
 *
 * \author Simon Duquennoy <simonduq@sics.se>
 */

#include "contiki-conf.h"
#include "net/netstack.h"
#include "net/rpl/rpl-private.h"
#include "net/ip/uip-udp-packet.h"
#include "net/ip/uip-debug.h"
#include "lib/random.h"
#include <stdio.h>

#define START_DELAY    (CLOCK_SECOND * CONF_START_DELAY)
#define SEND_INTERVAL   (CLOCK_SECOND * CONF_SEND_INTERVAL)

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#define UDP_CLIENT_PORT 8775
#define UDP_SERVER_PORT 5688

static struct uip_udp_conn *server_conn;
static uip_ipaddr_t client_ipaddr;

struct app_data {
  uint32_t magic;
  uint32_t seqno;
  uint16_t src;
  uint16_t dest;
  uint8_t hop;
  uint8_t ping;
  uint16_t dummy_for_padding;
};

static struct ctimer down_send_timer;
static uint16_t receiver_id;
static unsigned int cnt = 1;
static uint32_t seqno;

static uint16_t app_tx_num[MAX_NODES];
static uint16_t rcvd[MAX_NODES];
static uint16_t last_seq[MAX_NODES];
static uint32_t last_tx, last_rx, last_time;
static uint32_t delta_tx, delta_rx, delta_time;
static uint32_t curr_tx, curr_rx, curr_time;

/*---------------------------------------------------------------------------*/
PROCESS(udp_sink_process, "UDP Sink Application");
AUTOSTART_PROCESSES(&udp_sink_process);
/*---------------------------------------------------------------------------*/
void
simple_energest_init()
{
  energest_flush();
  last_tx = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  last_rx = energest_type_time(ENERGEST_TYPE_LISTEN);
  last_time = energest_type_time(ENERGEST_TYPE_CPU) + energest_type_time(ENERGEST_TYPE_LPM);
}
/*---------------------------------------------------------------------------*/
void
simple_energest_step(int verbose)
{
  static uint16_t energest_cnt;
  energest_flush();

  curr_tx = energest_type_time(ENERGEST_TYPE_TRANSMIT);
  curr_rx = energest_type_time(ENERGEST_TYPE_LISTEN);
  curr_time = energest_type_time(ENERGEST_TYPE_CPU) + energest_type_time(ENERGEST_TYPE_LPM);

  delta_tx = curr_tx - last_tx;
  delta_rx = curr_rx - last_rx;
  delta_time = curr_time - last_time;

  last_tx = curr_tx;
  last_rx = curr_rx;
  last_time = curr_time;

  if(verbose) {
    uint32_t fraction = (1000ul * (delta_tx + delta_rx)) / delta_time;
    uint32_t all_fraction = (1000ul * (curr_tx + curr_rx)) / curr_time;
    printf("dc:[%u %u]|%8lu|+|%8lu|/|%8lu|(%lu|permil)|%lu\n",
        node_id,
        energest_cnt++,
        delta_tx, delta_rx, delta_time,
        fraction,
        all_fraction
        );
  }
}
/*---------------------------------------------------------------------------*/
/* Copy an appdata to another with no assumption that the addresses are aligned */
void
appdata_copy(void *dst, void *src)
{
  if(dst != NULL) {
    if(src != NULL) {
      memcpy(dst, src, sizeof(struct app_data));
    } else {
      memset(dst, 0, sizeof(struct app_data));
    }   
  }
}
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  struct app_data ad;
  appdata_copy(&ad, (struct app_data *)uip_appdata);

  uint8_t index = UIP_HTONS(ad.src) - 1;
  uint8_t hops = uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl + 1;

  uint16_t current_seq = (uint16_t)((uint32_t)UIP_HTONL(ad.seqno) - ((uint32_t)(index + 1) << 16));

  if(current_seq <= last_seq[index]) {
    printf("a:d|f|%u|%u|s|%lx|%u|", index + 1, rcvd[index], (unsigned long)UIP_HTONL(ad.seqno), last_seq[index]);
    printf("h|%u\n", hops);
    return;
  }
  last_seq[index] = current_seq;

  rcvd[index]++;
  printf("a:rxu|f|%u|%u|s|%lx|", index + 1, rcvd[index], (unsigned long)UIP_HTONL(ad.seqno));
  printf("h|%u\n", hops);
}
/*---------------------------------------------------------------------------*/
void
app_send(void *ptr)
{
  /* hckim added */
#if TESTBED_01
  if(receiver_id != SINGLE_SENDER_ID)
    goto pass;
#elif TESTBED_10
  if(receiver_id % 3 != 2)
    goto pass;
#elif TESTBED_20
  if(receiver_id % 3 == 1)
    goto pass;
#endif

  uip_ip6addr(&client_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  client_ipaddr.u8[8] = 2;
  client_ipaddr.u8[15] = receiver_id;
  seqno = ((uint32_t)receiver_id << 16) + cnt;

  struct app_data data;

  data.magic = UIP_HTONL(LOG_MAGIC);
  data.seqno = UIP_HTONL(seqno);
  data.src = UIP_HTONS(node_id);
  data.dest = UIP_HTONS(receiver_id);
  data.hop = 0;

  rpl_dag_t *dag = rpl_get_any_dag();

  uint8_t index = receiver_id - 1;

  printf("a:txd|%u|t|%u|s|%lx|h|%u\n", ++app_tx_num[index], receiver_id, 
    (unsigned long)UIP_HTONL(data.seqno),
    DAG_RANK(dag->preferred_parent->rank, dag->instance));

  uip_udp_packet_sendto(server_conn, &data, sizeof(data),
          &client_ipaddr, UIP_HTONS(UDP_CLIENT_PORT));

pass:
  receiver_id++;
  if(receiver_id > MAX_NODES) {
    ctimer_stop(&down_send_timer);
  } else {
    ctimer_reset(&down_send_timer);
  }
}
/*---------------------------------------------------------------------------*/
void
down_send(void)
{
  receiver_id = 2;
  ctimer_set(&down_send_timer, SEND_INTERVAL / (MAX_NODES), app_send, NULL);
}
/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  PRINTF("Server IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(state == ADDR_TENTATIVE || state == ADDR_PREFERRED) {
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n");
      /* hack to make address "final" */
      if (state == ADDR_TENTATIVE) {
        uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_sink_process, ev, data)
{
  uip_ipaddr_t ipaddr;
  struct uip_ds6_addr *root_if;

  static struct etimer start_timer;
  static struct etimer periodic_timer;

  PROCESS_BEGIN();

  simple_energest_init();

  PROCESS_PAUSE();

#if UIP_CONF_ROUTER
  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, ROOT_ID);
  /* uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr); */
  uip_ds6_addr_add(&ipaddr, 0, ADDR_MANUAL);
  root_if = uip_ds6_addr_lookup(&ipaddr);
  if(root_if != NULL) {
    rpl_dag_t *dag;
    dag = rpl_set_root(RPL_DEFAULT_INSTANCE, (uip_ip6addr_t *)&ipaddr);
    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    rpl_set_prefix(dag, &ipaddr, 64);
    PRINTF("created a new RPL dag\n");
  } else {
    PRINTF("failed to create a new RPL DAG\n");
  }

  uip_ip6addr(&client_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
#endif /* UIP_CONF_ROUTER */

  print_local_addresses();

  NETSTACK_RDC.off(1);

#if MOBIRPL_RH_OF
  printf("a:rhof|%d\n", RSSI_LOW_THRESHOLD);
#else
  printf("a:mrhof\n");
#endif

  server_conn = udp_new(NULL, UIP_HTONS(UDP_CLIENT_PORT), NULL);
  if(server_conn == NULL) {
    PRINTF("No UDP connection available, exiting the process!\n");
    PROCESS_EXIT();
  }
  udp_bind(server_conn, UIP_HTONS(UDP_SERVER_PORT));

#if DOWNWARD_TRAFFIC
  etimer_set(&start_timer, START_DELAY);
#endif
  
  while(1) {
    PROCESS_YIELD();
    if(ev == tcpip_event) {
      tcpip_handler();
      simple_energest_step(!(default_instance == NULL));
    }

#if DOWNWARD_TRAFFIC
    else if(ev == PROCESS_EVENT_TIMER) {
      if(data == &start_timer) {
        etimer_set(&periodic_timer, SEND_INTERVAL);
        if(default_instance != NULL) {
          down_send();
        } else {
          //printf("a:n_D\n");
        }
        simple_energest_step(!(default_instance == NULL));

      } else if(data == &periodic_timer) {
        cnt++;
        if(cnt <= APP_MAX_SEQNO) {
          if(default_instance != NULL) {
            down_send();
          } else {
            //printf("a:n_D\n");
          }
          if(cnt == APP_MAX_SEQNO) {
            printf("a:e\n");
          }
        }

        etimer_reset(&periodic_timer);
        simple_energest_step(!(default_instance == NULL));

/*
        if(cnt > APP_MAX_SEQNO) {
          printf("a:end\n");
          break;
        }
        if(default_instance != NULL) {
          down_send();
        } else {
          printf("a:n_D\n");
        }
        simple_energest_step(!(default_instance == NULL));
*/
      }
    }
#endif

  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
