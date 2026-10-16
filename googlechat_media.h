/*
 * GoogleChat Plugin for libpurple/Pidgin
 * Copyright (c) 2015-2026 Eion Robb, Mike Ruprecht
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

#ifndef _GOOGLECHAT_MEDIA_H_
#define _GOOGLECHAT_MEDIA_H_

#include <purple.h>
#include "media.h"
#include "mediamanager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes runtime linking of libpurple voice/video functions via dlopen/dlsym.
 * Returns TRUE if media functions were successfully resolved and available.
 */
gboolean googlechat_media_init(void);

/**
 * Cleans up media function pointers and resources.
 */
void googlechat_media_uninit(void);

/**
 * Checks whether voice/video media is supported and ready at runtime.
 */
gboolean googlechat_media_is_supported(void);

/**
 * Returns media capabilities (audio/video) for a given account and buddy.
 */
PurpleMediaCaps googlechat_get_media_caps(PurpleAccount *account, const char *who);

/**
 * Initiates a voice/video media session.
 */
gboolean googlechat_initiate_media(PurpleAccount *account, const gchar *who, PurpleMediaSessionType type);

/**
 * Notifies libpurple that media capabilities have been updated for a buddy.
 */
void googlechat_media_got_caps(PurpleAccount *account, const gchar *who);

#ifdef __cplusplus
}
#endif

#endif /* _GOOGLECHAT_MEDIA_H_ */
