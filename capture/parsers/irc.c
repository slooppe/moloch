#include <string.h>
#include <ctype.h>
#include "moloch.h"

typedef struct {
    int ircState;
} IRCInfo_t;

static int channelsField;
static int nickField;

/******************************************************************************/
int irc_parser(MolochSession_t *session, void *uw, const unsigned char *data, int remaining)
{
    IRCInfo_t *irc = uw;

    if (session->which == 1)
        return 0;

    while (remaining) {
        if (irc->ircState & 0x1) {
            unsigned char *newline = memchr(data, '\n', remaining);
            if (newline) {
                remaining -= (newline - data) +1;
                data = newline+1;
                irc->ircState &= ~ 0x1;
            } else {
                return 0;
            }
        }

        if (remaining > 5 && memcmp("JOIN ", data, 5) == 0) {
            const unsigned char *end = data + remaining;
            const unsigned char *ptr = data + 5;

            while (ptr < end && *ptr != ' ' && *ptr != '\r' && *ptr != '\n') {
                ptr++;
            }

            moloch_field_string_add(channelsField, session, (char*)data + 5, ptr - data - 5, TRUE);
        }

        if (remaining > 5 && memcmp("NICK ", data, 5) == 0) {
            const unsigned char *end = data + remaining;
            const unsigned char *ptr = data + 5;

            while (ptr < end && *ptr != ' ' && *ptr != '\r' && *ptr != '\n') {
                ptr++;
            }

            moloch_field_string_add(nickField, session, (char*)data + 5, ptr - data - 5, TRUE);
        }

        if (remaining > 0) {
            irc->ircState |=  0x1;
        }
    }

    return 0;
}
/******************************************************************************/
void irc_free(MolochSession_t UNUSED(*session), void *uw)
{
    IRCInfo_t            *irc          = uw;

    MOLOCH_TYPE_FREE(IRCInfo_t, irc);
}
/******************************************************************************/
void irc_classify(MolochSession_t *session, const unsigned char *data, int len)
{
    if (data[0] == ':' && !moloch_memstr((char *)data, len, " NOTICE ", 8))
        return;

    //If a USER packet must have NICK with it so we don't pickup FTP
    if (data[0] == 'U' && !moloch_memstr((char *)data, len, "\nNICK ", 6)) {
        return;
    }

    if (moloch_nids_has_protocol(session, "irc"))
        return;

    moloch_nids_add_tag(session, "protocol:irc");
    moloch_nids_add_protocol(session, "irc");

    IRCInfo_t            *irc          = MOLOCH_TYPE_ALLOC0(IRCInfo_t);

    moloch_parsers_register(session, irc_parser, irc, irc_free);
    irc_parser(session, irc, data, len);
}
/******************************************************************************/
void moloch_parser_init()
{
    nickField = moloch_field_define("irc", "termfield",
        "irc.nick", "Nickname", "ircnck", 
        "Nicknames set", 
        MOLOCH_FIELD_TYPE_STR_HASH, MOLOCH_FIELD_FLAG_CNT, 
        NULL);

    channelsField = moloch_field_define("irc", "termfield",
        "irc.channel", "Channel", "ircch", 
        "Channels joined",  
        MOLOCH_FIELD_TYPE_STR_HASH, MOLOCH_FIELD_FLAG_CNT, 
        NULL);

    moloch_parsers_classifier_register_tcp("irc", 0, (unsigned char*)":", 1, irc_classify);
    moloch_parsers_classifier_register_tcp("irc", 0, (unsigned char*)"NOTICE AUTH", 11, irc_classify);
    moloch_parsers_classifier_register_tcp("irc", 0, (unsigned char*)"NICK ", 5, irc_classify);
    moloch_parsers_classifier_register_tcp("irc", 0, (unsigned char*)"USER ", 5, irc_classify);
}

