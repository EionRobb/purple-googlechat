/*
 * Hangouts Plugin for libpurple/Pidgin
 * Copyright (c) 2015-2016 Eion Robb, Mike Ruprecht
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _HANGOUTS_AUTH_H_
#define _HANGOUTS_AUTH_H_

#include "libhangouts.h"

void hangouts_oauth_with_code(HangoutsAccount *ha, const gchar *auth_code);
void hangouts_oauth_refresh_token(HangoutsAccount *ha);
void hangouts_auth_get_session_cookies(HangoutsAccount *ha);

#endif /*_HANGOUTS_AUTH_H_*/