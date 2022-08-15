#include <iostream>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

extern "C"
{
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <getopt.h>
}

#include "ng_config.h"
config* config::pinstance(nullptr);
#include "ng_parser.h"

static
bool is_numeric(char *d)
{
    for (int i=0,e=(int)strlen(d); i<e; ++i)
    {
        if (!isdigit(d[i]))
            return false;
    }
    return true;
}

static void find_statusfile()
{
    std::ifstream		filestr;
    const char			*configlist[] = {
            "/usr/local/etc/nagios/nagios.cfg",
            "/usr/local/etc/nagios3/nagios.cfg",
            "/usr/local/nagios/etc/nagios.cfg",
            "/usr/local/nagios3/etc/nagios.cfg",
            "/etc/nagios3/nagios.cfg",
            "/etc/nagios/nagios.cfg",
            nullptr
    };
    static const int	buffer_max = 512;
    char				**cl = (char**)configlist;
    char				buffer[buffer_max];
    char				statusfile[buffer_max];
    char				*cur_helper = nullptr;
    size_t				line_length = 0;
    int					i = 0, e = 0;
    int					statusfile_position = 0;
    bool				found = false;

    memset(buffer,		0,	buffer_max);
    memset(statusfile,	0,	buffer_max);

    while (*cl != nullptr)
    {
        filestr.open(*cl, std::ifstream::in|std::ifstream::binary);
        ++cl;

        if (!filestr.is_open())
            continue;

        while (filestr.getline(buffer, buffer_max))
        {
            line_length = strlen(buffer);
            if (line_length <= 13)
                continue;
            cur_helper = strstr(buffer, "status_file");
            if (cur_helper == nullptr)
                continue;
            cur_helper = cur_helper+11;
            cur_helper = strchr(buffer, '=');
            if (cur_helper == nullptr || cur_helper-buffer == 0)
                continue;
            cur_helper++;
            for (i=0, e=strlen(cur_helper); i<e; ++i)
            {
                if (isgraph(cur_helper[i]))
                    statusfile[statusfile_position++] = cur_helper[i];
            }
            if (statusfile_position > 0) {
                found = true;
                config::getInstance()->set("statusfile", statusfile);
                break;
            }
        }
        filestr.close();
        if (found)
            break;
    }
}

static void cgi_parse(char *d)
{
    if (d == nullptr)
        return;
    char	**stringp	= &d;
    const
    char	sep[]		= "?&";
    char	*ret		= nullptr;
    char	*eq_helper	= nullptr;
    int		len			= 0;
    int		len_helper	= 0;
    config::getInstance()->setInt("cgi", 1);
    config::getInstance()->setInt("json", 1);

    while (1) {
        ret = strsep(stringp, sep);
        if (ret == nullptr)
            break;
        eq_helper = strchr(ret, '=');
        if (eq_helper == nullptr)
            continue;
        len = eq_helper-ret;
        if (len == 0)
            continue;
        if (strlen(eq_helper) == 1)
            continue;
        ++eq_helper;
        if (strncmp("serviceprops", ret, len) == 0) {
            if (is_numeric(eq_helper))
                config::getInstance()->setInt("serviceprops", atoi(eq_helper));
        }
        else if (strncmp("hostprops", ret, len) == 0) {
            if (is_numeric(eq_helper))
                config::getInstance()->setInt("hostprops", atoi(eq_helper));
        }
        else if (strncmp("servicestatustypes", ret, len) == 0) {
            if (is_numeric(eq_helper))
                config::getInstance()->setInt("servicestatustypes", atoi(eq_helper));
        }
        else if (strncmp("json", ret, len) == 0) {
            if (is_numeric(eq_helper))
                config::getInstance()->setInt("json", atoi(eq_helper));
        }
        else if (strncmp("callback", ret, len) == 0) {
            len_helper = (int)strlen(eq_helper);
            if (len_helper > 0 && len_helper < 100)
                config::getInstance()->set("callback", eq_helper);
        }
        else if (strncmp("include-fields", ret, len) == 0) {
            len_helper = (int)strlen(eq_helper);
            if (len_helper > 0) {
                config::getInstance()->set("include-fields", eq_helper);
            }
        }
    }
}

void do_help(char *pname)
{
    std::cout << "Usage: " << pname << " [OPTIONS] -s <status.dat file>" << std::endl;
    std::cout << "Nagios status.dat parser" << std::endl;
    std::cout << std::endl;
    std::cout << "Mandatory arguments to long options are mandatory for short options too." << std::endl;
    std::cout << "  -d, --debug                  verbose output" << std::endl;
    std::cout << "  -s, --status=[FILE]          nagios status.dat path" << std::endl;
    std::cout << "  -c, --cgi                    cgi mode" << std::endl;
    std::cout << "  -j, --json                   json output" << std::endl;
    std::cout << "  -t, --servicestatustypes     service status type (OK/CRIT/WARN)" << std::endl;
    std::cout << "  -p, --serviceprops           service props (ACK/MAINTENANCE)" << std::endl;
    std::cout << "  -n, --hostprops              host props (ACK/MAINTENANCE)" << std::endl;
    std::cout << "  -i, --include-fields         status.dat fields, separated by: ','" << std::endl;
    std::cout << std::endl;
    std::cout << "Report bugs to macskas @bh" << std::endl;
}

int main(int argc, char **argv)
{
    char					*program_name = argv[0];
    static int				verbose_flag;
    char					*query_string = nullptr;
    int						c, option_index, optfail = 0;
    static struct option	long_options[] =
            {
                    {"help",				no_argument,			&verbose_flag,	'h'},
                    {"status",				required_argument,		0,				's'},
                    {"debug",				no_argument,			0,				'd'},
                    {"cgi",					no_argument,			0,				'c'},
                    {"servicestatustypes",	required_argument,		0,				't'},
                    {"serviceprops",		required_argument,		0,				'p'},
                    {"hostprops",			required_argument,		0,				'n'},
                    {"include-fields",	    required_argument,		0,				'i'},
                    {nullptr, 0, nullptr, 0}
            };

    config::getInstance()->setInt("serviceprops",		0);
    config::getInstance()->setInt("servicestatustypes", 28);
    config::getInstance()->setInt("hostprops",			0);
    config::getInstance()->setInt("version",			20110619);

    while ((c = getopt_long(argc, argv, "hs:dcjt:p:n:i:", long_options, &option_index)) != -1)
    {
        switch (c)
        {
            case 'h':
                do_help(program_name);
                exit(1);
                break;
            case 'd':
                config::getInstance()->setInt("debugmode", 1);
                break;
            case 'c':
                config::getInstance()->setInt("cgi", 1);
                break;
            case 'j':
                config::getInstance()->setInt("json", 1);
                break;
            case 's':
                if (optarg != nullptr)
                {
                    config::getInstance()->set("statusfile", optarg);
                }
                break;
            case 't':
                if (optarg != nullptr) {
                    if (!is_numeric(optarg))
                        optfail = 1;
                    config::getInstance()->setInt("servicestatustypes", atoi(optarg));
                } else {
                    optfail = 1;
                }
                break;
            case 'p':
                if (optarg != nullptr) {
                    if (!is_numeric(optarg))
                        optfail = 1;
                    config::getInstance()->setInt("serviceprops", atoi(optarg));
                } else {
                    optfail = 1;
                }
                break;
            case 'n':
                if (optarg != nullptr) {
                    if (!is_numeric(optarg)) {
                        optfail = 1;
                    }
                    config::getInstance()->setInt("hostprops", atoi(optarg));
                } else {
                    optfail = 1;
                }
                break;
            case 'i':
                if (optarg != nullptr) {
                    config::getInstance()->set("include-fields", optarg);
                }
                break;
            default:
                optfail = 1;
        }
    }

    if (config::getInstance()->get("statusfile", "") == "")
        find_statusfile();

    if (config::getInstance()->get("statusfile", "") == "") {
        std::cerr << "Error: Missing statusfile." << std::endl;
        exit(1);
    }
    if (optfail == 1) {
        do_help(program_name);
        exit(1);
    }
    query_string = getenv("QUERY_STRING") ? strdup(getenv("QUERY_STRING")) : nullptr;
    cgi_parse(query_string);
    NagiosParser NP(config::getInstance()->get("statusfile", ""));
    NP.run();

    delete config::getInstance();
    free(query_string);
    return 0;
}