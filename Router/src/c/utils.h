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

#ifndef SR_UTILS_H
#define SR_UTILS_H

#include "chirouter.h"

/* IP Protocol header fields */
#define ICMP_PROTO          (1)
#define TCP_PROTO           (6)
#define UDP_PROTO           (17)

/*
 * cksum - Computes a checksum
 *
 * Computes a 16-bit checksum that can be used in an IP or ICMP header.
 *
 * _data: Pointer to data to generate the checksum on
 *
 * len: Number of bytes of data
 *
 * Returns: 16-bit checksum
 *
 */
uint16_t cksum(const void *_data, int len);


/*
 * ethernet_addr_is_equal - Compares two MAC addresses
 *
 * addr1, addr2: Pointers to the MAC addresses. Assumed to be six bytes long.
 *
 * Returns: true if addr1 and addr2 are the same address, false otherwise.
 *
 */
bool ethernet_addr_is_equal(uint8_t *addr1, uint8_t *addr2);

/*
 * chirouter_send_icmp - Send icmp frame
 *
 * This function will get called every time when we want to send icmp frame
 * 
 * ctx: Router context
 *
 * frame: ICMP Ethernet frame to be sent
 * 
 * type: ICMP TYPE
 * 
 * code: ICMP CODE
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
int chirouter_send_icmp(chirouter_ctx_t *ctx, ethernet_frame_t *frame, uint8_t type,
                        uint8_t code);

/*
 * chirouter_send_arp - Send arp frame
 *
 * This function will get called every time when we want to send arp request/reply
 * 
 * ctx: Router context
 * 
 * eth_dst_mac: Destination mac address
 * 
 * out_interface: Destination out interface
 * 
 * arp_op: ARP_REQUEST or ARP_REPLY
 *
 * frame: ICMP Ethernet frame to be sent
 * 
 * tpa: Target IP Address
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
int chirouter_send_arp(chirouter_ctx_t *ctx, uint8_t *eth_dst_mac,
                       chirouter_interface_t *out_interface, uint16_t arp_op, uint32_t tpa);

#endif
