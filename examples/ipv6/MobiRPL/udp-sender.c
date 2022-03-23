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

static struct uip_udp_conn *client_conn;
static uip_ipaddr_t server_ipaddr;

struct app_data {
  uint32_t magic;
  uint32_t seqno;
  uint16_t src;
  uint16_t dest;
  uint8_t hop;
  uint8_t ping;
  uint16_t dummy_for_padding;
};

static uint16_t app_tx_num;
static uint16_t rcvd;
static uint16_t last_seq;
static uint32_t last_tx, last_rx, last_time;
static uint32_t delta_tx, delta_rx, delta_time;
static uint32_t curr_tx, curr_rx, curr_time;

/*---------------------------------------------------------------------------*/
PROCESS(udp_sender_process, "UDP Sender Application");
AUTOSTART_PROCESSES(&udp_sender_process);
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

  if(current_seq <= last_seq) {
    printf("a:d|f|%u|%u|s|%lx|%u|", index + 1, rcvd, (unsigned long)UIP_HTONL(ad.seqno), last_seq);
    printf("h|%u\n", hops);
    return;
  }
  last_seq = current_seq;

  rcvd++;
  printf("a:rxd|f|%u|%u|s|%lx|", index + 1, rcvd, (unsigned long)UIP_HTONL(ad.seqno));
  printf("h|%u\n", hops);
}
/*---------------------------------------------------------------------------*/
int
app_send_to(uint16_t id, uint32_t seqno)
{
  /* hckim added */
#if TESTBED_01
  if(node_id != SINGLE_SENDER_ID)
    return 1;
#elif TESTBED_10
  if(node_id % 3 != 2)
    return 1;
#elif TESTBED_20
  if(node_id % 3 == 1)
    return 1;
#endif

  struct app_data data;

  data.magic = UIP_HTONL(LOG_MAGIC);
  data.seqno = UIP_HTONL(seqno);
  data.src = UIP_HTONS(node_id);
  data.dest = UIP_HTONS(id);
  data.hop = 0;

  rpl_dag_t *dag = rpl_get_any_dag();

  printf("a:txu|%u|t|%u|s|%lx|h|%u\n", ++app_tx_num, id, 
    (unsigned long)UIP_HTONL(data.seqno),
    DAG_RANK(dag->preferred_parent->rank, dag->instance));

  uip_udp_packet_sendto(client_conn, &data, sizeof(data),
          &server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));

  return 1;
}
/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  PRINTF("Client IPv6 addresses: ");
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
PROCESS_THREAD(udp_sender_process, ev, data)
{
  uip_ipaddr_t ipaddr;

  static struct etimer start_timer;
  static struct etimer periodic_timer;
  static struct etimer send_timer;

  static unsigned int cnt = 1;
  static uint32_t seqno;

  PROCESS_BEGIN();

  simple_energest_init();

  PROCESS_PAUSE();

#if UIP_CONF_ROUTER
  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  uip_ip6addr(&server_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, ROOT_ID);
  server_ipaddr.u8[8] = 2;

#endif /* UIP_CONF_ROUTER */

  print_local_addresses();

#if ALWAYS_ON_RDC
  NETSTACK_RDC.off(1);
#endif

#if MOBIRPL_RH_OF
  printf("a:rhof|%d\n", RSSI_LOW_THRESHOLD);
#else
  printf("a:mrhof\n");
#endif

  client_conn = udp_new(NULL, UIP_HTONS(UDP_SERVER_PORT), NULL);
  if(client_conn == NULL) {
    PRINTF("No UDP connection available, exiting the process!\n");
    PROCESS_EXIT();
  }
  udp_bind(client_conn, UIP_HTONS(UDP_CLIENT_PORT));

#if UPWARD_TRAFFIC
  etimer_set(&start_timer, START_DELAY);
#endif

  while(1) {
    PROCESS_YIELD();
    if(ev == tcpip_event) {
      tcpip_handler();
      simple_energest_step(!(default_instance == NULL));
    }

#if UPWARD_TRAFFIC
    else if(ev == PROCESS_EVENT_TIMER) {
      if(data == &start_timer) {
        etimer_set(&send_timer, random_rand() % (SEND_INTERVAL));
        etimer_set(&periodic_timer, SEND_INTERVAL);
        simple_energest_step(!(default_instance == NULL));

      } else if(data == &periodic_timer) {
        etimer_set(&send_timer, random_rand() % (SEND_INTERVAL));
        etimer_reset(&periodic_timer);
        simple_energest_step(!(default_instance == NULL));

      } else if(data == &send_timer) {
        if(cnt <= APP_MAX_SEQNO) {
          if(default_instance != NULL) {
            seqno = ((uint32_t)node_id << 16) + cnt;
            app_send_to(ROOT_ID, seqno);
            cnt++;
          } else {
            //printf("a:n_D\n");
          }
          if(cnt > APP_MAX_SEQNO) {
            printf("a:e\n");
          }
        }
/*
        if(cnt > APP_MAX_SEQNO) {
          printf("a:end\n");
          break;
        }
        if(default_instance != NULL) {
          seqno = ((uint32_t)node_id << 16) + cnt;
          app_send_to(ROOT_ID, seqno);
          cnt++;
        } else {
          printf("a:n_D\n");
        }
*/
      }
    }
#endif

  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
