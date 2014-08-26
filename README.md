nagios2json
===========

Description
-----------
Nagios Status to JSON/Console.

- Everything should be automatic. It tries to find the nagios configuration and locate the status.dat and parse it.
  If something is wrong or you are using exotic nagios configuration, you can set the statusdat as a parameter.
- It works on Linux and BSD also.
- The parser is much faster then the built-in nagios parser. (Using the nagios cgi scripts the parsing takes 3+ seconds. With this It takes ~0.1s)

Why?
-----------
I am using this as a backend for my nagios aggregator front-end. So with this you can aggregate multiple nagios instances.

Dependencies
-----------
- make/gmake for makefile
- c++ compiler

Installation
-----------
- gmake / make
- copy the binary to the nagios cgi directory

Example
-----------
## Console:
    user@monitor1:~$ /mnt/data/nagios/sbin/nagios2json.cgi
    example1          | PING                 | CRITICAL |  12d 3h 11m 8s   | "PING CRITICAL - Packet loss = 100%"
    example2          | PORT-80              | CRITICAL |  18d 7h 44m 45s  | "Connection refused"

## HTTP
    user@monitor1:~$ curl http://monitor1/nagios/cgi-bin/nagios2json.cgi?servicestatustypes=28&serviceprops=...
    {
	"version":20110619,
	"running":1,
	"servertime":1409075862,
	"localtime":[1,7200],
	"created":1409075855,
	"data":
	[
	    {
	    "last_state_change": 1409075855,
	    "plugin_output":"PING CRITICAL - Packet loss = 100%",
	    "status":"WARNING",
	    "flags":1485146,
	    "hostname":"examplehost",
	    "service":"PING",
	    "host_alive":1,
	    "services_total":10,
	    "services_visible":1,
	    "duration":"0d 0h 0m 0s"
	    }
	]
    }

Options
-----------
## Console arguments (optional)
  -s, --status=[FILE]          nagios status.dat path
  -c, --cgi                    cgi mode
  -j, --json                   json output
  -t, --servicestatustypes     service status type (OK/CRIT/WARN)
  -p, --serviceprops           service props (ACK/MAINTENANCE)
  -n, --hostprops              host props (ACK/MAINTENANCE)

## parameter details (http GET / console)
 hostprops:
        #define HOST_SCHEDULED_DOWNTIME                 1
        #define HOST_NO_SCHEDULED_DOWNTIME              (1<<1)
        #define HOST_STATE_ACKNOWLEDGED                 (1<<2)
        #define HOST_STATE_UNACKNOWLEDGED               (1<<3)
        #define HOST_CHECKS_DISABLED                    (1<<4)
        #define HOST_CHECKS_ENABLED                             (1<<5)
        #define HOST_EVENT_HANDLER_DISABLED             (1<<6)
        #define HOST_EVENT_HANDLER_ENABLED              (1<<7)
        #define HOST_FLAP_DETECTION_DISABLED    (1<<8)
        #define HOST_FLAP_DETECTION_ENABLED             (1<<9)
        #define HOST_IS_FLAPPING                                (1<<10)
        #define HOST_IS_NOT_FLAPPING                    (1<<11)
        #define HOST_NOTIFICATIONS_DISABLED             (1<<12)
        #define HOST_NOTIFICATIONS_ENABLED              (1<<13)
        #define HOST_PASSIVE_CHECKS_DISABLED    (1<<14)
        #define HOST_PASSIVE_CHECKS_ENABLED             (1<<15)
        #define HOST_PASSIVE_CHECK                              (1<<16)
        #define HOST_ACTIVE_CHECK                               (1<<17)
        #define HOST_HARD_STATE                                 (1<<18)
        #define HOST_SOFT_STATE                                 (1<<19)
        #define HOST_ACTIVE_CHECKS_DISABLED             (1<<20)
        #define HOST_ACTIVE_CHECKS_ENABLED              (1<<21)

  serviceprops:
        #define SERVICE_SCHEDULED_DOWNTIME              1
        #define SERVICE_NO_SCHEDULED_DOWNTIME   (1<<1)
        #define SERVICE_STATE_ACKNOWLEDGED              (1<<2)
        #define SERVICE_STATE_UNACKNOWLEDGED    (1<<3)
        #define SERVICE_CHECKS_DISABLED                 (1<<4)
        #define SERVICE_CHECKS_ENABLED                  (1<<5)
        #define SERVICE_EVENT_HANDLER_DISABLED  (1<<6)
        #define SERVICE_EVENT_HANDLER_ENABLED   (1<<7)
        #define SERVICE_FLAP_DETECTION_ENABLED  (1<<8)
        #define SERVICE_FLAP_DETECTION_DISABLED (1<<9)
        #define SERVICE_IS_FLAPPING                             (1<<10)
        #define SERVICE_IS_NOT_FLAPPING                 (1<<11)
        #define SERVICE_NOTIFICATIONS_DISABLED  (1<<12)
        #define SERVICE_NOTIFICATIONS_ENABLED   (1<<13)
        #define SERVICE_PASSIVE_CHECKS_DISABLED (1<<14)
        #define SERVICE_PASSIVE_CHECKS_ENABLED  (1<<15)
        #define SERVICE_PASSIVE_CHECK                   (1<<16)
        #define SERVICE_ACTIVE_CHECK                    (1<<17)
        #define SERVICE_HARD_STATE                              (1<<18)
        #define SERVICE_SOFT_STATE                              (1<<19)
        #define SERVICE_ACTIVE_CHECKS_DISABLED  (1<<20)
        #define SERVICE_ACTIVE_CHECKS_ENABLED   (1<<21)

 servicestatustypes:
        #define SERVICE_PENDING                                 1
        #define SERVICE_OK                                              2
        #define SERVICE_WARNING                                 4
        #define SERVICE_UNKNOWN                                 8
        #define SERVICE_CRITICAL                                16

History
-----------
I created this in 2011 when I had to watch my sites 0-24h on a single monitor :).
Later I started using this at the company I am working for. We are using it with 7 nagios instances and like 3k+ physical nodes and ~61k service.

Screenshots
-----------
![Screenshot1](https://raw.githubusercontent.com/macskas/nagios2json/master/github-static/nagios1.png "Screenshot1")
![Screenshot2](https://raw.githubusercontent.com/macskas/nagios2json/master/github-static/nagios2.png "Screenshot2")
