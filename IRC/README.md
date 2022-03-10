#  Chirc Project

## Introduction 
In this project, we have implemented a simple [Internet Relay Chat (IRC)](https://datatracker.ietf.org/doc/html/rfc2810) server called chirc. IRC is one of the earliest network protocols for text messaging and multi-participant chatting. It remains a popular standard and still sees heavy use in certain communities, specially the open source software community  (such as Slack). Please see http://chi.cs.uchicago.edu/chirc/ for a specification (functions and how to test) of the chirc.

## Supported Command 

NICK

USER

QUIT

PRIVMSG (to clients & channels)

NOTICE (to clients & channels)

PING

PONG

MOTD

LUSERS

WHOIS

JOIN

PART

NAMES

LIST


## Correctness of Test

### assignment-1

```
Assignment 1
=========================================================================
Category                            Passed / Total       Score  / Points    
-------------------------------------------------------------------------
Basic Connection                    22     / 22          50.00  / 50.00     
-------------------------------------------------------------------------
                                                 TOTAL = 50.00  / 50        
=========================================================================

[100%] Built target assignment-1
```

### assignment-4
```
Assignment 4
=========================================================================
Category                            Passed / Total       Score  / Points    
-------------------------------------------------------------------------
Connection Registration             24     / 24          8.00   / 8.00      
PRIVMSG and NOTICE (between users)  14     / 14          7.00   / 7.00      
PING and PONG                       6      / 6           2.00   / 2.00      
LUSERS                              8      / 8           5.00   / 5.00      
WHOIS                               3      / 3           5.00   / 5.00      
JOIN                                6      / 6           5.00   / 5.00      
PRIVMSG and NOTICE (to channels)    6      / 6           5.00   / 5.00      
PART                                14     / 14          5.00   / 5.00      
Channel Operator                    7      / 7           3.00   / 3.00      
IRC Operator                        4      / 4           2.00   / 2.00      
LIST                                3      / 3           2.00   / 2.00      
ERR_UNKNOWN                         3      / 3           1.00   / 1.00      
-------------------------------------------------------------------------
                                                 TOTAL = 50.00  / 50        
=========================================================================

[100%] Built target assignment-4

```

## Notice

1. We are still using the “sdssplitlen” with "\r\n" to parse the command segments. There would be problems on my own Macbook when the total command characters sent to the sever from one client at a time is greater than the buffer size(512), no matter whether it is the situation the previous buffer ends with '\r', and the followed one starts with '\n'. And the characters in the join positions are not predictable due to diefferent OS. So it is still hard to avoid errors to parse with only '\r'. While on the Linux sever of CS department, there is no such problem.

## Reference

[uthash](https://troydhanson.github.io/uthash/)

[SDS library](https://github.com/antirez/sds)

[dispatch table sample codes](https://github.com/uchicago-cs/cmsc23320/tree/master/samples/dispatch_table)
