#ifndef NG_PARSER_H
#define NG_PARSER_H
#include <string>
#include <map>
#include <ctime>
#include <vector>

extern "C"
{
#include <stdint.h>
}

#define HOST_SCHEDULED_DOWNTIME			1
#define HOST_NO_SCHEDULED_DOWNTIME		(1<<1)
#define HOST_STATE_ACKNOWLEDGED			(1<<2)
#define HOST_STATE_UNACKNOWLEDGED		(1<<3)
#define HOST_CHECKS_DISABLED			(1<<4)
#define HOST_CHECKS_ENABLED				(1<<5)
#define HOST_EVENT_HANDLER_DISABLED		(1<<6)
#define HOST_EVENT_HANDLER_ENABLED		(1<<7)
#define HOST_FLAP_DETECTION_DISABLED	(1<<8)
#define HOST_FLAP_DETECTION_ENABLED		(1<<9)
#define HOST_IS_FLAPPING				(1<<10)
#define HOST_IS_NOT_FLAPPING			(1<<11)
#define HOST_NOTIFICATIONS_DISABLED		(1<<12)
#define HOST_NOTIFICATIONS_ENABLED		(1<<13)
#define HOST_PASSIVE_CHECKS_DISABLED	(1<<14)
#define HOST_PASSIVE_CHECKS_ENABLED		(1<<15)
#define HOST_PASSIVE_CHECK				(1<<16)
#define HOST_ACTIVE_CHECK				(1<<17)
#define HOST_HARD_STATE					(1<<18)
#define HOST_SOFT_STATE					(1<<19)
#define HOST_ACTIVE_CHECKS_DISABLED		(1<<20)
#define HOST_ACTIVE_CHECKS_ENABLED		(1<<21)

#define HOST_PENDING					1
#define HOST_UP							2
#define HOST_DOWN						4
#define HOST_UNREACHABLE				8

#define SERVICE_SCHEDULED_DOWNTIME		1
#define SERVICE_NO_SCHEDULED_DOWNTIME	(1<<1)
#define SERVICE_STATE_ACKNOWLEDGED		(1<<2)
#define SERVICE_STATE_UNACKNOWLEDGED	(1<<3)
#define SERVICE_CHECKS_DISABLED			(1<<4)
#define SERVICE_CHECKS_ENABLED			(1<<5)
#define SERVICE_EVENT_HANDLER_DISABLED	(1<<6)
#define SERVICE_EVENT_HANDLER_ENABLED	(1<<7)
#define SERVICE_FLAP_DETECTION_ENABLED	(1<<8)
#define SERVICE_FLAP_DETECTION_DISABLED	(1<<9)
#define SERVICE_IS_FLAPPING				(1<<10)
#define SERVICE_IS_NOT_FLAPPING			(1<<11)
#define SERVICE_NOTIFICATIONS_DISABLED	(1<<12)
#define SERVICE_NOTIFICATIONS_ENABLED	(1<<13)
#define SERVICE_PASSIVE_CHECKS_DISABLED	(1<<14)
#define SERVICE_PASSIVE_CHECKS_ENABLED	(1<<15)
#define SERVICE_PASSIVE_CHECK			(1<<16)
#define SERVICE_ACTIVE_CHECK			(1<<17)
#define SERVICE_HARD_STATE				(1<<18)
#define SERVICE_SOFT_STATE				(1<<19)
#define SERVICE_ACTIVE_CHECKS_DISABLED	(1<<20)
#define SERVICE_ACTIVE_CHECKS_ENABLED	(1<<21)

#define SERVICE_PENDING					1
#define SERVICE_OK						2
#define SERVICE_WARNING					4
#define SERVICE_UNKNOWN					8
#define SERVICE_CRITICAL				16

#define C_HOSTSTATUS					1
#define C_SERVICESTATUS					2
#define C_PROGRAMSTATUS					3
#define C_INFO							4
#define C_UNKNOWN						5
#define C_SERVICECOMMENT				6
#define C_HOSTCOMMENT					7
#define C_SERVICEDOWNTIME				8
#define C_HOSTDOWNTIME					9

typedef struct {
	std::string	author;
	std::string comment;
	time_t		added;
} nagios_comment_t;

typedef struct {
	std::string	author;
	std::string comment;
	time_t		added;
	time_t		start_time;
	time_t		end_time;
	bool		fixed;
	time_t		duration;
} nagios_downtime_t;

typedef std::vector<nagios_comment_t> commentlist_t;

typedef struct {
	std::string 		hostname;
	uint32_t			flags;
	uint8_t				status;
	std::string 		output;
	time_t				last_state_change;
	uint32_t			last_state_diff;
	uint32_t			max_attempts;
	uint32_t			current_attempt;
	nagios_downtime_t	downtime;
	commentlist_t		comments;
	uint32_t			services;
	uint32_t			services_visible;
    std::map<std::string, std::string> extra;
} nagios_host_t;

typedef struct {
	uint32_t			flags;
	uint8_t				status;
	std::string 		hostname;
	std::string 		servicename;
	std::string 		output;
	time_t				last_state_change;
	uint32_t			last_state_diff;
	uint32_t			max_attempts;
	uint32_t			current_attempt;
	nagios_downtime_t	downtime;
	commentlist_t		comments;
	nagios_host_t		*hostdata;
    std::map<std::string, std::string> extra;
} nagios_service_t;

typedef std::map<std::string, nagios_host_t>		nagiosmap_host_t;
typedef std::map<std::string, nagios_service_t>		nagiosmap_service_t;

class NagiosParser
{
public:
	NagiosParser(const std::string&);
	~NagiosParser();
private:
	char *fbuffer;
	int flength;
	nagiosmap_host_t		hosts;
	nagiosmap_service_t		services;
	time_t					created;
	bool					is_nagios_running;
private:
	void parse();
	void do_json();
	static void do_cgi();
	void do_plaintext();
	void gc();
	void filter();
	void add_host(nagios_host_t*);
	void add_service(nagios_service_t*);
	void reset_host(nagios_host_t*);
	void reset_service(nagios_service_t*);
	void reset_comment(nagios_comment_t*);
	void reset_downtime(nagios_downtime_t*);
	void add_hostcomment(const std::string&, nagios_comment_t*);
	void add_servicecomment(const std::string&, const std::string&, nagios_comment_t*);
	void add_hostdowntime(const std::string&, nagios_downtime_t*);
	void add_servicedowntime(const std::string&, const std::string&, nagios_downtime_t*);
	bool is_host_alive(std::string);
public:
	void run();
};
#endif