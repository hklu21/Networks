/*
 *  chirouter - A simple, testable IP router
 *
 *  This module contains the actual functionality of the router.
 *  When a router receives an Ethernet frame, it is handled by
 *  the chirouter_process_ethernet_frame() function.
 *
 */

/*
 * This project is based on the Simple Router assignment included in the
 * Mininet project (https://github.com/mininet/mininet/wiki/Simple-Router) which,
 * in turn, is based on a programming assignment developed at Stanford
 * (http://www.scs.stanford.edu/09au-cs144/lab/router.html)
 *
 * While most of the code for chirouter has been written from scratch, some
 * of the original Stanford code is still present in some places and, whenever
 * possible, we have tried to provide the exact attribution for such code.
 * Any omissions are not intentional and will be gladly corrected if
 * you contact us at borja@cs.uchicago.edu
 */

/*
 *  Copyright (c) 2016-2018, The University of Chicago
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
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
 *    software without specific prior written permission.
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
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "chirouter.h"
#include "arp.h"
#include "utils.h"
#include "utlist.h"

/* process_arp: process arp request/reply
 *
 * ctx: Router context
 *
 * frame: Inbound Ethernet frame
 *
 * Returns:
 *   0 on success,
 *
 *   1 if a non-critical error happens
 *
 *   -1 if a critical error happens
 *
 *   Note: In the event of a critical error, the entire router will shut down and exit.
 *         You should only return -1 for issues that would prevent the router from
 *         continuing to run normally. Return 1 to indicate that the frame could
 *         not be processed, but that subsequent frames can continue to be processed.
 */
int process_arp(chirouter_ctx_t *ctx, ethernet_frame_t *frame);

/* process_ipv4: process ipv4 request/reply
 *
 * ctx: Router context
 *
 * frame: Inbound Ethernet frame
 *
 * Returns:
 *   0 on success,
 *
 *   1 if a non-critical error happens
 *
 *   -1 if a critical error happens
 *
 *   Note: In the event of a critical error, the entire router will shut down and exit.
 *         You should only return -1 for issues that would prevent the router from
 *         continuing to run normally. Return 1 to indicate that the frame could
 *         not be processed, but that subsequent frames can continue to be processed.
 */
int process_ipv4(chirouter_ctx_t *ctx, ethernet_frame_t *frame);

/* process_ipv6: process ipv6 request/reply (not yet implemented)
 *
 * ctx: Router context
 *
 * frame: Inbound Ethernet frame
 *
 * Returns:
 *   0 on success,
 *
 *   1 if a non-critical error happens
 *
 *   -1 if a critical error happens
 *
 *   Note: In the event of a critical error, the entire router will shut down and exit.
 *         You should only return -1 for issues that would prevent the router from
 *         continuing to run normally. Return 1 to indicate that the frame could
 *         not be processed, but that subsequent frames can continue to be processed.
 */
int process_ipv6(chirouter_ctx_t *ctx, ethernet_frame_t *frame);

/* chirouter_forward_ip: IP forwarding to matching ports
 *
 * ctx: Router context
 *
 * frame: Inbound Ethernet frame
 *
 * gateway: gateway of the matching ip
 *
 * frame_out_iface: outbound frame interface
 *
 * Returns:
 *   0 on success,
 *
 *   1 if a non-critical error happens
 *
 *   -1 if a critical error happens
 *
 *   Note: In the event of a critical error, the entire router will shut down and exit.
 *         You should only return -1 for issues that would prevent the router from
 *         continuing to run normally. Return 1 to indicate that the frame could
 *         not be processed, but that subsequent frames can continue to be processed.
 */
int chirouter_forward_ip(chirouter_ctx_t *ctx,
                         ethernet_frame_t *frame,
                         struct in_addr gateway,
                         chirouter_interface_t *frame_out_iface);

/*
 * chirouter_process_ethernet_frame - Process a single inbound Ethernet frame
 *
 * This function will get called every time an Ethernet frame is received by
 * a router. This function receives the router context for the router that
 * received the frame, and the inbound frame (the ethernet_frame_t struct
 * contains a pointer to the interface where the frame was received).
 * Take into account that the chirouter code will free the frame after this
 * function returns so, if you need to persist a frame (e.g., because you're
 * adding it to a list of withheld frames in the pending ARP request list)
 * you must make a deep copy of the frame.
 *
 * chirouter can manage multiple routers at once, but does so in a single
 * thread. i.e., it is guaranteed that this function is always called
 * sequentially, and that there will not be concurrent calls to this
 * function. If two routers receive Ethernet frames "at the same time",
 * they will be ordered arbitrarily and processed sequentially, not
 * concurrently (and with each call receiving a different router context)
 *
 * ctx: Router context
 *
 * frame: Inbound Ethernet frame
 *
 * Returns:
 *   0 on success,
 *
 *   1 if a non-critical error happens
 *
 *   -1 if a critical error happens
 *
 *   Note: In the event of a critical error, the entire router will shut down and exit.
 *         You should only return -1 for issues that would prevent the router from
 *         continuing to run normally. Return 1 to indicate that the frame could
 *         not be processed, but that subsequent frames can continue to be processed.
 */
int chirouter_process_ethernet_frame(chirouter_ctx_t *ctx,
                                     ethernet_frame_t *frame)
{
    ethhdr_t *header = (ethhdr_t *)frame->raw;
    uint16_t ethertype = ntohs(header->type);

    if (ethertype == ETHERTYPE_IP)
    {
        process_ipv4(ctx, frame);
    }
    else if (ethertype == ETHERTYPE_ARP)
    {
        process_arp(ctx, frame);
    }
    else if (ethertype == ETHERTYPE_IPV6)
    {
        process_ipv6(ctx, frame);
    }
    else
    {
        chilog(ERROR, "Ethertype is not acceptable.");
        return 1;
    }

    return 0;
}

/*
 * Function description is at the most front of router.c
 */
int process_arp(chirouter_ctx_t *ctx, ethernet_frame_t *frame)
{
    /* Accessing the Ethernet header */
    ethhdr_t *eth_header = (ethhdr_t *)(frame->raw);
    /* Accessing an ARP message */
    arp_packet_t *arp = (arp_packet_t *)(frame->raw + sizeof(ethhdr_t));

    if (ntohs(arp->op) == ARP_OP_REQUEST)
    {
        bool arp_ip_exist = false;
        /* Decide if ARP request is for that interface’s IP address */
        for (int i = 0; i < ctx->num_interfaces; i++)
        {
            if (arp->tpa == ctx->interfaces[i].ip.s_addr)
            {
                arp_ip_exist = true;
                /* Send back an ARP reply to the host that sent the request. */
                chirouter_send_arp(ctx, eth_header->src,
                                   &ctx->interfaces[i],
                                   ARP_OP_REPLY, arp->tpa);
            }
        }

        /* Send icmp host_unreachable if not matching in arp */
        if (!arp_ip_exist)
        {
            chirouter_send_icmp(ctx, frame,
                                ICMPTYPE_DEST_UNREACHABLE,
                                ICMPCODE_DEST_HOST_UNREACHABLE);
        }
    }
    else if (ntohs(arp->op) == ARP_OP_REPLY)
    {
        struct in_addr *query_ip = calloc(1, sizeof(struct in_addr));
        query_ip->s_addr = arp->spa;
        uint8_t query_mac[ETHER_ADDR_LEN];
        memcpy(query_mac, arp->sha, ETHER_ADDR_LEN);

        /* Add to ARP cache */
        pthread_mutex_lock(&ctx->lock_arp);
        chirouter_arp_cache_add(ctx, query_ip, query_mac);
        pthread_mutex_unlock(&(ctx->lock_arp));

        pthread_mutex_lock(&(ctx->lock_arp));
        chirouter_pending_arp_req_t *pending_arp =
            chirouter_arp_pending_req_lookup(ctx, query_ip);

        if (pending_arp == NULL)
        {
            pthread_mutex_unlock(&ctx->lock_arp);
            return 0;
        }

        /* Forward pending ARP request and withheld frames */
        withheld_frame_t *withheld_frame;

        DL_FOREACH(pending_arp->withheld_frames, withheld_frame)
        {
            iphdr_t *ip_hdr = (iphdr_t *)(withheld_frame->frame->raw +
                                          sizeof(ethhdr_t));
            if (ip_hdr->ttl == 1)
            {
                chirouter_send_icmp(ctx, withheld_frame->frame,
                                    ICMPTYPE_TIME_EXCEEDED,
                                    0);
            }
            else
            {
                ethhdr_t *withheld_eth_header =
                    (ethhdr_t *)(withheld_frame->frame->raw);
                memcpy(withheld_eth_header->dst,
                       query_mac, ETHER_ADDR_LEN);
                memcpy(withheld_eth_header->src,
                       pending_arp->out_interface->mac, ETHER_ADDR_LEN);

                ip_hdr->ttl -= 1;
                ip_hdr->cksum = 0;
                ip_hdr->cksum = cksum(ip_hdr, sizeof(iphdr_t));
                uint8_t *frame = withheld_frame->frame->raw;
                size_t frame_len = withheld_frame->frame->length;

                chirouter_send_frame(ctx,
                                     pending_arp->out_interface,
                                     frame, frame_len);
            }
        }

        /* Remove the pending ARP request from the pending ARP request list. */
        chirouter_arp_pending_req_free_frames(pending_arp);
        DL_DELETE(ctx->pending_arp_reqs, pending_arp);
        free(pending_arp);
        
        pthread_mutex_unlock(&ctx->lock_arp);
    }
    else
    {
        chilog(ERROR, "Arptype is not acceptable.");
        return 1;
    }

    return 0;
}

/*
 * Function description is at the most front of router.c
 */
int process_ipv4(chirouter_ctx_t *ctx, ethernet_frame_t *frame)
{
    /* Accessing an headers */
    ethhdr_t *frame_ethhdr = (ethhdr_t *)(frame->raw);
    iphdr_t *frame_iphdr = (iphdr_t *)(frame->raw + sizeof(ethhdr_t));
    icmp_packet_t *frame_icmp;

    bool router_is_dst = false;
    bool host_unreachable = false;
    bool net_unreachable = false;
    chirouter_interface_t *frame_iface = NULL;

    /* Check whether the IP datagram can be forwarded */
    for (int i = 0; i < ctx->num_interfaces; i++)
    {
        chirouter_interface_t *iface = &(ctx->interfaces[i]);
        if (ethernet_addr_is_equal(frame_ethhdr->dst, iface->mac))
        {
            frame_iface = iface;
        }

        if (frame_iphdr->dst == iface->ip.s_addr)
        {
            router_is_dst = true;
            /* If router receives an IP datagram that it can forward,
             * but no host on the target network replies to an ARP
             * request, send an ICMP Host Unreachable reply.
             */
            if (frame_iphdr->dst != frame->in_interface->ip.s_addr)
            {
                host_unreachable = true;
            }
        }
    }

    host_unreachable = host_unreachable && router_is_dst;

    if (frame_iface == NULL)
    {
        chilog(ERROR, "The packet does not match any interface of MAC.");
        return 1;
    }

    bool tcp_udp = (frame_iphdr->proto == TCP_PROTO ||
                    frame_iphdr->proto == UDP_PROTO);
    tcp_udp = tcp_udp && router_is_dst;

    bool is_icmp = (frame_iphdr->proto == ICMP_PROTO);
    if (is_icmp)
    {
        frame_icmp = (icmp_packet_t *)(frame->raw +
                                       sizeof(ethhdr_t) +
                                       sizeof(iphdr_t));
    }

    in_addr_t forward_dst_max = 0;
    chirouter_interface_t *frame_out_iface = NULL;
    struct in_addr gateway;

    /* Loop routing table */
    if (!router_is_dst)
    {
        for (int i = 0; i < ctx->num_rtable_entries; i++)
        {
            int forward_candidate = (ctx->routing_table[i].mask.s_addr) &
                                    (ctx->routing_table[i].dest.s_addr);
            if (forward_candidate == ((frame_iphdr->dst) &
                                      (ctx->routing_table[i].mask.s_addr)))
            {
                if (ntohs(ctx->routing_table[i].mask.s_addr) >=
                    forward_dst_max)
                {
                    frame_out_iface = ctx->routing_table[i].interface;
                    forward_dst_max = ntohs(ctx->routing_table[i].mask.s_addr);
                    gateway.s_addr = ctx->routing_table[i].gw.s_addr;
                }
            }
        }

        /* If router receives an IP datagram that it cannot forward
         * according to its routing table,
         * send an ICMP Network Unreachable reply.
         */
        if (forward_dst_max == 0 && frame_out_iface == NULL)
        {
            net_unreachable = true;
        }
    }

    /* Router receives certain IP datagrams directed to one of its IP addresses,
     * send an ICMP reply:
     * The IP address is not the one the interface
     * where the datagram was received, send an ICMP Host Unreachable message.
     * Receives a TCP/UDP packet directed to one of its IP addresses,
     * send an ICMP Port Unreachable.
     * The IP datagram’s TTL is 1, send a Time Exceeded message.
     * (Only if the IP datagram can be forwarded and its MAC address available)
     * Receives an ICMP Echo Request, send an ICMP Echo Reply.
     */
    if (host_unreachable || is_icmp ||
        tcp_udp || net_unreachable)
    {
        /* IP datagram can be forwarded, sending a Destination Unreachable reply */
        if (host_unreachable || net_unreachable || tcp_udp)
        {
            int icmp_code = 0;

            if (host_unreachable)
            {
                icmp_code = ICMPCODE_DEST_HOST_UNREACHABLE;
            }
            else if (net_unreachable)
            {
                icmp_code = ICMPCODE_DEST_NET_UNREACHABLE;
            }
            else if (tcp_udp)
            {
                icmp_code = ICMPCODE_DEST_PORT_UNREACHABLE;
            }

            chirouter_send_icmp(ctx, frame,
                                ICMPTYPE_DEST_UNREACHABLE, icmp_code);

            return 0;
        }

        /* If the TTL of the datagram is 1, send an ICMP Time Exceeded reply. */
        if (frame_iphdr->ttl == 1 && router_is_dst)
        {
            /* ICMP time exceeded */
            chilog(DEBUG, "[TIME EXCEEDED TTL = 1]");
            chirouter_send_icmp(ctx, frame,
                                ICMPTYPE_TIME_EXCEEDED, 0);

            return 0;
        }

        if (is_icmp && router_is_dst)
        {
            if (frame_icmp->type == ICMPTYPE_ECHO_REQUEST)
            {
                chilog(DEBUG, "host_unreachable: %d,router_is_dst:%d ",
                       host_unreachable, router_is_dst);
                chirouter_send_icmp(ctx, frame,
                                    ICMPTYPE_ECHO_REPLY, 0);
            }

            return 0;
        }
    }

    return chirouter_forward_ip(ctx, frame, gateway, frame_out_iface);
}

/*
 * Function description is at the most front of router.c
 */
int process_ipv6(chirouter_ctx_t *ctx, ethernet_frame_t *frame)
{
    return 0;
}

/*
 * Function description is at the most front of router.c
 */
int chirouter_forward_ip(chirouter_ctx_t *ctx,
                         ethernet_frame_t *frame,
                         struct in_addr gateway,
                         chirouter_interface_t *frame_out_iface)
{
    ethhdr_t *frame_ethhdr = (ethhdr_t *)(frame->raw);
    iphdr_t *frame_iphdr = (iphdr_t *)(frame->raw + sizeof(ethhdr_t));

    /* Process IP datagram */
    struct in_addr *forward_dst = calloc(1, sizeof(struct in_addr));
    forward_dst->s_addr = frame_iphdr->dst;
    if (gateway.s_addr != 0)
    {
        forward_dst->s_addr = gateway.s_addr;
    }

    /* Check ARP cache for forward_dst :chirouter_arp_cache_lookup */
    pthread_mutex_lock(&ctx->lock_arp);
    chirouter_arpcache_entry_t *arp_entry =
        chirouter_arp_cache_lookup(ctx, forward_dst);
    pthread_mutex_unlock(&ctx->lock_arp);

    /* 1. If not exists,
     * chirouter_arp_pending_req_add,
     * chirouter_send_arp
     */
    if (arp_entry == NULL)
    {
        chirouter_send_arp(ctx, frame_ethhdr->dst,
                           frame_out_iface,
                           ARP_OP_REQUEST,
                           forward_dst->s_addr);

        pthread_mutex_lock(&ctx->lock_arp);
        chirouter_pending_arp_req_t *pend_arp =
            chirouter_arp_pending_req_lookup(ctx, forward_dst);

        if (!pend_arp)
        {
            pend_arp = chirouter_arp_pending_req_add(ctx,
                                                     forward_dst,
                                                     frame_out_iface);
        }

        chirouter_arp_pending_req_add_frame(ctx, pend_arp, frame);
        pthread_mutex_unlock(&ctx->lock_arp);

        return 0;
    }

    /* 2. If exists, chirouter_send_frame */
    if (frame_iphdr->ttl == 1)
    {
        /* ICMP time exceeded */
        chilog(DEBUG, "[TIME EXCEEDED TTL = 1]");
        chirouter_send_icmp(ctx, frame,
                            ICMPTYPE_TIME_EXCEEDED, 0);

        return 0;
    }

    memcpy(frame_ethhdr->src, frame_out_iface->mac, ETHER_ADDR_LEN);
    memcpy(frame_ethhdr->dst, arp_entry->mac, ETHER_ADDR_LEN);

    /* Decrement the TTL by 1 */
    frame_iphdr->ttl -= 1;
    /* Recompute the packet checksum over the modified header. */
    frame_iphdr->cksum = 0;
    frame_iphdr->cksum = cksum(frame_iphdr, sizeof(iphdr_t));

    chilog_ip(DEBUG, frame_iphdr, LOG_OUTBOUND);

    return chirouter_send_frame(ctx, frame_out_iface,
                                frame->raw, frame->length);
}