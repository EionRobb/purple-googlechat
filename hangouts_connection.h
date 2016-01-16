
#include <glib.h>

#include "purple-socket.h"
#include "http.h"


void hangouts_process_data_chunks(const gchar *data, gsize len);

void hangouts_process_channel(int fd);