/*
 * Copyright (c) 2011, Swedish Institute of Computer Science.
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
 *
 */

#include "contiki.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/rime/rime.h"


#include "simple-udp.h"
#include "dev/button-sensor.h"
#include "dev/light-sensor.h"
#include "dev/sht11/sht11-sensor.h"
#include "dev/leds.h"

#include <stdio.h>
#include <string.h>

#define UDP_PORT 1234

#define SEND_INTERVAL        (2 * CLOCK_SECOND)
#define SEND_TIME        (random_rand() % (SEND_INTERVAL))

static process_event_t event_arlam;
static process_event_t event_broadcast;
static process_event_t event_button;

static struct broadcast_conn broadcast;

/*---------------------------------------------------------------------------*/
PROCESS(light_sensor_montitor_process, "light_sensor_montitor_process");
PROCESS(reset_button_monitor_process, "Mreset_button_monitor_process");
PROCESS(actuate_arlam_process, "actuate_arlam_process");
PROCESS(broadcast_intrusion_process, "broadcast_intrusion_process");
//AUTOSTART_PROCESSES(&light_sensor_montitor_process);
AUTOSTART_PROCESSES(&light_sensor_montitor_process, &reset_button_monitor_process, &actuate_arlam_process, &broadcast_intrusion_process);
/*---------------------------------------------------------------------------*/
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  printf("broadcast message received from %d.%d: '%s'\n",
         from->u8[0], from->u8[1], (char *)packetbuf_dataptr());

    char* data = (char *)packetbuf_dataptr()  ;  
    static char light_data[4];
    static char arlam_data[4];
    static char sys_time[5];
    int arlam_flag;
    static int ID_queue[10];
    static int qHead = 0;
    int i;
    int flag_rebroadcast = 0;
    
    strncpy(sys_time, data, 4);
    strncpy(light_data, data+5, 3);
    strncpy(arlam_data, data+9, 3);
    sys_time[4] = '\0';
    light_data[3] = '\0';
    arlam_data[3] = '\0';

    printf("Received System Time message with: %s\n", sys_time);
    printf("Received light_data message with: %s\n", light_data);
    printf("Received arlam_data message with: %s\n", arlam_data);
    
    //Perform process of check if received message is not in history queue
    // if not then store then involk rebroadcast else do nothing
    int ID = atoi(sys_time);
    for(i=0; i<10; i++) {
        if(ID_queue[i] == ID) {
            flag_rebroadcast = 1;
            return;
        }
    }
    
    //store message
    if (flag_rebroadcast == 0) {
        ID_queue[qHead] = ID;
        qHead++;
        if(qHead > 9) qHead = 0;
    } 
    process_post(&actuate_arlam_process, event_arlam, &arlam_data);
    
    arlam_flag = atoi(arlam_data);
    //printf("Received arlam_flag with value: %d\n", arlam_flag);
    
    if(arlam_flag == 112) {
      leds_on(LEDS_RED);
    } else if(arlam_flag == 110) {
       leds_off(LEDS_RED);
   }
    
    process_post(&broadcast_intrusion_process, event_broadcast, data);
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(light_sensor_montitor_process, ev, data)
{
  static struct etimer periodic_timer;
  static struct etimer send_timer;
  uip_ipaddr_t addr;
  static char message[12];
  int arlam_flag;
  static char arlam_data[4];
  
  
  
  PROCESS_BEGIN();
  SENSORS_ACTIVATE(button_sensor);
    SENSORS_ACTIVATE(light_sensor);
    SENSORS_ACTIVATE(sht11_sensor);


  etimer_set(&periodic_timer, SEND_INTERVAL);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    etimer_reset(&periodic_timer);
    etimer_set(&send_timer, SEND_TIME);

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&send_timer));
    
    int send_data = light_sensor.value(0);
    //int send_data = sht11_sensor.value(SHT11_SENSOR_TEMP);
    //printf("Sending message with light_sensor value: %d\n", send_data);
    int sys_time = clock_time();
    sys_time = sys_time%10000;
    //printf("Sending message with sys_timer value: %d\n", sys_time);
    if(send_data > 150) 
    {

      //int send_data = sht11_sensor.value(SHT11_SENSOR_TEMP);
      //printf("Sending raw  data: %d\n", send_data);
      //sprintf(message,"hello");
      arlam_flag = 112;
      sprintf(arlam_data, "%d", arlam_flag);
      process_post(&actuate_arlam_process, event_arlam, &arlam_data);
      
      sprintf(message, "%04d.%03d.%03d", sys_time, send_data, arlam_flag);
      //printf("Sending message with data: %s\n", message);

      process_post(&broadcast_intrusion_process, event_broadcast, &message);
      
      
      
    
        }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(reset_button_monitor_process, ev, data)
{
  static int arlam_flag;
  static char arlam_data[4];
  static char message[12];
  PROCESS_BEGIN();
  SENSORS_ACTIVATE(button_sensor);
  while(1) {
   //PROCESS_WAIT_EVENT_UNTIL(ev == event_button);
   PROCESS_WAIT_EVENT_UNTIL(data == &button_sensor);
   
   printf("Reset button detected\n");
   
   //leds_off(LEDS_RED);
   arlam_flag = 110;
   sprintf(arlam_data, "%d", arlam_flag);
   //Rebroadcast message if received. we should not rebroadcast a message two times. 
   //message ID is used
   process_post(&actuate_arlam_process, event_arlam, &arlam_data);
   
   int sys_time = clock_time();
   sys_time = sys_time%10000;
   sprintf(message, "%04d.000.%03d",sys_time , arlam_flag);
   printf("Sending message with data: %s\n", message);

   //process_post(&broadcast_intrusion_process, event_broadcast, &message);
   
   //debug broadcast
   //PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
   broadcast_open(&broadcast, 129, &broadcast_call);
   packetbuf_clear();
   packetbuf_copyfrom(message, 12);
   broadcast_send(&broadcast);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(actuate_arlam_process, ev, data)
{
  int arlam_flag;
  char arlam_data[4];
  PROCESS_BEGIN();

  while(1) {
   PROCESS_WAIT_EVENT_UNTIL(ev == event_arlam);
   
   strncpy(arlam_data, data, 4);
   //printf("Arlam Event wake up with raw data: %s\n", arlam_data);
   arlam_flag = atoi(arlam_data);
   //printf("Arlam Event wake up with arlam flag: %d\n", arlam_flag);
   
   if(arlam_flag == 112) {
    leds_on(LEDS_RED);
   } else if(arlam_flag == 110) {
    leds_off(LEDS_RED);
   }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_intrusion_process, ev, data)
{
  
  static struct etimer periodic_timer;
  static struct etimer send_timer;
  uip_ipaddr_t addr;
  char message[7];
  
  
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();


    broadcast_open(&broadcast, 129, &broadcast_call);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == event_broadcast);
    printf("Broadcast Event wake up with raw data: %s\n", data);
    strncpy(message, data, 12);
    //message[3] = '\0';
    printf("Broadcast Event wake up with message: %s\n", message);
    
    packetbuf_copyfrom(message, 12);
    broadcast_send(&broadcast);
    

  }

  PROCESS_END();
}

