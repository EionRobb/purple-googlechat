
#ifndef _HANGOUTS_AUTH_H_
#define _HANGOUTS_AUTH_H_

#include "libhangouts.h"

void hangouts_oauth_with_code(HangoutsAccount *ha, const gchar *auth_code);
void hangouts_oauth_refresh_token(HangoutsAccount *ha);
void hangouts_auth_get_session_cookies(HangoutsAccount *ha);

#endif /*_HANGOUTS_AUTH_H_*/