/*
 *  chirouter - A simple, testable IP router
 *
 *  This module contains miscellaneous helper functions.
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
 *
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "protocols/ethernet.h"
#include "utils.h"

/* See utils.h */
uint16_t cksum (const void *_data, int len)
{
      const uint8_t *data = _data;
      uint32_t sum;

      for (sum = 0;len >= 2; data += 2, len -= 2)
      {
        sum += data[0] << 8 | data[1];
      }

      if (len > 0)
      {
        sum += data[0] << 8;
      }

      while (sum > 0xffff)
      {
        sum = (sum >> 16) + (sum & 0xffff);
      }

      sum = htons (~sum);

      return sum ? sum : 0xffff;
}

/* See utils.h */
bool ethernet_addr_is_equal(uint8_t *addr1, uint8_t *addr2)
{
    for (int i=0; i<ETHER_ADDR_LEN; i++)
    {
        if(addr1[i] != addr2[i])
            return false;
    }
    return true;
}

/* See utils.h */
int chirouter_send_icmp(chirouter_ctx_t *ctx,
                        ethernet_frame_t *frame,
                        uint8_t type, uint8_t code)
{
    ethhdr_t *frame_ethhdr = (ethhdr_t *)frame->raw;
    iphdr_t *frame_iphdr = (iphdr_t *)(frame->raw +
                                       sizeof(ethhdr_t));
    icmp_packet_t *icmp = (icmp_packet_t *)(frame->raw +
                                            sizeof(ethhdr_t) +
                                            sizeof(iphdr_t));
    int payload_len;

    if (type == ICMPTYPE_ECHO_REPLY ||
        type == ICMPTYPE_ECHO_REQUEST)
    {
        payload_len = ntohs(frame_iphdr->len) -
                      sizeof(iphdr_t) - ICMP_HDR_SIZE;
    }
    else
    {
        payload_len = sizeof(iphdr_t) + 8;
    }

    int reply_len = sizeof(ethhdr_t) + sizeof(iphdr_t) +
                    ICMP_HDR_SIZE + payload_len;
    uint8_t reply[reply_len];
    memset(reply, 0, reply_len);

    ethhdr_t *reply_eth = (ethhdr_t *)reply;
    memcpy(reply_eth->dst,
           frame_ethhdr->src, ETHER_ADDR_LEN);
    memcpy(reply_eth->src,
           frame->in_interface->mac, ETHER_ADDR_LEN);
    reply_eth->type = htons(ETHERTYPE_IP);

    iphdr_t *reply_ip = (iphdr_t *)(reply + sizeof(ethhdr_t));
    int reply_ip_len = sizeof(iphdr_t) +
                       ICMP_HDR_SIZE + payload_len;
    reply_ip->tos = 0;
    reply_ip->cksum = 0;
    reply_ip->len = htons(reply_ip_len);
    reply_ip->id = htons(0);
    reply_ip->off = htons(0);
    reply_ip->ttl = 64;
    reply_ip->proto = ICMP_PROTO;
    reply_ip->version = 4;
    reply_ip->ihl = 5;

    memcpy(&reply_ip->src,
           &frame->in_interface->ip.s_addr,
           IPV4_ADDR_LEN);
    memcpy(&reply_ip->dst,
           &frame_iphdr->src,
           IPV4_ADDR_LEN);
    reply_ip->cksum = cksum(reply_ip, sizeof(iphdr_t));

    icmp_packet_t *reply_icmp = (icmp_packet_t *)(reply +
                                                  sizeof(ethhdr_t) +
                                                  sizeof(iphdr_t));
    reply_icmp->code = code;
    reply_icmp->type = type;
    reply_icmp->chksum = 0;

    if (type == ICMPTYPE_ECHO_REQUEST ||
        type == ICMPTYPE_ECHO_REPLY)
    {
        if (code == 0)
        {
            reply_icmp->echo.identifier = icmp->echo.identifier;
            reply_icmp->echo.seq_num = icmp->echo.seq_num;
            memcpy(reply_icmp->echo.payload,
                   icmp->echo.payload,
                   payload_len);
        }
    }
    else if (type == ICMPTYPE_DEST_UNREACHABLE)
    {
        memcpy(reply_icmp->dest_unreachable.payload,
               frame_iphdr,
               payload_len);
    }
    else
    {
        memcpy(reply_icmp->time_exceeded.payload,
               frame_iphdr,
               payload_len);
    }
    reply_icmp->chksum = cksum(reply_icmp,
                               ICMP_HDR_SIZE + payload_len);

    chilog(DEBUG, "Sending ICMP packet");
    chilog_ip(DEBUG, reply_ip, LOG_OUTBOUND);
    chilog_icmp(DEBUG, reply_icmp, LOG_OUTBOUND);

    return chirouter_send_frame(ctx, frame->in_interface,
                                reply, reply_len);
}

/* See utils.h */
int chirouter_send_arp(chirouter_ctx_t *ctx, uint8_t *eth_dst_mac,
                       chirouter_interface_t *out_interface,
                       uint16_t arp_op, uint32_t tpa)
{
    int reply_len = sizeof(ethhdr_t) + (sizeof(arp_packet_t));
    uint8_t reply[reply_len];
    memset(reply, 0, reply_len);

    ethhdr_t *reply_ether_hdr = (ethhdr_t *)reply;

    memcpy(reply_ether_hdr->src, out_interface->mac, ETHER_ADDR_LEN);
    reply_ether_hdr->type = htons(ETHERTYPE_ARP);

    arp_packet_t *reply_arp = (arp_packet_t *)(reply +
                                               sizeof(ethhdr_t));
    reply_arp->hrd = htons(ARP_HRD_ETHERNET);
    reply_arp->pro = htons(ETHERTYPE_IP);
    reply_arp->hln = ETHER_ADDR_LEN;
    reply_arp->pln = IPV4_ADDR_LEN;
    reply_arp->op = htons(arp_op);
    reply_arp->spa = out_interface->ip.s_addr;
    reply_arp->tpa = tpa;
    memcpy(reply_arp->sha, out_interface->mac, ETHER_ADDR_LEN);

    if (arp_op == ARP_OP_REQUEST)
    {
        memcpy(reply_ether_hdr->dst,
               "\xFF\xFF\xFF\xFF\xFF\xFF",
               ETHER_ADDR_LEN);
        memcpy(reply_arp->tha,
               "\x00\x00\x00\x00\x00\x00",
               ETHER_ADDR_LEN);
    }
    else if (arp_op == ARP_OP_REPLY)
    {
        memcpy(reply_ether_hdr->dst, eth_dst_mac, ETHER_ADDR_LEN);
        memcpy(reply_arp->tha, eth_dst_mac, ETHER_ADDR_LEN);
    }
    else
    {
        chilog(ERROR, "INVALID ARP OP CODE.");
        return 1;
    }

    return chirouter_send_frame(ctx, out_interface,
                                reply, reply_len);
}
