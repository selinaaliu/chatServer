#ifndef __cs_client_h_
#define __cs_client_h_
#include "cs_common.h"

void client_nick(address client, string const &name);
void client_part(address client);
void client_quit(address client);
void client_join(address client, int newRoomId);

#endif
