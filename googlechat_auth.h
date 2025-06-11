/*
 * GoogleChat Plugin for libpurple/Pidgin
 * Copyright (c) 2015-2025 Eion Robb, Mike Ruprecht
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


#ifndef _GOOGLECHAT_AUTH_H_
#define _GOOGLECHAT_AUTH_H_

#include "libgooglechat.h"

void googlechat_oauth_with_code(GoogleChatAccount *ha, const gchar *auth_code);
gboolean googlechat_oauth_refresh_token(GoogleChatAccount *ha);
void googlechat_cache_ssl_certs(GoogleChatAccount *ha);
void googlechat_auth_finished_auth(GoogleChatAccount *ha);
gboolean googlechat_auth_refresh_xsrf_token(GoogleChatAccount *ha);

#endif /*_GOOGLECHAT_AUTH_H_*/