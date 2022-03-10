/*
 *  chiTCP - A simple, testable TCP stack
 *
 *  Implementation of the TCP protocol.
 *
 *  chiTCP follows a state machine approach to implementing TCP.
 *  This means that there is a handler function for each of
 *  the TCP states (CLOSED, LISTEN, SYN_RCVD, etc.). If an
 *  event (e.g., a packet arrives) while the connection is
 *  in a specific state (e.g., ESTABLISHED), then the handler
 *  function for that state is called, along with information
 *  about the event that just happened.
 *
 *  Each handler function has the following prototype:
 *
 *  int f(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event);
 *
 *  si is a pointer to the chiTCP server info. The functions in
 *       this file will not have to access the data in the server info,
 *       but this pointer is needed to call other functions.
 *
 *  entry is a pointer to the socket entry for the connection that
 *          is being handled. The socket entry contains the actual TCP
 *          data (variables, buffers, etc.), which can be extracted
 *          like this:
 *
 *            tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;
 *
 *          Other than that, no other fields in "entry" should be read
 *          or modified.
 *
 *  event is the event that has caused the TCP thread to wake up. The
 *          list of possible events corresponds roughly to the ones
 *          specified in http://tools.ietf.org/html/rfc793#section-3.9.
 *          They are:
 *
 *            APPLICATION_CONNECT: Application has called socket_connect()
 *            and a three-way handshake must be initiated.
 *
 *            APPLICATION_SEND: Application has called socket_send() and
 *            there is unsent data in the send buffer.
 *
 *            APPLICATION_RECEIVE: Application has called socket_recv() and
 *            any received-and-acked data in the recv buffer will be
 *            collected by the application (up to the maximum specified
 *            when calling socket_recv).
 *
 *            APPLICATION_CLOSE: Application has called socket_close() and
 *            a connection tear-down should be initiated.
 *
 *            PACKET_ARRIVAL: A packet has arrived through the network and
 *            needs to be processed (RFC 793 calls this "SEGMENT ARRIVES")
 *
 *            TIMEOUT: A timeout (e.g., a retransmission timeout) has
 *            happened.
 *
 */

/*
 *  Copyright (c) 2013-2014, The University of Chicago
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or withsend
 *  modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  - Neither the name of The University of Chicago nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software withsend specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY send OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "chitcp/log.h"
#include "chitcp/utils.h"
#include "chitcp/buffer.h"
#include "chitcp/chitcpd.h"
#include "serverinfo.h"
#include "connection.h"
#include "tcp.h"
#include <stdlib.h>
#include <string.h>

/*
 * clock granularity of 50 milliseconds.
 * Furthermore, while the RFC requires that
 * the RTO always be at least one second,
 * we will instead use a minimum RTO of 200
 * milliseconds. A maximum value may be placed
 * on RTO provided it is at least 60 seconds.
 */
#define CLOCK_GRANULARITY (50 * MILLISECOND)
#define MIN_RTO (200 * MILLISECOND)
#define MAX_RTO (60 * SECOND)

/* Data structure and helper functions for retransmission */
/* callback args */
typedef struct callback_args
{
    /* Server Information */
    serverinfo_t *si;
    /* Chisocket entry */
    chisocketentry_t *entry;
    /* timer_type: RETRANSMISSION or PERSIST */
    tcp_timer_type_t timer_type;

} callback_args_t;

/*
 * A helper function to generate random ISS
 *
 * Returns: uint32_t random number
 */
uint32_t random_number()
{
    return rand() & 0xff;
}

/*
 * A helper function to free packet
 *
 * packet: the tcp_packet to be freed
 *
 * Returns: nothing
 */
void free_packet(tcp_packet_t *packet)
{
    chitcp_tcp_packet_free(packet);
    free(packet);
    return;
}

/*
 * A comparator function to compare SEG
 *
 * a, b: our_of_order_list_t struct to be compared
 *
 * Returns: the difference of a,b
 */
int segmentcmp(out_of_order_list_t *a, out_of_order_list_t *b)
{
    return a->seq - b->seq;
}

/*
 * Retransmission callback function
 *
 * mt: multi_timer
 *
 * timer: single timer
 *
 * callback_args: callback arguments used in callback functions
 *
 * Returns: nothing
 */
void rtx_callback_fn(multi_timer_t *mt, single_timer_t *timer, void *callback_args)
{
    callback_args_t *curr_callback_args = (callback_args_t *)callback_args;
    chitcpd_timeout(curr_callback_args->si, curr_callback_args->entry, RETRANSMISSION);
}

/*
 * Persist callback function
 *
 * mt: multi_timer
 *
 * timer: single timer
 *
 * callback_args: callback arguments used in callback functions
 *
 * Returns: nothing
 */
void pst_callback_fn(multi_timer_t *mt, single_timer_t *timer, void *callback_args)
{
    callback_args_t *curr_callback_args = (callback_args_t *)callback_args;
    chitcpd_timeout(curr_callback_args->si, curr_callback_args->entry, PERSIST);
}

/*
 * Adds a packet to the retransmission queue
 *
 * tcp_data: tcp_data with the retransmission queue
 *
 * packet: packet to be added to the retransmission queue
 *
 * seq: current sequence number
 *
 * Returns: nothing
 */
void rtqueue_append(tcp_data_t *tcp_data, tcp_packet_t *packet, tcp_seq seq)
{
    retransmission_queue_t *retransmission_queue = tcp_data->rt_queue;
    retransmission_queue_t *temp = calloc(1, sizeof(retransmission_queue_t)); // new packet to be added
    temp->packet_sent_ts = (struct timespec *)calloc(1, sizeof(struct timespec));
    // Set sent time
    clock_gettime(CLOCK_REALTIME, temp->packet_sent_ts);
    temp->retransmitted = false;
    temp->expected_ack_seq = TCP_PAYLOAD_LEN(packet) + seq;
    temp->packet_sent = packet;

    // Add to list
    DL_APPEND(tcp_data->rt_queue, temp);

    // Reset timer
    single_timer_t *timer = NULL;
    mt_get_timer_by_id(tcp_data->mt, RETRANSMISSION, &timer);
    if ((!tcp_data->rtms_timer_on) && (tcp_data->rt_queue != NULL))
    {
        tcp_data->rtms_timer_on = true;
        mt_set_timer(tcp_data->mt, RETRANSMISSION, tcp_data->RTO,
                     timer->callback_fn, timer->callback_args);
    }
}

/* A helper function to update RTO, RTT, SRTT of a tcp block
 *
 * tcp_data: tcp_data with RTT to be extimated
 *
 * recv_ts: timespec of recv packet
 *
 * sent_ts: timespec of sent packet
 *
 * Returns: nothing
 */
void update_RTO(tcp_data_t *tcp_data, struct timespec *recv_ts, struct timespec *sent_ts)
{
    struct timespec *RTT = malloc(sizeof(struct timespec));
    double beta = 0.25;
    double alpha = 0.125;

    // calculate RTT
    timespec_subtract(RTT, recv_ts, sent_ts);
    tcp_data->RTT = RTT->tv_sec * SECOND + RTT->tv_nsec;

    if (tcp_data->first_RTT)
    {
        /*
         * When the first RTT measurement R is made, the host MUST set
         * SRTT <- R
         * RTTVAR <- R/2
         * RTO <- SRTT + max (G, K*RTTVAR)
         * where K = 4.
         */
        tcp_data->SRTT = tcp_data->RTT;
        tcp_data->RTTVAR = tcp_data->RTT / 2;
        if (CLOCK_GRANULARITY > 4 * tcp_data->RTTVAR)
        {
            tcp_data->RTO = tcp_data->SRTT + CLOCK_GRANULARITY;
        }
        else
        {
            tcp_data->RTO = tcp_data->SRTT + 4 * tcp_data->RTTVAR;
        }
        tcp_data->first_RTT = false;
    }
    else
    {
        /*
         * When a subsequent RTT measurement R' is made, a host MUST set
         * RTTVAR <- (1 - beta) * RTTVAR + beta * |SRTT - R'|
         * SRTT <- (1 - alpha) * SRTT + alpha * R'
         */
        tcp_data->RTTVAR = ((1 - beta) * (tcp_data->RTTVAR) + beta * abs(tcp_data->SRTT - tcp_data->RTT));
        tcp_data->SRTT = ((1 - alpha) * (tcp_data->SRTT) + alpha * tcp_data->RTT);

        /*
         * After the computation, a host MUST update
         * RTO <- SRTT + max (G, K*RTTVAR)
         */
        if (CLOCK_GRANULARITY > 4 * tcp_data->RTTVAR)
        {
            tcp_data->RTO = tcp_data->SRTT + CLOCK_GRANULARITY;
        }
        else
        {
            tcp_data->RTO = tcp_data->SRTT + 4 * tcp_data->RTTVAR;
        }
    }

    /* Check the bound of the RTO */
    if (tcp_data->RTO > MAX_RTO)
    {
        tcp_data->RTO = MAX_RTO;
    }
    else if (tcp_data->RTO < MIN_RTO)
    {
        tcp_data->RTO = MIN_RTO;
    }
}

/*
 * Removes a packet from the retransmission queue
 *
 * tcp_data: tcp_data with retransmission queue
 *
 * packet: timespec of recv packet
 *
 * si: server information for callback function
 *
 * entry: chisocket entry for callback function
 *
 * ack_seq: acked sequence
 *
 * Returns: nothing
 */
void rtqueue_pop(tcp_data_t *tcp_data, tcp_packet_t *packet, serverinfo_t *si, chisocketentry_t *entry, tcp_seq ack_seq)
{
    retransmission_queue_t *retransmission_queue = tcp_data->rt_queue;
    retransmission_queue_t *elt, *temp;
    callback_args_t *callback_args = calloc(1, sizeof(callback_args_t));
    callback_args->si = si;
    callback_args->entry = entry;
    callback_args->timer_type = RETRANSMISSION;

    /*
     * Since a TCP packet could acknowledge multiple packets at once,
     * make sure to traverse the retransmission queue in case there
     * are multiple packets that should be removed.
     */
    if (retransmission_queue != NULL)
    {
        if (ack_seq < 0)
        {
            // SYN segments
            // update RTO, SRTT, RTTVAR
            struct timespec *curr_ts = calloc(1, sizeof(struct timespec));
            clock_gettime(CLOCK_REALTIME, curr_ts);
            update_RTO(tcp_data, curr_ts, retransmission_queue->packet_sent_ts);
            DL_DELETE(tcp_data->rt_queue, retransmission_queue);
            free_packet(retransmission_queue->packet_sent);
            free(retransmission_queue);
        }
        else
        {
            DL_FOREACH(tcp_data->rt_queue, elt)
            {
                if (elt->expected_ack_seq <= ack_seq)
                {
                    // update RTO, SRTT, RTTVAR
                    struct timespec *curr_ts = calloc(1, sizeof(struct timespec));
                    clock_gettime(CLOCK_REALTIME, curr_ts);
                    update_RTO(tcp_data, curr_ts, elt->packet_sent_ts);
                    circular_buffer_read(&tcp_data->send, NULL,
                                         TCP_PAYLOAD_LEN(elt->packet_sent), FALSE);
                    DL_DELETE(tcp_data->rt_queue, elt);
                    free_packet(elt->packet_sent);
                    free(elt);
                }
                else
                {
                    break;
                }
            }
        }

        if (tcp_data->rtms_timer_on)
        {
            tcp_data->rtms_timer_on = false;
            mt_cancel_timer(tcp_data->mt, RETRANSMISSION);
        }
        int len;
        retransmission_queue_t *temp;
        DL_COUNT(tcp_data->rt_queue, temp, len);
        if (len > 0)
        {
            single_timer_t *timer = NULL;
            mt_get_timer_by_id(tcp_data->mt, RETRANSMISSION, &timer);
            if (!tcp_data->rtms_timer_on)
            {
                tcp_data->rtms_timer_on = true;
                mt_set_timer(tcp_data->mt, RETRANSMISSION, tcp_data->RTO,
                             timer->callback_fn, timer->callback_args);
            }
        }
    }
    else
    {
        // Retransmission Queue is empty
        mt_cancel_timer(tcp_data->mt, RETRANSMISSION);
    }
}

/*
 * A helper function to handle a close call
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 */
int chitcpd_tcp_state_handle_APPLICATION_CLOSE(serverinfo_t *si, chisocketentry_t *entry)
{
    tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;
    tcp_packet_t *packet_to_send = calloc(1, sizeof(tcp_packet_t));

    chitcpd_tcp_packet_create(entry, packet_to_send, NULL, 0);
    tcphdr_t *header_to_send = TCP_PACKET_HEADER(packet_to_send);
    header_to_send->syn = 0;
    header_to_send->ack = 1;
    header_to_send->fin = 1;
    header_to_send->seq = chitcp_htonl(tcp_data->SND_NXT);
    header_to_send->ack_seq = chitcp_htonl(tcp_data->RCV_NXT);
    header_to_send->win = chitcp_htons(tcp_data->RCV_WND);
    chitcpd_send_tcp_packet(si, entry, packet_to_send);

    // Add packet to retransmission queue
    rtqueue_append(tcp_data, packet_to_send, tcp_data->SND_NXT);

    tcp_data->closing = false;
    tcp_data->SND_NXT += 1;
}

/* A helper function to send buffer
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * Returns: nothing
 */
void chitcp_update_send_buffer(serverinfo_t *si, chisocketentry_t *entry)
{
    chilog(DEBUG, "chitcp_update_send_buffer");
    tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;

    int bytes_to_send = circular_buffer_count(&tcp_data->send);

    if (bytes_to_send == 0 && tcp_data->closing)
    {
        chilog(DEBUG, "CLOSING EVENT");
        /* issue an ACK & FIN */
        chitcpd_tcp_state_handle_APPLICATION_CLOSE(si, entry);
        return;
    }
    if (bytes_to_send <= ((int)tcp_data->SND_NXT - (int)tcp_data->SND_UNA))
    {
        // nothing to send
        return;
    }
    else
    {
        if (circular_buffer_count(&tcp_data->send) > (int)tcp_data->SND_WND)
        {
            bytes_to_send = (int)tcp_data->SND_WND - ((int)tcp_data->SND_NXT - (int)tcp_data->SND_UNA);
        }
        else
        {
            bytes_to_send = circular_buffer_count(&tcp_data->send) - ((int)tcp_data->SND_NXT - (int)tcp_data->SND_UNA);
        }

        while (bytes_to_send > 0)
        {
            // Have bytes to send
            int tcp_payload_len = bytes_to_send;
            tcp_packet_t *packet_to_send = calloc(1, sizeof(tcp_packet_t));
            if (bytes_to_send > TCP_MSS)
            {
                tcp_payload_len = TCP_MSS;
            }
            int bytes_read = 0;
            uint8_t payload_dst[tcp_payload_len];
            bytes_read = circular_buffer_peek_at(&tcp_data->send,
                                                 payload_dst,
                                                 tcp_data->SND_NXT,
                                                 tcp_payload_len);

            if (bytes_read > 0)
            {
                // update send buffer
                bytes_to_send -= bytes_read;

                // create send packet
                chitcpd_tcp_packet_create(entry, packet_to_send,
                                          payload_dst,
                                          tcp_payload_len);
                tcphdr_t *header = TCP_PACKET_HEADER(packet_to_send);

                // issue an ACK segment
                header->syn = 0;
                header->ack = 1;
                header->fin = 0;
                header->seq = chitcp_htonl(tcp_data->SND_NXT);
                header->ack_seq = chitcp_htonl(tcp_data->RCV_NXT);
                header->win = chitcp_htons(tcp_data->RCV_WND);
                chitcpd_send_tcp_packet(si, entry, packet_to_send);

                // add packet to retransmission queue
                rtqueue_append(tcp_data, packet_to_send, tcp_data->SND_NXT);

                // update Tcp SND_NXT
                tcp_data->SND_NXT += (uint32_t)bytes_read;
            }
        }
    }
}

/*
 * Initiate tcp data
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * Returns: nothing
 */
void tcp_data_init(serverinfo_t *si, chisocketentry_t *entry)
{
    tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;

    tcp_data->pending_packets = NULL;
    pthread_mutex_init(&tcp_data->lock_pending_packets, NULL);
    pthread_cond_init(&tcp_data->cv_pending_packets, NULL);

    /* Initialization of additional tcp_data_t fields,
     * and creation of retransmission thread, goes here */
    tcp_data->mt = (multi_timer_t *)malloc(sizeof(multi_timer_t));
    if (mt_init(tcp_data->mt, 2) != CHITCP_OK)
    {
        /* multitimer not created successfully */
        return;
    }

    callback_args_t *callback_args_1 = calloc(1, sizeof(callback_args_t));
    callback_args_1->si = si;
    callback_args_1->entry = entry;
    callback_args_1->timer_type = RETRANSMISSION;
    tcp_data->mt->timers[0]->callback_fn = rtx_callback_fn;
    tcp_data->mt->timers[0]->callback_args = callback_args_1;
    mt_set_timer_name(tcp_data->mt, RETRANSMISSION, "RETRANSMISSION");

    /* init PERSIST timer */
    callback_args_t *callback_args_2 = calloc(1, sizeof(callback_args_t));
    callback_args_2->si = si;
    callback_args_2->entry = entry;
    callback_args_2->timer_type = PERSIST;
    tcp_data->mt->timers[1]->callback_fn = pst_callback_fn;
    tcp_data->mt->timers[1]->callback_args = callback_args_2;
    mt_set_timer_name(tcp_data->mt, PERSIST, "PERSIST");

    tcp_data->rt_queue = NULL;
    tcp_data->list = NULL;
    tcp_data->RTO = MIN_RTO;
    tcp_data->rtms_timer_on = false;
    tcp_data->first_RTT = false;
    tcp_data->probe_packet = NULL;
    tcp_data->closing = false;
}

/*
 * Free tcp data
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * Returns: nothing
 */
void tcp_data_free(serverinfo_t *si, chisocketentry_t *entry)
{
    tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;
    // free pending packets list
    chitcp_packet_list_destroy(&tcp_data->pending_packets);
    // destroy thread mutex and cond
    pthread_mutex_destroy(&tcp_data->lock_pending_packets);
    pthread_cond_destroy(&tcp_data->cv_pending_packets);

    return;
}

/* Handler function to handle retransmission timeout (TIMEOUT_RTX) event
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * Returns: CHITCP_OK
 */
int chitcpd_tcp_state_handle_TIMEOUT_RTX(serverinfo_t *si, chisocketentry_t *entry)
{
    tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;
    /* update RTO */
    tcp_data->RTO = tcp_data->RTO * 2;
    if (tcp_data->RTO > MAX_RTO)
    {
        tcp_data->RTO = MAX_RTO;
    }
    else if (tcp_data->RTO < MIN_RTO)
    {
        tcp_data->RTO = MIN_RTO;
    }

    retransmission_queue_t *elt;
    DL_FOREACH(tcp_data->rt_queue, elt)
    {
        tcphdr_t *header_to_send = TCP_PACKET_HEADER(elt->packet_sent);
        if (tcp_data->SND_WND == 0 &&
            header_to_send->syn != 1 &&
            header_to_send->fin != 1)
        {
            continue;
        }
        clock_gettime(CLOCK_REALTIME, elt->packet_sent_ts);
        elt->retransmitted = true;
        chitcpd_send_tcp_packet(si, entry, elt->packet_sent);
    }

    tcp_data->rtms_timer_on = false;
    /* Reset retransmission timer */
    single_timer_t *timer = NULL;
    mt_get_timer_by_id(tcp_data->mt, RETRANSMISSION, &timer);
    if ((!tcp_data->rtms_timer_on) && (tcp_data->rt_queue != NULL))
    {
        tcp_data->rtms_timer_on = true;
        mt_set_timer(tcp_data->mt, RETRANSMISSION, tcp_data->RTO,
                     timer->callback_fn, timer->callback_args);
    }

    return CHITCP_OK;
}

/* Handler function to handle persist timeout (TIMEOUT_PST) event
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * Returns: CHITCP_OK
 */
int chitcpd_tcp_state_handle_TIMEOUT_PST(serverinfo_t *si, chisocketentry_t *entry)
{
    tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;
    /* Sends probe segment */
    tcp_packet_t *packet_to_send = malloc(sizeof(tcp_packet_t));

    if (circular_buffer_count(&tcp_data->send) <= 0)
    {
        /* If the timer expires, and there is currently
         * no data to send (i.e., if the send buffer is empty),
         * reset the timer to expire after RTO seconds.
         */
        free(packet_to_send);
    }
    else
    {
        /* If the timer expires, and there is data to send,
         * then send a probe segment with a single byte of
         * data from the send buffer. Reset the timer to
         * expire after RTO seconds. Update SND.NXT.
         */
        if (tcp_data->probe_packet == NULL)
        {
            /* send 1 byte probe segment */
            uint8_t payload[1];
            int bytes_read = circular_buffer_peek_at(&tcp_data->send,
                                                     payload,
                                                     tcp_data->SND_NXT, 1);
            if (bytes_read > 0)
            {
                chitcpd_tcp_packet_create(entry, packet_to_send, payload, 1);
                tcphdr_t *header_to_send = TCP_PACKET_HEADER(packet_to_send);
                /* Issue a ACK segment */
                header_to_send->syn = 0;
                header_to_send->ack = 1;
                header_to_send->fin = 0;
                header_to_send->seq = chitcp_htonl(tcp_data->SND_NXT);
                header_to_send->ack_seq = chitcp_htonl(tcp_data->RCV_NXT);
                header_to_send->win = chitcp_htons(tcp_data->RCV_WND);
                tcp_data->SND_NXT++;
                chitcpd_send_tcp_packet(si, entry, packet_to_send);
                tcp_data->probe_packet = packet_to_send;
            }
        }
        else
        {
            chitcpd_send_tcp_packet(si, entry, tcp_data->probe_packet);
        }
    }

    /* Reset PERSIST timer */
    single_timer_t *timer = NULL;
    mt_get_timer_by_id(tcp_data->mt, PERSIST, &timer);
    mt_set_timer(tcp_data->mt, PERSIST, tcp_data->RTO,
                 timer->callback_fn, timer->callback_args);

    return CHITCP_OK;
}

/*
 * Handler function to handle PACKET_ARRIVAL
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * event: event to handle
 *
 * Returns: CHITCP_OK/CHITCP_ERROR
 */
int chitcpd_tcp_state_handle_PACKET_ARRIVAL(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    // current tcp info
    tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;
    tcp_state_t tcp_state = entry->tcp_state;
    tcp_packet_t *packet = NULL;
    retransmission_queue_t *retransmission_queue = tcp_data->rt_queue;

    /* Extract and remove packet at the top of the queue */
    if (tcp_data->pending_packets)
    {
        /* tcp_data->pending_packets points to the head node of the list */
        pthread_mutex_lock(&tcp_data->lock_pending_packets);
        packet = tcp_data->pending_packets->packet;
        chitcp_packet_list_pop_head(&tcp_data->pending_packets);
        pthread_mutex_unlock(&tcp_data->lock_pending_packets);
        /* This removes the list node at the head of the list */
    }
    else
    {
        return CHITCP_OK;
    }
    // create packet header
    tcphdr_t *header = TCP_PACKET_HEADER(packet);
    tcp_packet_t *packet_to_send = calloc(1, sizeof(tcp_packet_t));
    chitcpd_tcp_packet_create(entry, packet_to_send, NULL, 0);
    tcphdr_t *header_to_send = TCP_PACKET_HEADER(packet_to_send);

    if (tcp_state == CLOSED)
    {
        chilog(DEBUG, "In CLOSED state, discard packet.");
        // do nothing
        return CHITCP_OK;
    }
    else if (tcp_state == LISTEN)
    {
        // 1: check RST: skip
        // 2: check ACK
        if (header->ack == 1)
        {
            // reset
            chilog(INFO, "ACK is bad if it arrives on a connection still in the LISTEN state. RESET");
            return CHITCP_OK;
        }
        if (header->syn == 1)
        {
            // is SYN
            // check security: skip
            uint32_t ISS = random_number();
            tcp_data->ISS = ISS;
            tcp_data->SND_UNA = ISS;
            tcp_data->SND_NXT = ISS + 1;
            tcp_data->RCV_WND = circular_buffer_capacity(&tcp_data->recv);

            tcp_data->IRS = chitcp_ntohl(header->seq);
            tcp_data->RCV_NXT = chitcp_ntohl(header->seq) + 1;
            // tcp_data->SND_WND = circular_buffer_capacity(&tcp_data->send);
            circular_buffer_set_seq_initial(&tcp_data->send, tcp_data->SND_NXT);
            circular_buffer_set_seq_initial(&tcp_data->recv, tcp_data->RCV_NXT);

            /* Issue a SYN & ACK segment */
            header_to_send->syn = 1;
            header_to_send->ack = 1;
            header_to_send->fin = 0;
            header_to_send->seq = chitcp_htonl(tcp_data->ISS);
            header_to_send->ack_seq = chitcp_htonl(tcp_data->RCV_NXT);
            header_to_send->win = chitcp_htons(tcp_data->RCV_WND);
            chitcpd_send_tcp_packet(si, entry, packet_to_send);

            // add packet to retransmission queue
            rtqueue_append(tcp_data, packet_to_send, tcp_data->ISS);

            /* change state to SYN_RCVD */
            chilog(DEBUG, "chaneg state to SYN_RCVD!");
            chitcpd_update_tcp_state(si, entry, SYN_RCVD);
            return CHITCP_OK;
        }
    }
    else if (tcp_state == SYN_SENT)
    {
        // 1: check ACK
        if (header->ack == 1)
        {
            // If SEG.ACK =< ISS, or SEG.ACK > SND.NXT, send a reset
            if (chitcp_ntohl(header->ack_seq) <= tcp_data->ISS ||
                chitcp_ntohl(header->ack_seq) > tcp_data->SND_NXT)
            {
                // not acceptable
                chilog(INFO, "If SEG.ACK =< ISS, or SEG.ACK > SND.NXT, send a reset");
                return CHITCP_OK;
            }
        }
        // 2: check RST: skip
        // 3: check security and precedence: skip
        // 4: check SYN
        if (header->syn == 1)
        {
            /* When a packet is acknowledged, remove it from the retranmission queue. */
            tcp_data->first_RTT = true;
            rtqueue_pop(tcp_data, packet, si, entry, -1);

            tcp_data->RCV_NXT = chitcp_ntohl(header->seq) + 1;
            tcp_data->IRS = chitcp_ntohl(header->seq);
            tcp_data->SND_UNA = chitcp_ntohl(header->ack_seq);
            tcp_data->SND_NXT = chitcp_ntohl(header->ack_seq);
            tcp_data->SND_WND = chitcp_ntohs(header->win);
            circular_buffer_set_seq_initial(&tcp_data->recv, tcp_data->RCV_NXT);

            if (tcp_data->SND_UNA > tcp_data->ISS)
            {
                chilog(DEBUG, "SYN has been ACKed!");
                // SYN has been ACKed
                /* Issue a ACK segment */
                header_to_send->syn = 0;
                header_to_send->ack = 1;
                header_to_send->fin = 0;
                header_to_send->seq = chitcp_htonl(tcp_data->SND_NXT);
                header_to_send->ack_seq = chitcp_htonl(tcp_data->RCV_NXT);
                header_to_send->win = chitcp_htons(tcp_data->RCV_WND);
                chitcpd_send_tcp_packet(si, entry, packet_to_send);

                free_packet(packet_to_send);

                //  change the connection state to ESTABLISHED
                chilog(DEBUG, "chaneg state to ESTABLISHED!");
                chitcpd_update_tcp_state(si, entry, ESTABLISHED);
            }
            else
            {
                chilog(DEBUG, "SYN has not been ACKed!");
                // SYN has not been ACKed
                /* Issue a SYN & ACK segment */
                header_to_send->syn = 1;
                header_to_send->ack = 1;
                header_to_send->fin = 0;
                header_to_send->seq = chitcp_htonl(tcp_data->ISS);
                header_to_send->ack_seq = chitcp_htonl(tcp_data->RCV_NXT);
                header_to_send->win = chitcp_htons(tcp_data->RCV_WND);
                chitcpd_send_tcp_packet(si, entry, packet_to_send);

                // add packet to retransmission queue
                rtqueue_append(tcp_data, packet_to_send, header_to_send->seq);

                // change the connection state to SYN_RCVD
                chitcpd_update_tcp_state(si, entry, SYN_RCVD);
            }
            return CHITCP_OK;
        }
        // 5: if neither of the SYN or RST bits is set then drop the segment and return.
        return CHITCP_OK;
    }
    else
    {
        /*
        SYN-RECEIVED STATE  ||  ESTABLISHED STATE   ||  FIN-WAIT-1 STATE    ||
        FIN-WAIT-2 STATE    ||  CLOSE-WAIT STATE    ||  CLOSING STATE       ||
        LAST-ACK STATE      ||  TIME-WAIT STATE
        */
        // 1: check sequence number
        uint16_t Receive_Window = tcp_data->RCV_WND;
        uint16_t Segment_Length = SEG_LEN(packet);

        if (Segment_Length < 0)
        {
            // segment error
            return CHITCP_OK;
        }

        if (chitcp_ntohl(header->seq) > tcp_data->RCV_NXT && Segment_Length)
        {
            /* Out of order segment */
            out_of_order_list_t *out_of_order_seg = malloc(sizeof(out_of_order_list_t));
            out_of_order_seg->seq = chitcp_ntohl(header->seq);
            out_of_order_seg->packet = packet;
            DL_INSERT_INORDER(tcp_data->list, out_of_order_seg, segmentcmp);

            /* Remove duplicates */
            if ((out_of_order_seg->next != NULL &&
                 out_of_order_seg->next->seq == out_of_order_seg->seq) ||
                (out_of_order_seg->prev != NULL &&
                 out_of_order_seg->prev->seq == out_of_order_seg->seq))
            {
                DL_DELETE(tcp_data->list, out_of_order_seg);
            }
            return CHITCP_OK;
        }

        if (chitcp_ntohl(header->seq) < tcp_data->RCV_NXT)
        {
            return CHITCP_OK;
        }

        bool seq_acceptability = true;

        if (Segment_Length == 0 && Receive_Window == 0)
        {
            if (tcp_data->RCV_NXT != chitcp_ntohl(header->seq))
            {
                seq_acceptability = false;
            }
        }
        else if (Segment_Length == 0 && Receive_Window > 0)
        {
            if (tcp_data->RCV_NXT > chitcp_ntohl(header->seq) ||
                chitcp_ntohl(header->seq) >= (tcp_data->RCV_NXT + tcp_data->RCV_WND))
            {
                seq_acceptability = false;
            }
        }
        else if (Segment_Length > 0 && Receive_Window == 0)
        {
            seq_acceptability = false;
        }
        else if (Segment_Length > 0 && Receive_Window > 0)
        {
            if ((tcp_data->RCV_NXT > chitcp_ntohl(header->seq) ||
                 chitcp_ntohl(header->seq) >= (tcp_data->RCV_NXT + tcp_data->RCV_WND)) &&
                (tcp_data->RCV_NXT > chitcp_ntohl(header->seq) + Segment_Length - 1 ||
                 chitcp_ntohl(header->seq) + Segment_Length - 1 >= (tcp_data->RCV_NXT + tcp_data->RCV_WND)))
            {
                seq_acceptability = false;
            }
        }

        if (!seq_acceptability)
        {
            /* Issue a ACK segment */
            header_to_send->syn = 0;
            header_to_send->ack = 1;
            header_to_send->fin = 0;
            header_to_send->seq = chitcp_htonl(tcp_data->SND_NXT);
            header_to_send->ack_seq = chitcp_htonl(tcp_data->RCV_NXT);
            header_to_send->win = chitcp_htons(tcp_data->RCV_WND);
            chitcpd_send_tcp_packet(si, entry, packet_to_send);
            free_packet(packet_to_send);

            return CHITCP_OK;
        }

        // 2: check RST: skip
        // 3: third check security and precedence: skip
        // 4: check SYN
        if (header->syn == 1)
        {
            // SYN in the window
            return CHITCP_OK;
        }

        // 5: check ACK
        if (header->ack == 0)
        {
            // the ACK bit is off: drop
            chilog(INFO, "The ACK bit is off: drop.");
            return CHITCP_OK;
        }
        else
        {
            // SYN-RECEIVED STATE
            if (tcp_state == SYN_RCVD)
            {
                if (tcp_data->SND_UNA <= chitcp_ntohl(header->ack_seq) &&
                    chitcp_ntohl(header->ack_seq) <= tcp_data->SND_NXT)
                {
                    /* When a packet is acknowledged, remove it from the retranmission queue. */
                    tcp_data->first_RTT = true;
                    rtqueue_pop(tcp_data, packet, si, entry, -1);

                    // change the connection state to ESTABLISHED
                    tcp_data->SND_UNA = chitcp_ntohl(header->ack_seq);
                    tcp_data->SND_NXT = chitcp_ntohl(header->ack_seq);
                    tcp_data->SND_WND = chitcp_ntohs(header->win);
                    chilog(DEBUG, "change state to ESTABLISHED!");
                    chitcpd_update_tcp_state(si, entry, ESTABLISHED);
                    return CHITCP_OK;
                }
            }

            if ((tcp_state == ESTABLISHED) ||
                (tcp_state == FIN_WAIT_1) ||
                (tcp_state == FIN_WAIT_2) ||
                (tcp_state == CLOSE_WAIT) ||
                (tcp_state == CLOSING) ||
                (tcp_state == LAST_ACK) ||
                (tcp_state == TIME_WAIT))
            {
                if ((tcp_data->SND_UNA <= chitcp_ntohl(header->ack_seq)) &&
                    (chitcp_ntohl(header->ack_seq) <= tcp_data->SND_NXT))
                {
                    /* When a packet is acknowledged, remove it from the retranmission queue. */
                    rtqueue_pop(tcp_data,
                                packet, si, entry,
                                chitcp_ntohl(header->ack_seq));
                    /* When a segment is received with SEG.WND=0 (i.e., the advertised window is zero),
                     * set the persist timer to expire after RTO seconds. If a segment is received with
                     * SEG.WND>0 before the timer expires, then cancel the timer.
                     */
                    single_timer_t *pst;
                    mt_get_timer_by_id(tcp_data->mt, PERSIST, &pst);
                    if (chitcp_ntohs(header->win) == 0)
                    {
                        if (pst->active)
                        {
                            mt_cancel_timer(tcp_data->mt, PERSIST);
                        }
                        /*
                        mt_set_timer(tcp_data->mt, PERSIST,
                                     tcp_data->RTO, pst->callback_fn,
                                     pst->callback_args);
                        */
                        single_timer_t *timer = NULL;
                        mt_get_timer_by_id(tcp_data->mt, PERSIST, &timer);
                        mt_set_timer(tcp_data->mt, PERSIST, tcp_data->RTO,
                                    timer->callback_fn, timer->callback_args);
                    }
                    else if (chitcp_ntohs(header->win) > 0 && tcp_data->SND_WND == 0)
                    {
                        /*
                        if (pst->active)
                        {
                            mt_cancel_timer(tcp_data->mt, PERSIST);
                        }
                        */
                        mt_cancel_timer(tcp_data->mt, PERSIST);
                        if (tcp_data->probe_packet != NULL)
                        {
                            free_packet(tcp_data->probe_packet);
                            tcp_data->probe_packet = NULL;
                            circular_buffer_read(&tcp_data->send, NULL, 1, FALSE);
                        }
                    }
                    tcp_data->SND_UNA = chitcp_ntohl(header->ack_seq);
                    tcp_data->SND_WND = chitcp_ntohs(header->win);
                    // update send buffer
                    chitcp_update_send_buffer(si, entry);
                }

                else if (chitcp_ntohl(header->ack_seq) > tcp_data->SND_NXT)
                {
                    // issue an Ack segment
                    header_to_send->syn = 0;
                    header_to_send->ack = 1;
                    header_to_send->fin = 0;
                    header_to_send->seq = chitcp_htonl(tcp_data->SND_NXT);
                    header_to_send->ack_seq = chitcp_htonl(tcp_data->RCV_NXT);
                    header_to_send->win = chitcp_htons(tcp_data->RCV_WND);
                    chitcpd_send_tcp_packet(si, entry, packet_to_send);
                    free_packet(packet_to_send);
                }
                else
                {
                    chilog(INFO, "The ACK is a duplicate, it can be ignored.");
                    return CHITCP_OK;
                }
                if (tcp_state == FIN_WAIT_1)
                {
                    if (chitcp_ntohl(header->ack_seq) == tcp_data->SND_NXT)
                    {
                        chitcpd_update_tcp_state(si, entry, FIN_WAIT_2);
                    }
                }
                else if (tcp_state == CLOSING)
                {
                    if (chitcp_ntohl(header->ack_seq) == tcp_data->SND_NXT)
                    {
                        chitcpd_update_tcp_state(si, entry, TIME_WAIT);
                        chitcpd_update_tcp_state(si, entry, CLOSED);
                    }
                }
                else if (tcp_state == LAST_ACK)
                {
                    if (chitcp_ntohl(header->ack_seq) == tcp_data->SND_NXT)
                    {
                        chitcpd_update_tcp_state(si, entry, CLOSED);
                    }
                }
            }
            // 6: check the URG bit, skip
            // 7: process the segment text
            if ((tcp_state == ESTABLISHED) ||
                (tcp_state == FIN_WAIT_1) ||
                (tcp_state == FIN_WAIT_2))
            {
                if ((header->fin != 1) &&
                    (tcp_state == ESTABLISHED) &&
                    (TCP_PAYLOAD_LEN(packet) > 0))
                {
                    circular_buffer_write(&tcp_data->recv,
                                          TCP_PAYLOAD_START(packet),
                                          TCP_PAYLOAD_LEN(packet),
                                          FALSE);

                    tcp_data->RCV_NXT = circular_buffer_next(&tcp_data->recv);
                    tcp_data->RCV_WND = circular_buffer_available(&tcp_data->recv);

                    out_of_order_list_t *l;
                    int len;
                    DL_COUNT(tcp_data->list, l, len);
                    int next_seq = tcp_data->RCV_NXT;
                    if (len > 0)
                    {
                        DL_FOREACH(tcp_data->list, l)
                        {
                            if (l != NULL && l->seq == next_seq)
                            {
                                pthread_mutex_lock(&tcp_data->lock_pending_packets);
                                chitcp_packet_list_append(
                                    &tcp_data->pending_packets,
                                    l->packet);
                                pthread_mutex_unlock(&tcp_data->lock_pending_packets);

                                next_seq += TCP_PAYLOAD_LEN(l->packet);
                                DL_DELETE(tcp_data->list, l);
                                free(l);
                            }
                        }
                    }
                    header_to_send->ack = 1;
                    header_to_send->syn = 0;
                    header_to_send->fin = 0;
                    header_to_send->seq = chitcp_htonl(tcp_data->SND_NXT);
                    header_to_send->ack_seq = chitcp_htonl(next_seq);
                    header_to_send->win = chitcp_htons(tcp_data->RCV_WND);
                    chitcpd_send_tcp_packet(si, entry, packet_to_send);
                    free_packet(packet_to_send);
                }
            }
            else
            {
                return CHITCP_OK;
            }

            // 8: check the FIN bit
            if ((tcp_state == CLOSED) ||
                (tcp_state == LISTEN) ||
                (tcp_state == SYN_SENT))
            {
                return CHITCP_OK;
            }
            else
            {
                if (header->fin == 1)
                {
                    /* issue ACK */
                    tcp_data->RCV_NXT = chitcp_ntohl(header->seq) + 1;
                    header_to_send->ack = 1;
                    header_to_send->syn = 0;
                    header_to_send->fin = 0;
                    header_to_send->seq = chitcp_htonl(tcp_data->SND_NXT);
                    header_to_send->ack_seq = chitcp_htonl(tcp_data->RCV_NXT);
                    header_to_send->win = chitcp_htons(tcp_data->RCV_WND);
                    chitcpd_send_tcp_packet(si, entry, packet_to_send);
                    free_packet(packet_to_send);

                    if ((tcp_state == SYN_RCVD) || (tcp_state == ESTABLISHED))
                    {
                        chitcpd_update_tcp_state(si, entry, CLOSE_WAIT);
                        return CHITCP_OK;
                    }
                    else if (tcp_state == FIN_WAIT_1)
                    {
                        if (chitcp_ntohl(header->ack_seq) == tcp_data->SND_NXT)
                        {
                            tcp_data->RCV_NXT = chitcp_ntohl(header->seq) + 1;
                            tcp_data->SND_NXT = chitcp_ntohl(header->ack_seq);

                            chitcpd_update_tcp_state(si, entry, TIME_WAIT);
                            chitcpd_update_tcp_state(si, entry, CLOSING);
                            // free_packet(packet);
                            return CHITCP_OK;
                        }
                        else
                        {
                            chitcpd_update_tcp_state(si, entry, CLOSING);
                            return CHITCP_OK;
                        }
                    }
                    else if (tcp_state == FIN_WAIT_2)
                    {
                        chitcpd_update_tcp_state(si, entry, TIME_WAIT);
                        chitcpd_update_tcp_state(si, entry, CLOSED);
                        return CHITCP_OK;
                    }
                }
            }
        }
    }
}

/*
 * Handler function to handle CLOSED state
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * event: event to handle
 *
 * Returns: CHITCP_OK/CHITCP_ERROR
 */
int chitcpd_tcp_state_handle_CLOSED(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == APPLICATION_CONNECT)
    {
        /* Create a new transmission control block (TCB) */
        tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;
        uint32_t ISS = random_number();
        tcp_data->ISS = ISS;
        tcp_data->SND_UNA = ISS;
        tcp_data->SND_NXT = ISS + 1;
        tcp_data->RCV_WND = circular_buffer_capacity(&tcp_data->recv);
        // tcp_data->SND_WND = circular_buffer_available(&tcp_data->send);
        circular_buffer_set_seq_initial(&tcp_data->send, tcp_data->SND_NXT);

        /* Issue a SYN segment, only assign value to the following fields, other fields are already handled */
        tcp_packet_t *packet = calloc(1, sizeof(tcp_packet_t));
        chitcpd_tcp_packet_create(entry, packet, NULL, 0);
        tcphdr_t *header = TCP_PACKET_HEADER(packet);
        header->syn = 1;
        header->ack = 0;
        header->fin = 0;
        header->seq = chitcp_htonl(tcp_data->ISS);
        header->ack_seq = chitcp_ntohl(0);
        header->win = chitcp_htons(tcp_data->RCV_WND);
        chitcpd_send_tcp_packet(si, entry, packet);

        // add packet to retransmission queue
        rtqueue_append(tcp_data, packet, header->seq);

        chitcpd_update_tcp_state(si, entry, SYN_SENT);
    }
    else if (event == CLEANUP)
    {
        /* Any additional cleanup goes here */
        tcp_data_free(si, entry);
    }
    else
        chilog(WARNING, "In CLOSED state, received unexpected event.");

    return CHITCP_OK;
}

/*
 * Handler function to handle LISTEN state
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * event: event to handle
 *
 * Returns: CHITCP_OK/CHITCP_ERROR
 */
int chitcpd_tcp_state_handle_LISTEN(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == PACKET_ARRIVAL)
    {
        chilog(DEBUG, "PACKET_ARRIVAL");
        chitcpd_tcp_state_handle_PACKET_ARRIVAL(si, entry, event);
    }
    else
        chilog(WARNING, "In LISTEN state, received unexpected event.");

    return CHITCP_OK;
}

/*
 * Handler function to handle SYN_RCVD state
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * event: event to handle
 *
 * Returns: CHITCP_OK/CHITCP_ERROR
 */
int chitcpd_tcp_state_handle_SYN_RCVD(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == PACKET_ARRIVAL)
    {
        chilog(DEBUG, "PACKET_ARRIVAL");
        chitcpd_tcp_state_handle_PACKET_ARRIVAL(si, entry, event);
    }
    else if (event == TIMEOUT_RTX)
    {
        chilog(DEBUG, "TIMEOUT in SYN_RCVD");
        chitcpd_tcp_state_handle_TIMEOUT_RTX(si, entry);
    }
    else
        chilog(WARNING, "In SYN_RCVD state, received unexpected event.");

    return CHITCP_OK;
}

/*
 * Handler function to handle SYN_SENT state
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * event: event to handle
 *
 * Returns: CHITCP_OK/CHITCP_ERROR
 */
int chitcpd_tcp_state_handle_SYN_SENT(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == PACKET_ARRIVAL)
    {
        chilog(DEBUG, "PACKET_ARRIVAL");
        chitcpd_tcp_state_handle_PACKET_ARRIVAL(si, entry, event);
    }
    else if (event == TIMEOUT_RTX)
    {
        chilog(DEBUG, "TIMEOUT in SYN_SENT");
        chitcpd_tcp_state_handle_TIMEOUT_RTX(si, entry);
    }
    else
        chilog(WARNING, "In SYN_SENT state, received unexpected event.");

    return CHITCP_OK;
}

/*
 * Handler function to handle ESTABLISHED state
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * event: event to handle
 *
 * Returns: CHITCP_OK/CHITCP_ERROR
 */
int chitcpd_tcp_state_handle_ESTABLISHED(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;
    if (event == APPLICATION_SEND)
    {
        chilog(DEBUG, "APPLICATION_SEND");
        chitcp_update_send_buffer(si, entry);
    }
    else if (event == PACKET_ARRIVAL)
    {
        chilog(DEBUG, "PACKET_ARRIVAL");
        chitcpd_tcp_state_handle_PACKET_ARRIVAL(si, entry, event);
    }
    else if (event == APPLICATION_RECEIVE)
    {
        chilog(DEBUG, "APPLICATION_RECEIVE");
        tcp_data->RCV_WND = circular_buffer_available(&tcp_data->recv);
    }
    else if (event == APPLICATION_CLOSE)
    {
        chilog(DEBUG, "APPLICATION_CLOSE");
        tcp_data->closing = true;
        chitcp_update_send_buffer(si, entry);
        chitcpd_update_tcp_state(si, entry, FIN_WAIT_1);
    }
    else if (event == TIMEOUT_RTX)
    {
        chilog(DEBUG, "TIMEOUT in ESTABLISHED");
        chitcpd_tcp_state_handle_TIMEOUT_RTX(si, entry);
    }
    else if (event == TIMEOUT_PST)
    {
        chilog(DEBUG, "TIMEOUT_PST in ESTABLISHED");
        chitcpd_tcp_state_handle_TIMEOUT_PST(si, entry);
    }
    else
        chilog(WARNING, "In ESTABLISHED state, received unexpected event (%i).", event);

    return CHITCP_OK;
}

/*
 * Handler function to handle FIN_WAIT_1 state
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * event: event to handle
 *
 * Returns: CHITCP_OK/CHITCP_ERROR
 */
int chitcpd_tcp_state_handle_FIN_WAIT_1(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == PACKET_ARRIVAL)
    {
        chilog(DEBUG, "PACKET_ARRIVAL");
        chitcpd_tcp_state_handle_PACKET_ARRIVAL(si, entry, event);
    }
    else if (event == APPLICATION_RECEIVE)
    {
        tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;
        tcp_data->RCV_WND = circular_buffer_available(&tcp_data->recv);
    }
    else if (event == TIMEOUT_RTX)
    {
        chilog(DEBUG, "TIMEOUT in FIN_WAIT_1");
        chitcpd_tcp_state_handle_TIMEOUT_RTX(si, entry);
    }
    else if (event == TIMEOUT_PST)
    {
        chilog(DEBUG, "TIMEOUT_PST in FIN_WAIT_1");
        chitcpd_tcp_state_handle_TIMEOUT_PST(si, entry);
    }
    else
        chilog(WARNING, "In FIN_WAIT_1 state, received unexpected event (%i).", event);

    return CHITCP_OK;
}

/*
 * Handler function to handle FIN_WAIT_2 state
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * event: event to handle
 *
 * Returns: CHITCP_OK/CHITCP_ERROR
 */
int chitcpd_tcp_state_handle_FIN_WAIT_2(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;
    if (event == PACKET_ARRIVAL)
    {
        chilog(DEBUG, "PACKET_ARRIVAL");
        chitcpd_tcp_state_handle_PACKET_ARRIVAL(si, entry, event);
    }
    else if (event == APPLICATION_RECEIVE)
    {

        tcp_data->RCV_WND = circular_buffer_available(&tcp_data->recv);
    }
    else if (event == TIMEOUT_RTX)
    {
        chilog(DEBUG, "TIMEOUT in FIN_WAIT_2");
        chitcpd_tcp_state_handle_TIMEOUT_RTX(si, entry);
    }
    else
        chilog(WARNING, "In FIN_WAIT_2 state, received unexpected event (%i).", event);

    return CHITCP_OK;
}

/*
 * Handler function to handle CLOSE_WAIT state
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * event: event to handle
 *
 * Returns: CHITCP_OK/CHITCP_ERROR
 */
int chitcpd_tcp_state_handle_CLOSE_WAIT(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    tcp_data_t *tcp_data = &entry->socket_state.active.tcp_data;
    if (event == APPLICATION_CLOSE)
    {
        chilog(DEBUG, "APPLICATION_CLOSE");
        tcp_data->closing = true;
        chitcp_update_send_buffer(si, entry);
        // RFC 1122
        chitcpd_update_tcp_state(si, entry, LAST_ACK);
    }
    else if (event == PACKET_ARRIVAL)
    {
        chilog(DEBUG, "PACKET_ARRIVAL");
        chitcpd_tcp_state_handle_PACKET_ARRIVAL(si, entry, event);
    }
    else if (event == TIMEOUT_RTX)
    {
        chilog(DEBUG, "TIMEOUT in CLOSE_WAIT");
        chitcpd_tcp_state_handle_TIMEOUT_RTX(si, entry);
    }
    else if (event == TIMEOUT_PST)
    {
        chilog(DEBUG, "TIMEOUT_PST in CLOSE_WAIT");
        chitcpd_tcp_state_handle_TIMEOUT_PST(si, entry);
    }
    else
        chilog(WARNING, "In CLOSE_WAIT state, received unexpected event (%i).", event);

    return CHITCP_OK;
}

/*
 * Handler function to handle CLOSING event
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * event: event to handle
 *
 * Returns: CHITCP_OK/CHITCP_ERROR
 */
int chitcpd_tcp_state_handle_CLOSING(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == PACKET_ARRIVAL)
    {
        chilog(DEBUG, "PACKET_ARRIVAL");
        chitcpd_tcp_state_handle_PACKET_ARRIVAL(si, entry, event);
    }
    else if (event == TIMEOUT_RTX)
    {
        chilog(DEBUG, "TIMEOUT in CLOSING");
        chitcpd_tcp_state_handle_TIMEOUT_RTX(si, entry);
    }
    else if (event == TIMEOUT_PST)
    {
        chilog(DEBUG, "TIMEOUT_PST in CLOSING");
        chitcpd_tcp_state_handle_TIMEOUT_PST(si, entry);
    }
    else
        chilog(WARNING, "In CLOSING state, received unexpected event (%i).", event);

    return CHITCP_OK;
}

/*
 * Handler function to handle TIME_WAIT event
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * event: event to handle
 *
 * Returns: CHITCP_OK/CHITCP_ERROR
 */
int chitcpd_tcp_state_handle_TIME_WAIT(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    chilog(WARNING, "Running handler for TIME_WAIT. This should not happen.");

    return CHITCP_OK;
}

/*
 * Handler function to handle LAST_ACK event
 *
 * si: server_information
 *
 * entry: chisocketentry in order to get the tcp_data
 *
 * event: event to handle
 *
 * Returns: CHITCP_OK/CHITCP_ERROR
 */
int chitcpd_tcp_state_handle_LAST_ACK(serverinfo_t *si, chisocketentry_t *entry, tcp_event_type_t event)
{
    if (event == PACKET_ARRIVAL)
    {
        chilog(DEBUG, "PACKET_ARRIVAL");
        chitcpd_tcp_state_handle_PACKET_ARRIVAL(si, entry, event);
    }
    else if (event == TIMEOUT_RTX)
    {
        chilog(DEBUG, "TIMEOUT in LAST_ACK");
        chitcpd_tcp_state_handle_TIMEOUT_RTX(si, entry);
    }
    else if (event == TIMEOUT_PST)
    {
        chilog(DEBUG, "TIMEOUT_PST in LAST_ACK");
        chitcpd_tcp_state_handle_TIMEOUT_PST(si, entry);
    }
    else
        chilog(WARNING, "In LAST_ACK state, received unexpected event (%i).", event);

    return CHITCP_OK;
}
