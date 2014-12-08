/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */


/*################################################################
  This program is half of a pair.  Precv and Psend are meant to 
  be simple-as-possible examples of how to use the proton-c
  engine interface to send and receive messages over a single 
  connection and a single session.

  In addition to being examples, these programs or their 
  descendants will be used in performance regression testing
  for both throughput and latency, and long-term soak testing.

  This program, psend, is highly similar to its peer precv.
  I put all the good comments in precv.
*################################################################*/


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>


#include <proton/connection.h>
#include <proton/delivery.h>
#include <proton/driver.h>
#include <proton/event.h>
#include <proton/terminus.h>
#include <proton/link.h>
#include <proton/message.h>
#include <proton/session.h>


#define MY_BUF_SIZE 1000



void
print_timestamp ( FILE * fp, char const * label )
{
  struct timeval tv;
  struct tm      * timeinfo;

  gettimeofday ( & tv, 0 );
  timeinfo = localtime ( & tv.tv_sec );

  int seconds_today = 3600 * timeinfo->tm_hour +
                        60 * timeinfo->tm_min  +
                             timeinfo->tm_sec;

  fprintf ( fp, "time : %d.%.6ld : %s\n", seconds_today, tv.tv_usec, label );
}





static 
double
get_time ( )
{
  struct timeval tv;
  struct tm      * timeinfo;

  gettimeofday ( & tv, 0 );
  timeinfo = localtime ( & tv.tv_sec );

  double time_now = 3600 * timeinfo->tm_hour +
                      60 * timeinfo->tm_min  +
                           timeinfo->tm_sec;

  time_now += ((double)(tv.tv_usec) / 1000000.0);
  return time_now;
}





int 
main ( int argc, char ** argv )
{
  char addr [ 1000 ];
  char host [ 1000 ];
  char port [ 1000 ];
  char output_file_name[1000];


  uint64_t messages       = 2000000,
           delivery_count = 0;

  int message_length = 100;

  bool done           = false;
  int  sent_count     = 0;
  int  n_links        = 5;
  int const max_links = 100;

  strcpy ( addr, "queue" );
  strcpy ( host, "0.0.0.0" );
  strcpy ( port, "5672" );

  FILE * output_fp;


  for ( int i = 1; i < argc; ++ i )
  {
    if ( ! strcmp ( "--host", argv[i] ) )
    {
      strcpy ( host, argv[i+1] );
      ++ i;
    }
    else
    if ( ! strcmp ( "--port", argv[i] ) )
    {
      strcpy ( port, argv[i+1] );
      ++ i;
    }
    else
    if ( ! strcmp ( "--messages", argv[i] ) )
    {
      sscanf ( argv [ i+1 ], "%" SCNd64 , & messages );
      ++ i;
    }
    else
    if ( ! strcmp ( "--message_length", argv[i] ) )
    {
      sscanf ( argv [ i+1 ], "%d", & message_length );
      ++ i;
    }
    else
    if ( ! strcmp ( "--n_links", argv[i] ) )
    {
      sscanf ( argv [ i+1 ], "%d", & n_links );
      ++ i;
    }
    if ( ! strcmp ( "--output", argv[i] ) )
    {
      if ( ! strcmp ( "stderr", argv[i+1] ) )
      {
        output_fp = stderr;
        strcpy ( output_file_name, "stderr");
      }
      else
      if ( ! strcmp ( "stdout", argv[i+1] ) )
      {
        output_fp = stdout;
        strcpy ( output_file_name, "stdout");
      }
      else
      {
        output_fp = fopen ( argv[i+1], "w" );
        strcpy ( output_file_name, argv[i+1] );
        if ( ! output_fp )
        {
          fprintf ( stderr, "Can't open |%s| for writing.\n", argv[i+1] );
          exit ( 1 );
        }
      }
      ++ i;
    }
    else
    {
      fprintf ( output_fp, "unknown arg %s", argv[i] );
    }
  }


  fprintf ( output_fp, "host            %s\n",          host );
  fprintf ( output_fp, "port            %s\n",          port );
  fprintf ( output_fp, "messages        %" PRId64 "\n", messages );
  fprintf ( output_fp, "message_length  %d\n",          message_length );
  fprintf ( output_fp, "n_links         %d\n",          n_links );
  fprintf ( output_fp, "output          %s\n",          output_file_name );


  if ( n_links > max_links )
  {
    fprintf ( output_fp, "You can't have more than %d links.\n", max_links );
    exit ( 1 );
  }

  pn_driver_t     * driver;
  pn_connector_t  * connector;
  pn_connector_t  * driver_connector;
  pn_connection_t * connection;
  pn_collector_t  * collector;
  pn_link_t       * links [ max_links ];
  pn_session_t    * session;
  pn_event_t      * event;
  pn_delivery_t   * delivery;


  char * message = (char *) malloc(message_length);
  memset ( message, 13, message_length );

  /*----------------------------------------------------
    Get everything set up.
    We will have a single connector, a single 
    connection, a single session, and a single link.
  ----------------------------------------------------*/
  driver = pn_driver ( );
  connector = pn_connector ( driver, host, port, 0 );

  connection = pn_connection();
  collector  = pn_collector  ( );
  pn_connection_collect ( connection, collector );
  pn_connector_set_connection ( connector, connection );

  session = pn_session ( connection );
  pn_connection_open ( connection );
  pn_session_open ( session );

  for ( int i = 0; i < n_links; ++ i )
  {
    char name[100];
    sprintf ( name, "tvc_15_%d", i );
    links[i] = pn_sender ( session, name );
    pn_terminus_set_address ( pn_link_target(links[i]), addr );
    pn_link_open ( links[i] );
  }

  /*-----------------------------------------------------------
    For my speed tests, I do not want to count setup time.
    Start timing here.  The receiver will print out a similar
    timestamp when he receives the final message.
  -----------------------------------------------------------*/
  fprintf ( output_fp, "psend: sending %llu messages.\n", messages );

  // Just before we start sending, print the start timestamp.
  fprintf ( output_fp, "psend_start %.3lf\n", get_time() );

  while ( 1 )
  {
    pn_driver_wait ( driver, -1 );

    int event_count = 1;
    while ( event_count > 0 )
    {
      event_count = 0;
      pn_connector_process ( connector );

      event = pn_collector_peek(collector);
      while ( event )
      {
        ++ event_count;
        pn_event_type_t event_type = pn_event_type ( event );
        //fprintf ( output_fp, "event: %s\n", pn_event_type_name ( event_type ) );

        switch ( event_type )
        {
          case PN_LINK_FLOW:
          {
            if ( delivery_count < messages )
            {
              /*---------------------------------------------------
                We may have opened multiple links.
                The event will tell us which one this flow-event
                happened on.  If the flow event gave us some 
                credit, we will greedily send messages until it
                is all used up.
              ---------------------------------------------------*/
              pn_link_t * link = pn_event_link ( event );
              int credit = pn_link_credit ( link );

              while ( credit > 0 )
              {
                // Every delivery we create needs a unique tag.
                char str [ 100 ];
                sprintf ( str, "%x", delivery_count ++ );
                delivery = pn_delivery ( link, pn_dtag(str, strlen(str)) );

                // If you settle the delivery before sending it,
                // you will spend some time wondering why your 
                // messages don't have any content when they arrive 
                // at the receiver.
                pn_link_send ( link, message, message_length );
                pn_delivery_settle ( delivery );
                pn_link_advance ( link );
                credit = pn_link_credit ( link );
              }
              
              if ( delivery_count >= messages )
              {
                fprintf ( output_fp, 
                          "I have sent all %d messages.\n" ,
                          delivery_count
                        );
                /*
                  I'm still kind of hazy on how to shut down the
                  psend / precv system properly ....
                  I can't go to all_done here, or precv will never
                  get all its messages and terminate.
                  So I let precv terminate properly ... which means
                  that this program, psend, dies with an error.
                  Hmm.
                */
                // goto all_done;
              }
            }
          }
          break;


          case PN_TRANSPORT:
            // I don't know what this means here, either.
          break;


          case PN_TRANSPORT_TAIL_CLOSED:
            goto all_done;
          break;


          default:
            /*
            fprintf ( output_fp,
                      "precv unhandled event: %s\n",
                      pn_event_type_name(event_type)
                    );
            */
          break;

        }

        pn_collector_pop ( collector );
        event = pn_collector_peek(collector);
      }
    }
  }

  all_done:

  for ( int i = 0; i < n_links; ++ i )
  {
    pn_link_close ( links[i] );
  }

  pn_session_close ( session );
  pn_connection_close ( connection );

  return 0;
}





