#include <iostream>
#include <cstdio>
#include <fstream>
#include <cstring>
#include <string>
#include <cctype>
#include <sstream>
#include <cstdlib>

extern "C" {
#include <signal.h>
#include <sys/stat.h>
}

#include "ng_config.h"
#include "ng_parser.h"

static std::string escapeJsonString(const std::string& input) {
    std::ostringstream ss;
    for (std::string::const_iterator iter = input.begin(); iter != input.end(); iter++) {
        switch (*iter) {
            case '\\': ss << "\\\\"; break;
            case '"': ss << "\\\""; break;
                //case '/': ss << "\\/"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default: ss << *iter; break;
        }
    }
    return ss.str();
}

static void format_date(const uint32_t *diff, std::string *buffer)
{
    static char tbuffer[512];
    static uint32_t day, hour, min;
    day = hour = min = 0;
    memset(tbuffer,0,512);
    static uint32_t seconds;
    seconds = *diff;
    if (seconds > 86400) {
        day = seconds/86400;
        seconds %= 86400;
    }
    if (seconds > 3600) {
        hour = seconds/3600;
        seconds %= 3600;
    }
    if (seconds > 60) {
        min = seconds/60;
        seconds %= 60;
    }
    snprintf(tbuffer, 512, "%dd %dh %dm %ds", day, hour, min, seconds);
    buffer->assign(tbuffer);
}

static void format_status_service(const uint8_t *st, std::string *buffer)
{
    if (*st & SERVICE_OK) {
        buffer->assign("OK");
    } else if (*st & SERVICE_WARNING) {
        buffer->assign("WARNING");
    } else if (*st & SERVICE_CRITICAL) {
        buffer->assign("CRITICAL");
    } else if (*st & SERVICE_UNKNOWN) {
        buffer->assign("UNKNOWN");
    } else if (*st & SERVICE_PENDING) {
        buffer->assign("PENDING");
    } else {
        buffer->assign("-");
    }

}

NagiosParser::NagiosParser(const std::string& filepath)
{
    this->is_nagios_running = false;
    this->created = 0;
    this->fbuffer = nullptr;
    int length;
    std::ifstream filestr;
    filestr.open(filepath.c_str(), std::ifstream::in|std::ifstream::binary);
    if (!filestr.is_open())
    {
        std::cerr << "Error: statusfile error (" << filepath << ")." << std::endl;
        exit(1);
    }
    filestr.seekg(0, std::ios::end);
    length = (int)filestr.tellg();
    filestr.seekg(0, std::ios::beg);
    this->fbuffer = new char[length];
    this->flength = length;
    filestr.read(this->fbuffer, length);
    filestr.close();

}

NagiosParser::~NagiosParser()
{
    this->gc();
}

void NagiosParser::gc()
{
    if (this->fbuffer != nullptr)
        delete [] this->fbuffer;
    this->fbuffer = nullptr;
}

void NagiosParser::parse()
{
    int					linesize = 0;
    int					read_size = 0;
    int					rc = 0;
    nagios_host_t		hostdata;
    nagios_service_t	servicedata;
    int					len_helper = 0;
    int					category = 0;
    int					i = 0, e = 0;
    int					value_length = 0, key_length = 0;
    char				*buffer_current = this->fbuffer;
    char				*buffer_key = nullptr, *buffer_value = nullptr;
    char				*eq_helper = nullptr;
    time_t				now = time(nullptr);
    pid_t				nagiospid = 0;
    nagios_comment_t	nagios_comment;
    nagios_downtime_t	nagios_downtime;
    std::string			hostname, servicename;
    struct				stat st;
    bool				check_inner = false;
    char				filenamebuffer[512];
    memset(filenamebuffer,0,512);

    this->reset_service(&servicedata);
    this->reset_host(&hostdata);
    this->reset_downtime(&nagios_downtime);
    this->reset_comment(&nagios_comment);

    char *buffer_newline = nullptr;

    while (1)
    {
        buffer_newline = strchr(buffer_current, '\n');
        if (buffer_newline == nullptr)
            break;
        *buffer_newline = 0;
        linesize = buffer_newline-buffer_current;
        check_inner = true;
        if (linesize > 0 && isgraph(buffer_current[0])) {
            check_inner = false;
            if (linesize > 10 && strncmp(buffer_current, "hoststatus",		10) == 0) {
                category = C_HOSTSTATUS;
            } else if (linesize > 13 && strncmp(buffer_current, "servicestatus",	13) == 0) {
                category = C_SERVICESTATUS;
            } else if (linesize > 13 && strncmp(buffer_current, "programstatus",	13) == 0) {
                category = C_PROGRAMSTATUS;
            } else if (linesize >  4 && strncmp(buffer_current, "info",				 4) == 0) {
                category = C_INFO;
            } else if (linesize > 15 && strncmp(buffer_current, "servicedowntime",	15) == 0) {
                category = C_SERVICEDOWNTIME;
            } else if (linesize > 12 && strncmp(buffer_current, "hostdowntime",		12) == 0) {
                category = C_HOSTDOWNTIME;
            } else if (linesize > 14 && strncmp(buffer_current, "servicecomment",	14) == 0) {
                category = C_SERVICECOMMENT;
            } else if (linesize > 11 && strncmp(buffer_current, "hostcomment",		11) == 0) {
                category = C_HOSTCOMMENT;
            } else {
                category = C_UNKNOWN;
            }
        } else if (linesize > 0 && buffer_current[linesize-1] == '}') {
            len_helper = 0;
            for (i=0, e=linesize-1; i<e; ++i)
            {
                if (isgraph(buffer_current[i]))
                {
                    i=e;
                    len_helper = 1;
                }
            }
            if (len_helper == 0) {
                check_inner = false;
                {
                    // HARD STATE
                    if ((hostdata.flags & (HOST_PASSIVE_CHECKS_DISABLED|HOST_ACTIVE_CHECKS_DISABLED)) != 0)
                        hostdata.flags |= HOST_CHECKS_DISABLED;
                    else
                        hostdata.flags |= HOST_CHECKS_ENABLED;
                    if ((servicedata.flags & (SERVICE_PASSIVE_CHECKS_DISABLED|SERVICE_ACTIVE_CHECKS_DISABLED)) != 0)
                        servicedata.flags |= SERVICE_CHECKS_DISABLED;
                    else
                        servicedata.flags |= SERVICE_CHECKS_ENABLED;

                    // CATEGORY END.
                    if (category == C_HOSTSTATUS) {
                        this->add_host(&hostdata);
                        this->reset_host(&hostdata);
                    } else if (category == C_SERVICESTATUS) {
                        this->add_service(&servicedata);
                        this->reset_service(&servicedata);
                    } else if (category == C_HOSTCOMMENT) {
                        this->add_hostcomment(hostname, &nagios_comment);
                        this->reset_comment(&nagios_comment);
                    } else if (category == C_SERVICECOMMENT) {
                        this->add_servicecomment(hostname, servicename, &nagios_comment);
                        this->reset_comment(&nagios_comment);
                    } else if (category == C_HOSTDOWNTIME) {
                        this->add_hostdowntime(hostname, &nagios_downtime);
                        this->reset_downtime(&nagios_downtime);
                    } else if (category == C_SERVICEDOWNTIME) {
                        this->add_servicedowntime(hostname, servicename, &nagios_downtime);
                        this->reset_downtime(&nagios_downtime);
                    }
                    // DO THE TMP WRITES
                }
            }
        }
        if (check_inner) {
            eq_helper = strchr(buffer_current, '=');
            if (eq_helper == nullptr)
                goto skip_line;
            *eq_helper = 0;
            len_helper = eq_helper-buffer_current;
            buffer_key = buffer_current;
            for (i=0; i<len_helper; i++)
            {
                if (isspace(buffer_current[i]))
                    buffer_key++;
                else
                    i=len_helper;
            }
            key_length		= eq_helper-buffer_key;
            value_length	= buffer_newline-eq_helper-1;
            len_helper		= value_length;
            buffer_value	= eq_helper+1;
            //printf("'%s' (%d) - '%s' (%d)\n", buffer_key, key_length, buffer_value, value_length);
            //check_helper = strtoull(tmpstring2.c_str(), NULL, 10);
            switch (category)
            {
                case C_HOSTSTATUS:
                    if (key_length == 11 && strncmp(buffer_key, "is_flapping", 11) == 0) {
                        if (buffer_value[0] == '1')
                            hostdata.flags |= HOST_IS_FLAPPING;
                        else
                            hostdata.flags |= HOST_IS_NOT_FLAPPING;
                    } else if (key_length == 22 && strncmp(buffer_key, "flap_detection_enabled", 22) == 0) {
                        if (buffer_value[0] == '1')
                            hostdata.flags |= HOST_FLAP_DETECTION_ENABLED;
                        else
                            hostdata.flags |= HOST_FLAP_DETECTION_DISABLED;
                    } else if (key_length == 13 && strncmp(buffer_key, "plugin_output", 13) == 0) {
                        hostdata.output = buffer_value;
                    } else if (key_length == 13 && strncmp(buffer_key, "current_state", 13) == 0) {
                        switch (buffer_value[0] - 48)
                        {
                            case 0:
                                hostdata.status |= HOST_UP;
                                break;
                            case 1:
                                hostdata.status |= HOST_DOWN;
                                break;
                            case 2:
                                hostdata.status |= HOST_UNREACHABLE;
                                break;
                            case 3:
                                hostdata.status |= HOST_PENDING;
                                break;
                            default:
                                hostdata.status |= HOST_UNREACHABLE;
                        }
                    } else if (key_length == 24 && strncmp(buffer_key, "scheduled_downtime_depth", 24) == 0) {
                        if (buffer_value[0] == '1') {
                            hostdata.flags |= HOST_SCHEDULED_DOWNTIME;
                        } else {
                            hostdata.flags |= HOST_NO_SCHEDULED_DOWNTIME;
                        }
                    } else if (key_length == 29 && strncmp(buffer_key, "problem_has_been_acknowledged", 29) == 0) {
                        if (buffer_value[0] == '1') {
                            hostdata.flags |= HOST_STATE_ACKNOWLEDGED;
                        } else {
                            hostdata.flags |= HOST_STATE_UNACKNOWLEDGED;
                        }
                    } else if (key_length == 10 && strncmp(buffer_key, "state_type", 10) == 0) {
                        if (buffer_value[0] == '1')
                            hostdata.flags |= HOST_HARD_STATE;
                        else
                            hostdata.flags |= HOST_SOFT_STATE;
                    } else if (key_length == 21 && strncmp(buffer_key, "notifications_enabled", 21) == 0) {
                        if (buffer_value[0] == '1')
                            hostdata.flags |= HOST_NOTIFICATIONS_ENABLED;
                        else
                            hostdata.flags |= HOST_NOTIFICATIONS_DISABLED;
                    } else if (key_length == 10 && strncmp(buffer_key, "check_type", 10) == 0) {
                        if (buffer_value[0] == '1')
                            hostdata.flags |= HOST_ACTIVE_CHECK;
                        else
                            hostdata.flags |= HOST_PASSIVE_CHECK;
                    } else if (key_length == 22 && strncmp(buffer_key, "passive_checks_enabled", 22) == 0) {
                        if (buffer_value[0] == '1')
                            hostdata.flags |= HOST_PASSIVE_CHECKS_ENABLED;
                        else
                            hostdata.flags |= HOST_PASSIVE_CHECKS_DISABLED;
                    } else if (key_length == 21 && strncmp(buffer_key, "active_checks_enabled", 21) == 0) {
                        if (buffer_value[0] == '1')
                            hostdata.flags |= HOST_ACTIVE_CHECKS_ENABLED;
                        else
                            hostdata.flags |= HOST_ACTIVE_CHECKS_DISABLED;
                    } else if (key_length == 21 && strncmp(buffer_key, "event_handler_enabled", 21) == 0) {
                        if (buffer_value[0] == '1')
                            hostdata.flags |= HOST_EVENT_HANDLER_ENABLED;
                        else
                            hostdata.flags |= HOST_EVENT_HANDLER_DISABLED;
                    } else if (key_length == 17 && strncmp(buffer_key, "last_state_change", 17) == 0) {
                        hostdata.last_state_change = (time_t)strtoull(buffer_value, nullptr, 10);
                        hostdata.last_state_diff = now - hostdata.last_state_change;
                    } else if (key_length == 12 && strncmp(buffer_key, "max_attempts", 12) == 0) {
                        hostdata.max_attempts = strtoull(buffer_value, nullptr, 10);
                    } else if (key_length == 15 && strncmp(buffer_key, "current_attempt", 15) == 0) {
                        hostdata.current_attempt = strtoull(buffer_value, nullptr, 10);
                    } else if (key_length == 9 && strncmp(buffer_key, "host_name", 9) == 0) {
                        hostdata.hostname = buffer_value;
                    }
                    break;
                case C_SERVICESTATUS:
                    if (key_length == 11 && strncmp(buffer_key, "is_flapping", 11) == 0) {
                        if (buffer_value[0] == '1')
                            servicedata.flags |= SERVICE_IS_FLAPPING;
                        else
                            servicedata.flags |= SERVICE_IS_NOT_FLAPPING;
                    } else if (key_length == 22 && strncmp(buffer_key, "flap_detection_enabled", 22) == 0) {
                        if (buffer_value[0] == '1')
                            servicedata.flags |= SERVICE_FLAP_DETECTION_ENABLED;
                        else
                            servicedata.flags |= SERVICE_FLAP_DETECTION_DISABLED;
                    } else if (key_length == 13 && strncmp(buffer_key, "plugin_output", 13) == 0) {
                        servicedata.output = buffer_value;
                    } else if (key_length == 13 && strncmp(buffer_key, "current_state", 13) == 0) {
                        switch (buffer_value[0] - 48)
                        {
                            case 0:
                                servicedata.status |= SERVICE_OK;
                                break;
                            case 1:
                                servicedata.status |= SERVICE_WARNING;
                                break;
                            case 2:
                                servicedata.status |= SERVICE_CRITICAL;
                                break;
                            case 3:
                                servicedata.status |= SERVICE_UNKNOWN;
                                break;
                            default:
                                servicedata.status |= SERVICE_PENDING;
                        }
                    } else if (key_length == 24 && strncmp(buffer_key, "scheduled_downtime_depth", 24) == 0) {
                        if (buffer_value[0] == '1') {
                            servicedata.flags |= SERVICE_SCHEDULED_DOWNTIME;
                        } else {
                            servicedata.flags |= SERVICE_NO_SCHEDULED_DOWNTIME;
                        }
                    } else if (key_length == 29 && strncmp(buffer_key, "problem_has_been_acknowledged", 29) == 0) {
                        if (buffer_value[0] == '1') {
                            servicedata.flags |= SERVICE_STATE_ACKNOWLEDGED;
                        } else {
                            servicedata.flags |= SERVICE_STATE_UNACKNOWLEDGED;
                        }
                    } else if (key_length == 10 && strncmp(buffer_key, "state_type", 10) == 0) {
                        if (buffer_value[0] == '1')
                            servicedata.flags |= SERVICE_HARD_STATE;
                        else
                            servicedata.flags |= SERVICE_SOFT_STATE;
                    } else if (key_length == 21 && strncmp(buffer_key, "notifications_enabled", 21) == 0) {
                        if (buffer_value[0] == '1')
                            servicedata.flags |= SERVICE_NOTIFICATIONS_ENABLED;
                        else
                            servicedata.flags |= SERVICE_NOTIFICATIONS_DISABLED;
                    } else if (key_length == 10 && strncmp(buffer_key, "check_type", 10) == 0) {
                        if (buffer_value[0] == '1')
                            servicedata.flags |= SERVICE_ACTIVE_CHECK;
                        else
                            servicedata.flags |= SERVICE_PASSIVE_CHECK;
                    } else if (key_length == 22 && strncmp(buffer_key, "passive_checks_enabled", 22) == 0) {
                        if (buffer_value[0] == '1')
                            servicedata.flags |= SERVICE_PASSIVE_CHECKS_ENABLED;
                        else
                            servicedata.flags |= SERVICE_PASSIVE_CHECKS_DISABLED;
                    } else if (key_length == 21 && strncmp(buffer_key, "active_checks_enabled", 21) == 0) {
                        if (buffer_value[0] == '1')
                            servicedata.flags |= SERVICE_ACTIVE_CHECKS_ENABLED;
                        else
                            servicedata.flags |= SERVICE_ACTIVE_CHECKS_DISABLED;
                    } else if (key_length == 21 && strncmp(buffer_key, "event_handler_enabled", 21) == 0) {
                        if (buffer_value[0] == '1')
                            servicedata.flags |= SERVICE_EVENT_HANDLER_ENABLED;
                        else
                            servicedata.flags |= SERVICE_EVENT_HANDLER_DISABLED;
                    } else if (key_length == 17 && strncmp(buffer_key, "last_state_change", 17) == 0) {
                        servicedata.last_state_change = (time_t)strtoull(buffer_value, nullptr, 10);
                        servicedata.last_state_diff = now - servicedata.last_state_change;
                    } else if (key_length == 12 && strncmp(buffer_key, "max_attempts", 12) == 0) {
                        servicedata.max_attempts = strtoull(buffer_value, nullptr, 10);
                    } else if (key_length == 15 && strncmp(buffer_key, "current_attempt", 15) == 0) {
                        servicedata.current_attempt = strtoull(buffer_value, nullptr, 10);
                    } else if (key_length == 19 && strncmp(buffer_key, "service_description", 19) == 0) {
                        servicedata.servicename = buffer_value;
                    } else if (key_length == 9 && strncmp(buffer_key, "host_name", 9) == 0) {
                        servicedata.hostname = buffer_value;
                    }
                    break;
                case C_INFO:
                    if (key_length == 7 && strncmp(buffer_key, "created", 7) == 0) {
                        now = (time_t)strtoull(buffer_value, nullptr, 10);
                        this->created = now;
                    }
                    break;
                case C_PROGRAMSTATUS:
                    if (key_length == 10 && strncmp(buffer_key, "nagios_pid", 10) == 0) {
                        nagiospid = atoi(buffer_value);
                        if (nagiospid > 0) {
                            rc = kill(nagiospid, 0);
                            if (rc != 0) {
                                snprintf(filenamebuffer,512,"/proc/%d",nagiospid);
                                if (stat(filenamebuffer, &st) == 0)
                                    this->is_nagios_running = true;
                            } else {
                                this->is_nagios_running = true;
                            }
                        }
                    }
                    break;
                case C_SERVICEDOWNTIME:
                    if (key_length == 19 && strncmp(buffer_key, "service_description", 19) == 0) {
                        servicename = buffer_value;
                    } else if (key_length == 9 && strncmp(buffer_key, "host_name", 9) == 0) {
                        hostname = buffer_value;
                    } else if (key_length == 6 && strncmp(buffer_key, "author", 6) == 0) {
                        nagios_downtime.author = buffer_value;
                    } else if (key_length == 10 && strncmp(buffer_key, "entry_time", 10) == 0) {
                        nagios_downtime.added = (time_t)strtoull(buffer_value, nullptr, 10);
                    } else if (key_length == 10 && strncmp(buffer_key, "start_time", 10) == 0) {
                        nagios_downtime.start_time = (time_t)strtoull(buffer_value, nullptr, 10);
                    } else if (key_length == 9 && strncmp(buffer_key, "end_time", 8) == 0) {
                        nagios_downtime.end_time = (time_t)strtoull(buffer_value, nullptr, 10);
                    } else if (key_length == 5 && strncmp(buffer_key, "fixed", 5) == 0) {
                        nagios_downtime.fixed = (buffer_value[0] == '1' ? true : false);
                    } else if (key_length == 8 && strncmp(buffer_key, "duration", 8) == 0) {
                        nagios_downtime.duration = (time_t)strtoull(buffer_value, nullptr, 10);
                    } else if (key_length == 7 && strncmp(buffer_key, "comment", 7) == 0) {
                        nagios_downtime.comment = buffer_value;
                    }
                    break;
                case C_HOSTCOMMENT:
                    if (key_length == 19 && strncmp(buffer_key, "service_description", 19) == 0) {
                        servicename = buffer_value;
                    } else if (key_length == 9 && strncmp(buffer_key, "host_name", 9) == 0) {
                        hostname = buffer_value;
                    } else if (key_length == 6 && strncmp(buffer_key, "author", 6) == 0) {
                        nagios_comment.author = buffer_value;
                    } else if (key_length == 10 && strncmp(buffer_key, "entry_time", 10) == 0) {
                        nagios_comment.added = (time_t)strtoull(buffer_value, nullptr, 10);
                    } else if (key_length == 7 && strncmp(buffer_key, "comment", 7) == 0) {
                        nagios_comment.comment = buffer_value;
                    }
                    break;
                case C_SERVICECOMMENT:
                    if (key_length == 19 && strncmp(buffer_key, "service_description", 19) == 0) {
                        servicename = buffer_value;
                    } else if (key_length == 9 && strncmp(buffer_key, "host_name", 9) == 0) {
                        hostname = buffer_value;
                    } else if (key_length == 6 && strncmp(buffer_key, "author", 6) == 0) {
                        nagios_comment.author = buffer_value;
                    } else if (key_length == 10 && strncmp(buffer_key, "entry_time", 10) == 0) {
                        nagios_comment.added = (time_t)strtoull(buffer_value, nullptr, 10);
                    } else if (key_length == 7 && strncmp(buffer_key, "comment", 7) == 0) {
                        nagios_comment.comment = buffer_value;
                    }
                    break;
            }
        }
        skip_line:
        read_size += linesize+1;
        if (read_size >= this->flength)
            break;
        buffer_current += linesize+1;
    }
    this->gc();
}

void NagiosParser::add_host(nagios_host_t *hostdata)
{
    this->hosts[hostdata->hostname] = *hostdata;
}

void NagiosParser::add_service(nagios_service_t *servicedata)
{
    static
    std::string						concat;
    nagiosmap_host_t::iterator		it_current	= this->hosts.find(servicedata->hostname);
    nagios_host_t					*hostdata	= nullptr;
    if (it_current != this->hosts.end()) {
        hostdata = &(it_current->second);
        ++(it_current->second.services);
    }
    concat.assign(servicedata->hostname);
    concat.append(servicedata->servicename);
    servicedata->hostdata = hostdata;
    this->services[concat] = *servicedata;
}

void NagiosParser::add_servicecomment(const std::string& hostname, const std::string& servicename, nagios_comment_t *d)
{
    static std::string concat;
    static nagiosmap_service_t::iterator it_c;
    concat.assign(hostname);
    concat.append(servicename);
    it_c = this->services.find(concat);
    if (it_c == this->services.end())
        return;
    it_c->second.comments.push_back(*d);
}

void NagiosParser::add_servicedowntime(const std::string& hostname, const std::string& servicename, nagios_downtime_t *d)
{
    static std::string concat;
    static nagiosmap_service_t::iterator it_c;
    concat.assign(hostname);
    concat.append(servicename);
    it_c = this->services.find(concat);
    if (it_c == this->services.end())
        return;
    it_c->second.downtime = *d;
}

void NagiosParser::add_hostcomment(const std::string& hostname, nagios_comment_t *d)
{
    static nagiosmap_host_t::iterator it_c;
    it_c = this->hosts.find(hostname);
    if (it_c == this->hosts.end())
        return;
    it_c->second.comments.push_back(*d);
}

void NagiosParser::add_hostdowntime(const std::string& hostname, nagios_downtime_t *d)
{
    static nagiosmap_host_t::iterator it_c;
    it_c = this->hosts.find(hostname);
    if (it_c == this->hosts.end())
        return;
    it_c->second.downtime = *d;
}

/*
bool NagiosParser::is_host_alive(std::string hostname)
{
	static nagiosmap_host_t::iterator it_c;
	it_c = this->hosts.find(hostname);
	if (it_c == this->hosts.end())
		return false;
	if ((it_c->second.status & HOST_UP) == 0)
		return false;
	else
		return true;
}
*/

void NagiosParser::reset_host(nagios_host_t *hostdata)
{
    hostdata->hostname.clear();
    hostdata->last_state_change =
    hostdata->max_attempts =
    hostdata->current_attempt =
    hostdata->status =
    hostdata->last_state_diff =
    hostdata->services =
    hostdata->services_visible =
    hostdata->flags = 0;
    this->reset_downtime(&hostdata->downtime);
}

void NagiosParser::reset_service(nagios_service_t *servicedata)
{
    servicedata->hostname.clear();
    servicedata->servicename.clear();
    servicedata->last_state_change =
    servicedata->max_attempts =
    servicedata->current_attempt =
    servicedata->status =
    servicedata->last_state_diff =
    servicedata->flags = 0;
    servicedata->hostdata = nullptr;
    this->reset_downtime(&servicedata->downtime);
}

void NagiosParser::reset_downtime(nagios_downtime_t *d)
{
    d->added =
    d->start_time =
    d->end_time =
    d->duration = 0;
    d->fixed = false;
    d->author.clear();
    d->comment.clear();
}

void NagiosParser::reset_comment(nagios_comment_t *d)
{
    d->added = 0;
    d->author.clear();
    d->comment.clear();
}

void NagiosParser::do_cgi()
{
    printf("Refresh: 15\r\n");
    printf("Content-type: text/plain\r\n\r\n");
}

void NagiosParser::do_json()
{
    // old buffer size: 4096, new buffer size: 412(json encap size atm) + 256(max-host-address) + 128(max service length) + 4096(max-plugin-output) = 4892 rounded to 5120
    char						*buffer = (char*)calloc(5120, sizeof(char));
    uint32_t					pversion	= config::getInstance()->getInt("version", 0);
    nagios_service_t			*scurr		= nullptr;
    std::vector<std::string>	jsonlist;
    std::string					callback_function_name = config::getInstance()->get("callback", "");
    size_t						vecsize		= 0,
            i			= 0;
    std::string					statusbuffer("-");
    std::string					diffbuffer("-");
    struct tm					*ltime;
    time_t						now			= time(nullptr);

    ltime = localtime(&now);
    if (callback_function_name.length() > 0) {
        std::cout << callback_function_name << "(";
    }

    std::cout << "{"
              << "\"version\":" << pversion << ","
              << "\"running\":" << (this->is_nagios_running ? "1" : "0") << ","
              << "\"servertime\":" << now << ","
              << "\"localtime\":["
              << ltime->tm_isdst << ","
              << ltime->tm_gmtoff
              << "],"
              << "\"created\":" << this->created << ","
              << "\"data\":[";

    for (nagiosmap_service_t::iterator it_c=this->services.begin(), it_e=this->services.end(); it_c != it_e; ++it_c)
    {
        scurr = &(it_c->second);
        format_status_service(&scurr->status, &statusbuffer);
        format_date(&scurr->last_state_diff, &diffbuffer);
        snprintf(buffer, 5120,
                 "{"
                 "\"last_state_change\":%lu,"
                 "\"plugin_output\":\"%s\","
                 "\"status\":\"%s\","
                 "\"flags\":%u,"
                 "\"hostname\":\"%s\","
                 "\"service\":\"%s\","
                 "\"host_alive\":%d,"
                 "\"services_total\":%d,"
                 "\"services_visible\":%d,"
                 "\"duration\":\"%s\""
                 "}",
                 scurr->last_state_change,
                 escapeJsonString(scurr->output).c_str(),
                 statusbuffer.c_str(),
                 scurr->flags,
                 escapeJsonString(scurr->hostname).c_str(),
                 escapeJsonString(scurr->servicename).c_str(),
                 ((scurr->hostdata != nullptr && (scurr->hostdata->status & HOST_UP) != 0)  ? 1 : 0),
                 (scurr->hostdata != nullptr ? scurr->hostdata->services : 0),
                 (scurr->hostdata != nullptr ? scurr->hostdata->services_visible : 0),
                 diffbuffer.c_str()
        );
        jsonlist.push_back(buffer);
    }
    vecsize = jsonlist.size();
    for (i=0; i<vecsize; i++)
    {
        if (i==0)
            std::cout << jsonlist[i];
        else
            std::cout << "," << jsonlist[i];
    }
    std::cout << "]}";
    if (callback_function_name.length() > 0) {
        std::cout << ")";
    }
    free(buffer);
}

void NagiosParser::filter()
{
    volatile
    bool							show				= false;
    uint32_t						servicestatustypes	= config::getInstance()->getInt("servicestatustypes",	0);
    uint32_t						serviceprops		= config::getInstance()->getInt("serviceprops",			0);
    uint32_t						hostprops			= config::getInstance()->getInt("hostprops",			0);
    std::string						statusbuffer("-");
    std::string						diffbuffer("-");
    nagios_service_t				*scurr				= nullptr;
    nagios_host_t					*hcurr				= nullptr;
    nagiosmap_service_t::iterator	it_current			= this->services.begin();
    nagiosmap_service_t::iterator	it_end				= this->services.end();
    nagiosmap_service_t::iterator	it_temp;
    while (it_current != it_end)
    {
        show = true;
        scurr = &(it_current->second);
        hcurr = it_current->second.hostdata;
        format_status_service(&scurr->status, &statusbuffer);
        format_date(&scurr->last_state_diff, &diffbuffer);
        if (serviceprops != 0 && ((scurr->flags & serviceprops) != serviceprops)) {
            show = false;
        }
        if (servicestatustypes != 0 && ((scurr->status & servicestatustypes) == 0)) {
            show = false;
        }
        if (hcurr != nullptr && hostprops != 0 && ((hcurr->flags & hostprops) != hostprops)) {
            show = false;
        }
        if (show) {
            ++it_current;
            hcurr->services_visible++;
        } else {
            it_temp = it_current;
            ++it_current;
            this->services.erase(it_temp);
        }
    }
}

void NagiosParser::do_plaintext()
{
    std::string statusbuffer("-");
    std::string diffbuffer("-");
    nagios_service_t *scurr = nullptr;
    for (nagiosmap_service_t::iterator it_c=this->services.begin(), it_e=this->services.end(); it_c != it_e; ++it_c)
    {
        scurr = &(it_c->second);
        format_status_service(&scurr->status, &statusbuffer);
        format_date(&scurr->last_state_diff, &diffbuffer);
        printf("%-25s | %-20s | %-8s | %-16s | \"%s\"\n",
               scurr->hostname.c_str(),
               scurr->servicename.c_str(),
               statusbuffer.c_str(),
               diffbuffer.c_str(),
               scurr->output.c_str()
        );
    }
}

void NagiosParser::run()
{
    this->parse();
    this->filter();
    if (config::getInstance()->getInt("cgi", 0) == 1)
        this->do_cgi();
    if (config::getInstance()->getInt("json", 0) == 1)
        this->do_json();
    else
        this->do_plaintext();
}
