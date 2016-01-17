
#ifndef _HANGOUTS_CONNECTION_H_
#define _HANGOUTS_CONNECTION_H_

#include <glib.h>

#include "purple-socket.h"
#include "http.h"


void hangouts_process_data_chunks(const gchar *data, gsize len);

void hangouts_process_channel(int fd);


#endif /*_HANGOUTS_CONNECTION_H_*/