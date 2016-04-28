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

#include "debug.h"
#include "mediamanager.h"
#include "media.h"

/** This is a bit of a hack; 
		if libpurple isn't compiled with USE_VV then a bunch of functions don't end up being exported, so we'll try load them in at runtime so we dont have to have a million and one different versions of the .so
*/
static gboolean purple_media_functions_initaliased = FALSE;
// media/candidates.h
static gchar *(*_purple_media_candidate_get_username)(PurpleMediaCandidate *candidate);
static gchar *(*_purple_media_candidate_get_password)(PurpleMediaCandidate *candidate);
static PurpleMediaNetworkProtocol (*_purple_media_candidate_get_protocol)(PurpleMediaCandidate *candidate);
static guint (*_purple_media_candidate_get_component_id)(PurpleMediaCandidate *candidate);
static gchar *(*_purple_media_candidate_get_ip)(PurpleMediaCandidate *candidate);
static guint16 (*_purple_media_candidate_get_port)(PurpleMediaCandidate *candidate);
static PurpleMediaCandidateType (*_purple_media_candidate_get_candidate_type)(PurpleMediaCandidate *candidate);
static guint32 (*_purple_media_candidate_get_priority)(PurpleMediaCandidate *candidate);
// media/codecs.h
static guint (*_purple_media_codec_get_id)(PurpleMediaCodec *codec);
static gchar *(*_purple_media_codec_get_encoding_name)(PurpleMediaCodec *codec);
static guint (*_purple_media_codec_get_clock_rate)(PurpleMediaCodec *codec);
static guint (*_purple_media_codec_get_channels)(PurpleMediaCodec *codec);
static GList *(*_purple_media_codec_get_optional_parameters)(PurpleMediaCodec *codec);

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
			// media/candidates.h
			_purple_media_candidate_get_username = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_username");
			_purple_media_candidate_get_password = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_password");
			_purple_media_candidate_get_component_id = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_component_id");
			_purple_media_candidate_get_protocol = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_protocol");
			_purple_media_candidate_get_ip = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_ip");
			_purple_media_candidate_get_port = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_port");
			_purple_media_candidate_get_candidate_type = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_candidate_type");
			_purple_media_candidate_get_priority = (gpointer) dlsym(libpurple_module, "purple_media_candidate_get_priority");
			
			// media/codecs.h
			_purple_media_codec_get_id = (gpointer) dlsym(libpurple_module, "purple_media_codec_get_id");
			_purple_media_codec_get_encoding_name = (gpointer) dlsym(libpurple_module, "purple_media_codec_get_encoding_name");
			_purple_media_codec_get_clock_rate = (gpointer) dlsym(libpurple_module, "purple_media_codec_get_clock_rate");
			_purple_media_codec_get_channels = (gpointer) dlsym(libpurple_module, "purple_media_codec_get_channels");
			_purple_media_codec_get_optional_parameters = (gpointer) dlsym(libpurple_module, "purple_media_codec_get_optional_parameters");
			
			purple_media_functions_initaliased = TRUE;
		}
	}
}

typedef struct {
	HangoutsAccount *ha;
	gchar *hangout_id;
	PurpleMedia *media;
	gchar *who;
} HangoutsMedia;

static MediaType
hangout_get_session_media_type(PurpleMedia *media, gchar *sid) {
	PurpleMediaSessionType purple_session_type = purple_media_get_session_type(media, sid);
	
	if (purple_session_type & PURPLE_MEDIA_AUDIO) {
		if (purple_session_type & PURPLE_MEDIA_VIDEO) {
			return MEDIA_TYPE__MEDIA_TYPE_BUNDLE;
		} else {
			return MEDIA_TYPE__MEDIA_TYPE_AUDIO;
		}
	} else if (purple_session_type & PURPLE_MEDIA_VIDEO) {
		return MEDIA_TYPE__MEDIA_TYPE_VIDEO;
	} else if (purple_session_type & PURPLE_MEDIA_APPLICATION) {
		return MEDIA_TYPE__MEDIA_TYPE_DATA;
	} else {
		return 0;
	}
}

static void
hangouts_pblite_media_media_session_add_cb(HangoutsAccount *ha, MediaSessionAddResponse *response, gpointer user_data)
{
	purple_debug_info("hangouts", "hangouts_pblite_media_media_session_add_cb: ");
	hangouts_default_response_dump(ha, &response->base, user_data);
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
	
	client_contents = &client_content;
	media_session.n_client_content = 1;
	media_session.client_content = &client_contents;
	
	client_content.has_media_type = TRUE;
	client_content.media_type = hangout_get_session_media_type(media, sid);
	
	media_sessions = &media_session;
	request.n_resource = 1;
	request.resource = &media_sessions;
	
	
	hangouts_pblite_media_media_session_add(hangouts_media->ha, &request, hangouts_pblite_media_media_session_add_cb, hangouts_media);
	
	g_free(ice_candidates);
}

static void
hangouts_media_codecs_changed_cb(PurpleMedia *media, gchar *sid, HangoutsMedia *hangouts_media)
{
	MediaSessionAddRequest request;
	MediaSession media_session;
	MediaSession *media_sessions;
	MediaContent client_content;
	MediaContent *client_contents;
	
	GList *purple_codecs;
	MediaCodec *codecs;
	guint n_codecs;
	guint i, j;
	MediaType media_type;
	GList *purple_codec_params;
	MediaCodecParam *params;
	guint n_params;
	
	media_session_add_request__init(&request);
	media_session__init(&media_session);
	media_content__init(&client_content);
	
	media_type = hangout_get_session_media_type(media, sid);
	
	purple_codecs = purple_media_get_codecs(media, sid);
	n_codecs = g_list_length(purple_codecs);
	codecs = g_new0(MediaCodec, n_codecs);
	for(i = 0; purple_codecs; purple_codecs = g_list_next(purple_codecs), i++) {
		PurpleMediaCodec *purple_codec = purple_codecs->data;
		MediaCodec codec = codecs[i];
		media_codec__init(&codec);
		
		codec.has_payload_id = TRUE;
		codec.payload_id = _purple_media_codec_get_id(purple_codec);
		
		codec.name = _purple_media_codec_get_encoding_name(purple_codec);
		
		codec.has_media_type = TRUE;
		codec.media_type = media_type;
		
		//codec.preference = ; not set
		
		codec.has_sample_rate = TRUE;
		codec.sample_rate = _purple_media_codec_get_clock_rate(purple_codec);
		
		if (g_strcmp0(codec.name, "PCMA") == 0 ||
			g_strcmp0(codec.name, "PCMU") == 0) {
			
			codec.has_bit_rate = TRUE;
			codec.bit_rate = 48000;
		}
		
		codec.has_channel_count = TRUE;
		codec.channel_count = _purple_media_codec_get_channels(purple_codec);
		
		purple_codec_params = _purple_media_codec_get_optional_parameters(purple_codec);
		n_params = g_list_length(purple_codec_params);
		params = g_new0(MediaCodecParam, n_params);
		for(j = 0; purple_codec_params; purple_codec_params = g_list_next(purple_codec_params), j++) {
			PurpleKeyValuePair *param_info = purple_codec_params->data;
			MediaCodecParam param = params[i];
			media_codec_param__init(&param);
			
			param.key = param_info->key;
			param.value = param_info->value;
		}
		codec.n_param = n_params;
		codec.param = &params;
	}
	
	client_content.n_codec = n_codecs;
	client_content.codec = &codecs;
	
	//TODO is this correct?
	media_session.session_id = sid;
	
	client_contents = &client_content;
	media_session.n_client_content = 1;
	media_session.client_content = &client_contents;
	
	client_content.has_media_type = TRUE;
	client_content.media_type = hangout_get_session_media_type(media, sid);
	
	media_sessions = &media_session;
	request.n_resource = 1;
	request.resource = &media_sessions;
	
	hangouts_pblite_media_media_session_add(hangouts_media->ha, &request, hangouts_pblite_media_media_session_add_cb, hangouts_media);
	
	
	for(i = 0; i < n_codecs; i++) {
		g_free(codecs[i].param);
	}
	g_free(codecs);
}


static void
hangouts_pblite_media_hangout_resolve_cb(HangoutsAccount *ha, HangoutResolveResponse *response, gpointer user_data)
{
	HangoutsMedia *hangouts_media = user_data;
	
	hangouts_media->hangout_id = g_strdup(response->hangout_id);
	
	hangouts_default_response_dump(ha, &response->base, user_data);
	
	//Add self to hangout
	{
		HangoutParticipantAddRequest participant_request;
		HangoutParticipant participant, *participant_ptr = &participant;
		
		hangout_participant_add_request__init(&participant_request);
		hangout_participant__init(&participant);
		
		participant.hangout_id = response->hangout_id;
		participant_request.n_resource = 1;
		participant_request.resource = &participant_ptr;
		
		hangouts_pblite_media_hangout_participant_add(ha, &participant_request, (HangoutsPbliteHangoutParticipantAddResponseFunc)hangouts_default_response_dump, NULL);
	}
	
	//Add remote to hangout
	{
		HangoutInvitationAddRequest invitation_request;
		HangoutInvitation invitation;
		HangoutInvitee invitee, *invitee_ptr = &invitee;
		HangoutSharingTargetId sharing_target_id;
		PersonId person_id;
		
		hangout_invitation_add_request__init(&invitation_request);
		hangout_invitation__init(&invitation);
		person_id__init(&person_id);
		hangout_sharing_target_id__init(&sharing_target_id);
		hangout_invitee__init(&invitee);
		
		invitation.hangout_id = response->hangout_id;
		person_id.user_id = hangouts_media->who;
		sharing_target_id.person_id = &person_id;
		invitee.invitee = &sharing_target_id;
		invitation.n_invited_entity = 1;
		invitation.invited_entity = &invitee_ptr;
		
		hangouts_pblite_media_hangout_invitation_add(ha, &invitation_request, (HangoutsPbliteHangoutInvitationAddResponseFunc)hangouts_default_response_dump, NULL);
	}
}

gboolean
hangouts_initiate_media(PurpleAccount *account, const gchar *who, PurpleMediaSessionType type)
{
	PurpleConnection *pc = purple_account_get_connection(account);
	HangoutsAccount *ha = purple_connection_get_protocol_data(pc);
	HangoutsMedia *hangouts_media;
	GParameter *params = NULL;
	gint num_params = 0;
	HangoutResolveRequest request;
	ExternalKey external_key;
	
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
	hangouts_media->who = g_strdup(who);
	purple_media_set_protocol_data(media, hangouts_media);
	
	g_signal_connect(G_OBJECT(media), "candidates-prepared",
				 G_CALLBACK(hangouts_media_candidates_prepared_cb), hangouts_media);
	g_signal_connect(G_OBJECT(media), "codecs-changed",
				 G_CALLBACK(hangouts_media_codecs_changed_cb), hangouts_media);
	// TODO
	// g_signal_connect(G_OBJECT(media), "state-changed",
				 // G_CALLBACK(hangouts_media_state_changed_cb), hangouts_media);
	// g_signal_connect(G_OBJECT(media), "stream-info",
			// G_CALLBACK(hangouts_media_stream_info_cb), hangouts_media);
	
	//TODO add params
	
	if (type & PURPLE_MEDIA_AUDIO) {
		if(!purple_media_add_stream(media, "audio", who, PURPLE_MEDIA_AUDIO, TRUE, "nice", num_params, params)) {
			purple_media_end(media, NULL, NULL);
			/* TODO: How much clean-up is necessary here? (does calling
					 purple_media_end lead to cleaning up Jingle structs?) */
			return FALSE;
		}
	}
	if (type & PURPLE_MEDIA_VIDEO) {
		if(!purple_media_add_stream(media, "video", who, PURPLE_MEDIA_VIDEO, TRUE, "nice", num_params, params)) {
			purple_media_end(media, NULL, NULL);
			/* TODO: How much clean-up is necessary here? (does calling
					 purple_media_end lead to cleaning up Jingle structs?) */
			return FALSE;
		}
	}
	
	hangout_resolve_request__init(&request);
	external_key__init(&external_key);
	external_key.service = "CONVERSATION";
	external_key.value = g_hash_table_lookup(ha->one_to_ones_rev, who);
	request.external_key = &external_key;
	hangouts_pblite_media_hangout_resolve(ha, &request, hangouts_pblite_media_hangout_resolve_cb, hangouts_media);
	
	return TRUE;
}
