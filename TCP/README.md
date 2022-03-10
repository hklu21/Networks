# chiTCP - A simple, testable TCP stack

## Changes Pass Additional Tests:

- Changes are made to the **chitcpd_tcp_state_handle_TIMEOUT_PS** and **chitcp_update_send_buffer**in tcp.c file


=====================================
## COrrectness of Test and Grade Report
Please run with **./test-tcp --verbose** on linux machine.

```
./test-tcp

[----] Warning! The test `persist::slow_receiver_4096bytes` crashed during its setup or teardown.
[====] Synthesis: Tested: 82 | Passing: 82 | Failing: 0 | Crashing: 0 
```

```
Assignment 1
==============================================================================
Category                                 Passed / Total       Score  / Points    
------------------------------------------------------------------------------
3-way handshake                          2      / 2           20.00  / 20.00     
Connection tear-down                     3      / 3           10.00  / 10.00     
Data transfer                            27     / 27          20.00  / 20.00     
------------------------------------------------------------------------------
                                                      TOTAL = 50.00  / 50        
==============================================================================

Assignment 2
==============================================================================
Category                                 Passed / Total       Score  / Points    
------------------------------------------------------------------------------
Timer API                                19     / 19          10.00  / 10.00     
Retransmissions - 3-way handshake        3      / 3           5.00   / 5.00      
Retransmissions - Connection tear-down   2      / 2           5.00   / 5.00      
Retransmissions - Data transfer          13     / 13          15.00  / 15.00     
Persist timer                            6      / 6           5.00   / 5.00      
Out-of-order delivery                    4      / 4           10.00  / 10.00     
------------------------------------------------------------------------------
                                                      TOTAL = 50.00  / 50        
==============================================================================

Built target grade
```


The chiTCP documentation is available at http://chi.cs.uchicago.edu/chitcp/