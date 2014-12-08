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
*################################################################*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
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





#define MY_BUF_SIZE  1000




/*---------------------------------------------------------
  These high-resolution times are used both for
  interim timing reports -- i.e. every 'report_frequency'
  messages -- and for the final timestamp, after all 
  expected messages have been received.
---------------------------------------------------------*/
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





/*----------------------------------------------------
  These absolute timestamps are useful in soak tests,
  where I want to align the program's output with 
  output from top to look at CPU and memory use..
----------------------------------------------------*/
void
print_timestamp_like_a_normal_person ( FILE * fp )
{
  char const * month_abbrevs[] = { "jan", 
                                   "feb", 
                                   "mar", 
                                   "apr", 
                                   "may", 
                                   "jun", 
                                   "jul", 
                                   "aug", 
                                   "sep", 
                                   "oct", 
                                   "nov", 
                                   "dec" 
                                 };
  time_t rawtime;
  struct tm * timeinfo;

  time ( & rawtime );
  timeinfo = localtime ( &rawtime );

  char time_string[100];
  sprintf ( time_string,
            "%d-%s-%02d %02d:%02d:%02d",
            1900 + timeinfo->tm_year,
            month_abbrevs[timeinfo->tm_mon],
            timeinfo->tm_mday,
            timeinfo->tm_hour,
            timeinfo->tm_min,
            timeinfo->tm_sec
          );

  fprintf ( fp, "timestamp %s\n", time_string );
}





int 
main ( int argc, char ** argv  ) 
{
  char info[1000];

  uint64_t received = 0;

  char host [1000];
  char port [1000];
  char output_file_name[1000];

  int initial_flow   = 400;
  int flow_increment = 200;

  int       report_frequency  = 200000;
  int64_t   messages          = 2000000,
            delivery_count    = 0;

  strcpy ( host, "0.0.0.0" );
  strcpy ( port, "5672" );

  pn_driver_t     * driver;
  pn_listener_t   * listener;
  pn_connector_t  * connector;
  pn_connection_t * connection;
  pn_collector_t  * collector;
  pn_transport_t  * transport;
  pn_sasl_t       * sasl;
  pn_session_t    * session;
  pn_event_t      * event;
  pn_link_t       * link;
  pn_delivery_t   * delivery;


  double last_time,
         this_time,
         time_diff;

  char * message_data          = (char *) malloc ( MY_BUF_SIZE );
  int    message_data_capacity = MY_BUF_SIZE;

  FILE * output_fp;


  /*-----------------------------------------------------------
    Read the command lines args and override initialization.
  -----------------------------------------------------------*/
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
    if ( ! strcmp ( "--report_frequency", argv[i] ) )
    {
      report_frequency = atoi ( argv[i+1] );
      ++ i;
    }
    else
    if ( ! strcmp ( "--initial_flow", argv[i] ) )
    {
      sscanf ( argv [ i+1 ], "%d", & initial_flow );
      ++ i;
    }
    else
    if ( ! strcmp ( "--flow_increment", argv[i] ) )
    {
      sscanf ( argv [ i+1 ], "%d", & flow_increment );
      ++ i;
    }
    else
    {
      fprintf ( output_fp, "unknown arg |%s|\n", argv[i] );
      exit ( 1 );
    }
  }

  /*-----------------------------------------------
    Show what we ended up with for all the args.
  -----------------------------------------------*/
  fprintf ( output_fp, "host                %s\n",          host );
  fprintf ( output_fp, "port                %s\n",          port );
  fprintf ( output_fp, "messages            %" PRId64 "\n", messages );
  fprintf ( output_fp, "report_frequency    %d\n",          report_frequency );
  fprintf ( output_fp, "initial_flow        %d\n",          initial_flow );
  fprintf ( output_fp, "flow_increment      %d\n",          flow_increment );
  fprintf ( output_fp, "output              %s\n",          output_file_name );


  /*--------------------------------------------
    Create a standard driver and listen for the 
    initial connector.
  --------------------------------------------*/
  driver = pn_driver ( );

  if ( ! pn_listener(driver, host, port, 0) ) 
  {
    fprintf ( output_fp, "precv listener creation failed.\n" );
    exit ( 1 );
  }

  fprintf ( output_fp, "\nprecv ready...\n\n" );

  while ( 1 )
  {
    pn_driver_wait ( driver, -1 );
    if ( listener = pn_driver_listener(driver) )
    {
      if ( connector = pn_listener_accept(listener) )
        break;
    }
  }

  /*--------------------------------------------------------
    Now make all the other structure around the connector,
    and tell it that skipping sasl is OK.
  --------------------------------------------------------*/
  connection = pn_connection ( );
  collector  = pn_collector  ( );
  pn_connection_collect ( connection, collector );
  pn_connector_set_connection ( connector, connection );

  transport = pn_connector_transport ( connector );
  sasl = pn_sasl ( transport );
  pn_sasl_mechanisms ( sasl, "ANONYMOUS" );
  pn_sasl_server ( sasl );
  pn_sasl_allow_skip ( sasl, true );
  pn_sasl_done ( sasl, PN_SASL_OK );

  /*----------------------------------------------------------
    If report_frequency is not set to zero, we will 
    produce a timing report every report_frequency messages.
    The timing reported will be the delta from the last_time
    to the current time.  
    This is useful in soak testing, where you basically 
    never stop, but still need to see how the system is doing
    every so often.
  ----------------------------------------------------------*/
  last_time = get_time();


  /*------------------------------------------------------------
    A triply-nested loop.
    In the outermost one, we just wait for activity to come in 
    from the driver.
  ------------------------------------------------------------*/
  while ( 1 )
  {
    pn_driver_wait ( driver, -1 );

    /*---------------------------------------------------------------
      In the next loop, we keep going as long as we processed
      some events.  This is because our own processing of events
      may have caused more to be generated that we need to handle.
      If we go back to the outermost loop and its pn_driver_wait()
      without handling these events, we will end up with the sender
      and receiver programs just staring at each other with blank
      expressions on their faces.
    ---------------------------------------------------------------*/
    int event_count = 1;
    while ( event_count > 0 )
    {
      event_count = 0;
      pn_connector_process ( connector );

      /*-------------------------------------------------------
        After we process the connector, it may have generated
        a batch of events for us to handle.  As we go through 
        this batch of events, our handling may generate other 
        events which we must handle before going back to 
        pn_driver_wait().
      --------------------------------------------------------*/
      event = pn_collector_peek(collector);
      while ( event )
      {
        ++ event_count;
        pn_event_type_t event_type = pn_event_type ( event );
        //fprintf ( output_fp, "precv event: %s\n", pn_event_type_name(event_type));


        switch ( event_type )
        {
          case PN_CONNECTION_REMOTE_OPEN:
          break;


          case PN_SESSION_REMOTE_OPEN:
            session = pn_event_session(event);
            if ( pn_session_state(session) & PN_LOCAL_UNINIT ) 
            {
              // big number because it's in bytes.
              pn_session_set_incoming_capacity ( session, 1000000 );
              pn_session_open ( session );
            }
          break;


          case PN_LINK_REMOTE_OPEN:
            /*----------------------------------------------------
              When we first open the link, we give it an initial 
              amount of credit in units of messages.
              We will later increment its credit whenever credit
              falls below some threshold.
            ----------------------------------------------------*/
            link = pn_event_link(event);
            if (pn_link_state(link) & PN_LOCAL_UNINIT )
            {
              pn_link_open ( link );
              pn_link_flow ( link, initial_flow );
            }
          break;


          case PN_CONNECTION_BOUND:
            if ( pn_connection_state(connection) & PN_LOCAL_UNINIT )
            {
              pn_connection_open ( connection );
            }
          break;


          // And now the event that you've all been waiting for.....

          case PN_DELIVERY:
            link = pn_event_link ( event );
            delivery = pn_event_delivery ( event );

            /*------------------------------------------------
              Since I want this program to be a receiver,
              I am not interested in deliver-related events 
              unless they are incoming, 'readable' events.
            ------------------------------------------------*/
            if ( pn_delivery_readable ( delivery ) )
            {
              // If the delivery is partial I am just going to ignore
              // it until it becomes complete.
              if ( ! pn_delivery_partial ( delivery ) )
              {
                ++ delivery_count; 
                /*
                if ( ! (delivery_count % report_frequency) )
                {
                  pn_link_t * delivery_link = pn_delivery_link ( delivery );
                  int received_bytes = pn_delivery_pending ( delivery );
                  pn_link_recv ( delivery_link, incoming, 1000 );
                  fprintf ( output_fp, "received bytes: %d\n", received_bytes );
                }
                */

                // don't bother updating.  they're pre-settled.
                // pn_delivery_update ( delivery, PN_ACCEPTED );
                pn_delivery_settle ( delivery );

                int credit = pn_link_credit ( link );

                if ( delivery_count >= messages )
                {
                  fprintf ( output_fp, "precv_stop %.3lf\n", get_time() );
                  goto all_done;
                }

                // Make a report frequency of zero shut down interim reporting.
                if ( report_frequency
                     &&
                     (! (delivery_count % report_frequency))
                   )
                {
                  this_time = get_time();
                  time_diff = this_time - last_time;

                  print_timestamp_like_a_normal_person ( output_fp );
                  fprintf ( output_fp, 
                            "deliveries: %" PRIu64 "  time: %.3lf\n", 
                            delivery_count,
                            time_diff
                          );
                  fflush ( output_fp );
                  last_time = this_time;
                }

                /*----------------------------------------------------------
                  I replenish the credit whevere it falls below this limit
                  because I this psend / precv pair of programs to run as
                  fast as possible.  A real application might want to do
                  something fancier here.
                ----------------------------------------------------------*/
                if ( credit <= flow_increment )
                {
                  pn_link_flow ( link, flow_increment );
                }
              }
            }
          break;


          case PN_TRANSPORT:
            // not sure why I would care...
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
  pn_session_close ( session );
  pn_connection_close ( connection );
  fclose ( output_fp );
  return 0;
}





