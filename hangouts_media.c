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

#define PURPLE_PLUGINS

#include "hangouts_media.h"


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "hangouts_pblite.h"
#include "hangouts_json.h"
#include "hangout_media.pb-c.h"
#include "hangouts_connection.h"

#include "mediamanager.h"
#include "media.h"

/** This is a bit of a hack; 
		if libpurple isn't compiled with USE_VV then a bunch of functions don't end up being exported, so we'll try load them in at runtime so we dont have to have a million and one different versions of the .so
*/
static gboolean purple_media_functions_initaliased = FALSE;
static gchar *(*_purple_media_candidate_get_username)(PurpleMediaCandidate *candidate);
static gchar *(*_purple_media_candidate_get_password)(PurpleMediaCandidate *candidate);
static PurpleMediaNetworkProtocol (*_purple_media_candidate_get_protocol)(PurpleMediaCandidate *candidate);
static guint (*_purple_media_candidate_get_component_id)(PurpleMediaCandidate *candidate);
static gchar *(*_purple_media_candidate_get_ip)(PurpleMediaCandidate *candidate);
static guint16 (*_purple_media_candidate_get_port)(PurpleMediaCandidate *candidate);
static PurpleMediaCandidateType (*_purple_media_candidate_get_candidate_type)(PurpleMediaCandidate *candidate);
static guint32 (*_purple_media_candidate_get_priority)(PurpleMediaCandidate *candidate);

// Using dlopen() instead of GModule because I don't want another dep
#ifdef _WIN32
#	include <windows.h>
#	define dlopen(filename, flag)  GetModuleHandleA(filename)
#	define dlsym(handle, symbol)   GetProcAddress(handle, symbol)
#	define dlclose(handle)         FreeLibrary(handle)
#else
#	include <dlfcn.h>
#endif

static void
hangouts_init_media_functions()
{
	gpointer libpurple_module;
	
	if (purple_media_functions_initaliased == FALSE) {
		libpurple_module = dlopen(NULL, 0);
		if (libpurple_module != NULL) {
			_purple_media_candidate_get_username = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_username");
			_purple_media_candidate_get_password = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_password");
			_purple_media_candidate_get_component_id = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_component_id");
			_purple_media_candidate_get_protocol = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_protocol");
			_purple_media_candidate_get_ip = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_ip");
			_purple_media_candidate_get_port = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_port");
			_purple_media_candidate_get_candidate_type = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_candidate_type");
			_purple_media_candidate_get_priority = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_priority");
			purple_media_functions_initaliased = TRUE;
		}
	}
}

typedef struct {
	HangoutsAccount *ha;
	gchar *hangout_id;
	PurpleMedia *media;
} HangoutsMedia;

static void
hangouts_pblite_media_media_session_add_cb(HangoutsAccount *ha, MediaSessionAddResponse *response, gpointer user_data)
{
	
}

static void
hangouts_media_candidates_prepared_cb(PurpleMedia *media, gchar *sid, gchar *name, HangoutsMedia *hangouts_media)
{
	MediaSessionAddRequest request;
	MediaSession media_session;
	MediaSession *media_sessions;
	MediaContent client_content;
	MediaContent *client_contents;
	// See TODO later down
	// MediaCryptoParams crypto_param;
	// MediaCryptoParams *crypto_params;
	MediaTransport transport;
	MediaIceCandidate *ice_candidates;
	gint n_ice_candidates;
	GList *purple_candidates;
	gint i;
	PurpleMediaSessionType purple_session_type;
	
	media_session_add_request__init(&request);
	media_session__init(&media_session);
	media_content__init(&client_content);
	media_transport__init(&transport);
	
	// Prefer RFC ICE
	transport.ice_version = ICE_VERSION__ICE_RFC_5245;
	
	purple_candidates = purple_media_get_local_candidates(media, sid, name);
	n_ice_candidates = g_list_length(purple_candidates);
	ice_candidates = g_new0(MediaIceCandidate, n_ice_candidates);
	for(i = 0; purple_candidates; purple_candidates = g_list_next(purple_candidates), i++) {
		PurpleMediaCandidate *purple_candidate = purple_candidates->data;
		MediaIceCandidate ice_candidate = ice_candidates[i];
		media_ice_candidate__init(&ice_candidate);
		
		//TODO multiple passwords needed?
		transport.username = _purple_media_candidate_get_username(purple_candidate);
		transport.password = _purple_media_candidate_get_password(purple_candidate);
		
		ice_candidate.has_component = TRUE;
		switch(_purple_media_candidate_get_component_id(purple_candidate)) {
			case PURPLE_MEDIA_COMPONENT_RTP:
				ice_candidate.component = COMPONENT__RTP;
				break;
			case PURPLE_MEDIA_COMPONENT_RTCP:
				ice_candidate.component = COMPONENT__RTCP;
				break;
			default:
				ice_candidate.has_component = FALSE;
				break;
		}
		
		ice_candidate.has_protocol = TRUE;
		switch(_purple_media_candidate_get_protocol(purple_candidate)) {
			case PURPLE_MEDIA_NETWORK_PROTOCOL_UDP:
				ice_candidate.protocol = PROTOCOL__UDP;
				break;
			case PURPLE_MEDIA_NETWORK_PROTOCOL_TCP_PASSIVE:
			case PURPLE_MEDIA_NETWORK_PROTOCOL_TCP_ACTIVE:
			case PURPLE_MEDIA_NETWORK_PROTOCOL_TCP_SO:
				ice_candidate.protocol = PROTOCOL__TCP;
				break;
			default:
				ice_candidate.has_protocol = FALSE;
				break;
		}
		
		ice_candidate.ip = _purple_media_candidate_get_ip(purple_candidate);
		ice_candidate.has_port = TRUE;
		ice_candidate.port = _purple_media_candidate_get_port(purple_candidate);
		
		ice_candidate.has_type = TRUE;
		switch(_purple_media_candidate_get_candidate_type(purple_candidate)) {
			case PURPLE_MEDIA_CANDIDATE_TYPE_HOST:
				ice_candidate.type = MEDIA_ICE_CANDIDATE_TYPE__HOST;
				break;
			case PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX:
				ice_candidate.type = MEDIA_ICE_CANDIDATE_TYPE__SERVER_REFLEXIVE;
				break;
			case PURPLE_MEDIA_CANDIDATE_TYPE_PRFLX:
				ice_candidate.type = MEDIA_ICE_CANDIDATE_TYPE__PEER_REFLEXIVE;
				break;
			case PURPLE_MEDIA_CANDIDATE_TYPE_RELAY:
				ice_candidate.type = MEDIA_ICE_CANDIDATE_TYPE__RELAY;
				break;
			default:
			case PURPLE_MEDIA_CANDIDATE_TYPE_MULTICAST:
				ice_candidate.has_type = FALSE;
				break;
		}
		
		ice_candidate.priority = _purple_media_candidate_get_priority(purple_candidate);
		ice_candidate.has_priority = TRUE;
	}
	transport.candidate = &ice_candidates;
	transport.n_candidate = n_ice_candidates;
	
	client_content.transport = &transport;
	
	//TODO generate a key
	//media_crypto_params__init(&crypto_param);
	//crypto_param.has_suite = TRUE;
	//crypto_param.suite = MEDIA_CRYPTO_SUITE__AES_CM_128_HMAC_SHA1_80;
	//crypto_param.key_params = 'inline:' random key
	//crypto_params = &crypto_param;
	//client_content.crypto_param = &crypto_params;
	
	//TODO is this correct?
	media_session.session_id = sid;
	media_session.n_client_content = 1;
	media_session.client_content = &client_contents;
	
	purple_session_type = purple_media_get_session_type(media, sid);
	client_content.has_media_type = TRUE;
	if (purple_session_type == PURPLE_MEDIA_SEND_AUDIO) {
		client_content.media_type = MEDIA_TYPE__MEDIA_TYPE_AUDIO;
	} else if (purple_session_type == PURPLE_MEDIA_SEND_VIDEO) {
		client_content.media_type = MEDIA_TYPE__MEDIA_TYPE_VIDEO;
	} else if (purple_session_type == PURPLE_MEDIA_SEND_APPLICATION) {
		client_content.media_type = MEDIA_TYPE__MEDIA_TYPE_DATA;
	} else {
		client_content.media_type = MEDIA_TYPE__MEDIA_TYPE_BUNDLE;
	}
	
	media_sessions = &media_session;
	request.n_resource = 1;
	request.resource = &media_sessions;
	
	
	hangouts_pblite_media_media_session_add(hangouts_media->ha, &request, hangouts_pblite_media_media_session_add_cb, hangouts_media);
	
	g_free(ice_candidates);
}



gboolean
hangouts_initiate_media(PurpleAccount *account, const gchar *who, PurpleMediaSessionType type)
{
	PurpleConnection *pc = purple_account_get_connection(account);
	HangoutsAccount *ha = purple_connection_get_protocol_data(pc);
	HangoutsMedia *hangouts_media;
	GParameter *params = NULL;
	gint num_params = 0;
	
	hangouts_init_media_functions();
	
	//TODO use openwebrtc instead of fsrtpconference
	PurpleMedia *media = purple_media_manager_create_media(purple_media_manager_get(),
			account, "fsrtpconference", who, TRUE);
	
	if (media == NULL) {
		return FALSE;
	}
	
	hangouts_media = g_new0(HangoutsMedia, 1);
	hangouts_media->ha = ha;
	hangouts_media->media = media;
	purple_media_set_protocol_data(media, hangouts_media);
	
	g_signal_connect(G_OBJECT(media), "candidates-prepared",
				 G_CALLBACK(hangouts_media_candidates_prepared_cb), hangouts_media);
	// TODO
	// g_signal_connect(G_OBJECT(media), "codecs-changed",
				 // G_CALLBACK(hangouts_media_codecs_changed_cb), hangouts_media);
	// g_signal_connect(G_OBJECT(media), "state-changed",
				 // G_CALLBACK(hangouts_media_state_changed_cb), hangouts_media);
	// g_signal_connect(G_OBJECT(media), "stream-info",
			// G_CALLBACK(hangouts_media_stream_info_cb), hangouts_media);
	
	//TODO add params
	
	if(!purple_media_add_stream(media, "needs a name", who, type, TRUE, "nice", num_params, params)) {
		purple_media_end(media, NULL, NULL);
		/* TODO: How much clean-up is necessary here? (does calling
		         purple_media_end lead to cleaning up Jingle structs?) */
		return FALSE;
	}
	
	return TRUE;
}
