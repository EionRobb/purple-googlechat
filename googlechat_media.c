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

#include "libgooglechat.h"
#include "googlechat_media.h"
#include "googlechat_conversation.h"

#include <stdlib.h>
#include <stdio.h>

#include "debug.h"
#include "account.h"
#include "connection.h"
#include "mediamanager.h"
#include "media.h"

#ifdef _WIN32
#	include <windows.h>
#	define dlopen(filename, flag)  GetModuleHandleA(filename)
#	define dlsym(handle, symbol)   GetProcAddress(handle, symbol)
#	define dlclose(handle)         FreeLibrary(handle)
static gchar *last_dlopen_error = NULL;
#	define dlerror()               (g_free(last_dlopen_error),last_dlopen_error=g_win32_error_message(GetLastError()))
#	define RTLD_LAZY               0x0001
#else
#	include <dlfcn.h>
#endif

/*
 * Dynamic linking function pointers for libpurple voice/video API.
 * Functions are loaded at runtime using dlopen/dlsym (or GetProcAddress on Windows)
 * to allow maximum compatibility across systems with or without USE_VV.
 */
static PurpleMediaManager *(*_purple_media_manager_get)(void) = NULL;
static PurpleMedia *(*_purple_media_manager_create_media)(PurpleMediaManager *manager,
                                                         PurpleAccount *account,
                                                         const char *conference_type,
                                                         const char *remote_user,
                                                         gboolean initiator) = NULL;
static gpointer (*_purple_media_manager_get_pipeline)(PurpleMediaManager *manager) = NULL;
static GType (*_purple_media_manager_get_type)(void) = NULL;
static GType (*_purple_media_get_type)(void) = NULL;
static gboolean (*_purple_media_add_stream)(PurpleMedia *media,
                                           const gchar *sess_id,
                                           const gchar *who,
                                           PurpleMediaSessionType type,
                                           gboolean initiator,
                                           const gchar *transmitter,
                                           guint num_params,
                                           GParameter *params) = NULL;
static void (*_purple_media_end)(PurpleMedia *media,
                                const gchar *session_id,
                                const gchar *participant) = NULL;
static void (*_purple_media_stream_info)(PurpleMedia *media,
                                        PurpleMediaInfoType type,
                                        const gchar *session_id,
                                        const gchar *participant,
                                        gboolean local) = NULL;
static PurpleMediaSessionType (*_purple_media_get_session_type)(PurpleMedia *media,
                                                               const gchar *sess_id) = NULL;
static GList *(*_purple_media_get_session_ids)(PurpleMedia *media) = NULL;
static GList *(*_purple_media_get_local_candidates)(PurpleMedia *media,
                                                    const gchar *sess_id,
                                                    const gchar *participant) = NULL;
static void (*_purple_media_add_remote_candidates)(PurpleMedia *media,
                                                  const gchar *sess_id,
                                                  const gchar *participant,
                                                  GList *remote_candidates) = NULL;
static GList *(*_purple_media_get_codecs)(PurpleMedia *media,
                                         const gchar *sess_id) = NULL;
static gboolean (*_purple_media_set_remote_codecs)(PurpleMedia *media,
                                                  const gchar *sess_id,
                                                  const gchar *participant,
                                                  GList *codecs) = NULL;
static gboolean (*_purple_media_candidates_prepared)(PurpleMedia *media,
                                                    const gchar *session_id,
                                                    const gchar *participant) = NULL;
static gboolean (*_purple_media_codecs_ready)(PurpleMedia *media,
                                             const gchar *sess_id) = NULL;
static gboolean (*_purple_media_set_send_rtcp_mux)(PurpleMedia *media,
                                                  const gchar *sess_id,
                                                  const gchar *participant,
                                                  gboolean send_rtcp_mux) = NULL;
static gboolean (*_purple_media_set_send_codec)(PurpleMedia *media,
                                               const gchar *sess_id,
                                               PurpleMediaCodec *codec) = NULL;
static gboolean (*_purple_media_set_encryption_parameters)(PurpleMedia *media,
                                                          const gchar *sess_id,
                                                          const gchar *cipher,
                                                          const gchar *auth,
                                                          const gchar *key,
                                                          gsize key_len) = NULL;
static gboolean (*_purple_media_set_decryption_parameters)(PurpleMedia *media,
                                                          const gchar *sess_id,
                                                          const gchar *participant,
                                                          const gchar *cipher,
                                                          const gchar *auth,
                                                          const gchar *key,
                                                          gsize key_len) = NULL;
static gboolean (*_purple_media_accepted)(PurpleMedia *media,
                                         const gchar *sess_id,
                                         const gchar *participant) = NULL;
static gboolean (*_purple_media_is_initiator)(PurpleMedia *media,
                                             const gchar *sess_id,
                                             const gchar *participant) = NULL;
static void (*_purple_media_set_protocol_data)(PurpleMedia *media,
                                              gpointer protocol_data) = NULL;
static gpointer (*_purple_media_get_protocol_data)(PurpleMedia *media) = NULL;

static PurpleMediaCandidate *(*_purple_media_candidate_new)(const gchar *foundation,
                                                           guint component_id,
                                                           PurpleMediaCandidateType type,
                                                           PurpleMediaNetworkProtocol proto,
                                                           const gchar *ip,
                                                           guint port) = NULL;
static gchar *(*_purple_media_candidate_get_username)(PurpleMediaCandidate *candidate) = NULL;
static gchar *(*_purple_media_candidate_get_password)(PurpleMediaCandidate *candidate) = NULL;
static PurpleMediaNetworkProtocol (*_purple_media_candidate_get_protocol)(PurpleMediaCandidate *candidate) = NULL;
static guint (*_purple_media_candidate_get_component_id)(PurpleMediaCandidate *candidate) = NULL;
static gchar *(*_purple_media_candidate_get_ip)(PurpleMediaCandidate *candidate) = NULL;
static guint16 (*_purple_media_candidate_get_port)(PurpleMediaCandidate *candidate) = NULL;
static PurpleMediaCandidateType (*_purple_media_candidate_get_candidate_type)(PurpleMediaCandidate *candidate) = NULL;
static guint32 (*_purple_media_candidate_get_priority)(PurpleMediaCandidate *candidate) = NULL;

static PurpleMediaCodec *(*_purple_media_codec_new)(int id,
                                                   const char *encoding_name,
                                                   PurpleMediaSessionType media_type,
                                                   guint clock_rate) = NULL;
static guint (*_purple_media_codec_get_id)(PurpleMediaCodec *codec) = NULL;
static gchar *(*_purple_media_codec_get_encoding_name)(PurpleMediaCodec *codec) = NULL;
static guint (*_purple_media_codec_get_clock_rate)(PurpleMediaCodec *codec) = NULL;
static guint (*_purple_media_codec_get_channels)(PurpleMediaCodec *codec) = NULL;
static GList *(*_purple_media_codec_get_optional_parameters)(PurpleMediaCodec *codec) = NULL;
static void (*_purple_media_codec_add_optional_parameter)(PurpleMediaCodec *codec,
                                                         const gchar *name,
                                                         const gchar *value) = NULL;

static GType (*_purple_media_backend_fs2_get_type)(void) = NULL;

static void (*_purple_prpl_got_media_caps)(PurpleAccount *account,
                                          const char *who) = NULL;

/* Undefine conflicting compat macros before redefining to our function pointers */
#ifdef purple_media_set_protocol_data
#	undef purple_media_set_protocol_data
#endif
#ifdef purple_media_get_protocol_data
#	undef purple_media_get_protocol_data
#endif

#define purple_media_manager_get                  _purple_media_manager_get
#define purple_media_manager_create_media         _purple_media_manager_create_media
#define purple_media_manager_get_pipeline         _purple_media_manager_get_pipeline
#define purple_media_manager_get_type             _purple_media_manager_get_type
#define purple_media_get_type                     _purple_media_get_type
#define purple_media_add_stream                   _purple_media_add_stream
#define purple_media_end                          _purple_media_end
#define purple_media_stream_info                  _purple_media_stream_info
#define purple_media_get_session_type             _purple_media_get_session_type
#define purple_media_get_session_ids              _purple_media_get_session_ids
#define purple_media_get_local_candidates         _purple_media_get_local_candidates
#define purple_media_add_remote_candidates        _purple_media_add_remote_candidates
#define purple_media_get_codecs                   _purple_media_get_codecs
#define purple_media_set_remote_codecs            _purple_media_set_remote_codecs
#define purple_media_candidates_prepared          _purple_media_candidates_prepared
#define purple_media_codecs_ready                 _purple_media_codecs_ready
#define purple_media_set_send_rtcp_mux            _purple_media_set_send_rtcp_mux
#define purple_media_set_send_codec               _purple_media_set_send_codec
#define purple_media_set_encryption_parameters    _purple_media_set_encryption_parameters
#define purple_media_set_decryption_parameters    _purple_media_set_decryption_parameters
#define purple_media_accepted                     _purple_media_accepted
#define purple_media_is_initiator                 _purple_media_is_initiator
#define purple_media_set_protocol_data            _purple_media_set_protocol_data
#define purple_media_get_protocol_data            _purple_media_get_protocol_data
#define purple_media_candidate_new                _purple_media_candidate_new
#define purple_media_candidate_get_username       _purple_media_candidate_get_username
#define purple_media_candidate_get_password       _purple_media_candidate_get_password
#define purple_media_candidate_get_protocol       _purple_media_candidate_get_protocol
#define purple_media_candidate_get_component_id   _purple_media_candidate_get_component_id
#define purple_media_candidate_get_ip             _purple_media_candidate_get_ip
#define purple_media_candidate_get_port           _purple_media_candidate_get_port
#define purple_media_candidate_get_candidate_type _purple_media_candidate_get_candidate_type
#define purple_media_candidate_get_priority       _purple_media_candidate_get_priority
#define purple_media_codec_new                    _purple_media_codec_new
#define purple_media_codec_get_id                 _purple_media_codec_get_id
#define purple_media_codec_get_encoding_name      _purple_media_codec_get_encoding_name
#define purple_media_codec_get_clock_rate         _purple_media_codec_get_clock_rate
#define purple_media_codec_get_channels           _purple_media_codec_get_channels
#define purple_media_codec_get_optional_parameters _purple_media_codec_get_optional_parameters
#define purple_media_codec_add_optional_parameter _purple_media_codec_add_optional_parameter
#define purple_media_backend_fs2_get_type         _purple_media_backend_fs2_get_type
#define purple_prpl_got_media_caps                _purple_prpl_got_media_caps

typedef struct {
	GoogleChatAccount *ha;
	PurpleMedia *media;
	gchar *who;
	PurpleMediaSessionType type;
	gchar *conv_id;
} GoogleChatMedia;

static gboolean media_initialized = FALSE;
static gboolean media_supported = FALSE;

static gpointer libpurple_module = NULL;

static gpointer
googlechat_media_find_symbol(const gchar *symbol_name)
{
	gpointer sym = NULL;

#ifdef _WIN32
	gpointer handle;

	/* First check pidgin-winvv.dll in case it provides VV on Windows */
	handle = dlopen("pidgin-winvv.dll", RTLD_LAZY);
	if (handle != NULL && (sym = (gpointer) dlsym(handle, symbol_name)) != NULL) {
		return sym;
	}

	/* Next check libpurple.dll */
	handle = dlopen("libpurple.dll", RTLD_LAZY);
	if (handle != NULL && (sym = (gpointer) dlsym(handle, symbol_name)) != NULL) {
		return sym;
	}
#endif

	if (libpurple_module == NULL) {
		libpurple_module = dlopen(NULL, RTLD_LAZY);
		if (libpurple_module == NULL) {
			purple_debug_warning("googlechat", "Couldn't dlopen libpurple: %s\n", dlerror());
		}
	}
	if (libpurple_module != NULL) {
		sym = (gpointer) dlsym(libpurple_module, symbol_name);
		if (sym != NULL) {
			return sym;
		}
	}

#ifdef RTLD_DEFAULT
	sym = (gpointer) dlsym(RTLD_DEFAULT, symbol_name);
	if (sym != NULL) {
		return sym;
	}
#endif

	return NULL;
}

static void
googlechat_media_destroy(GoogleChatMedia *gc_media)
{
	if (gc_media == NULL) {
		return;
	}

	if (gc_media->media != NULL && _purple_media_set_protocol_data != NULL) {
		_purple_media_set_protocol_data(gc_media->media, NULL);
	}

	g_free(gc_media->who);
	g_free(gc_media->conv_id);
	g_free(gc_media);
}

static void
googlechat_media_state_changed_cb(PurpleMedia *media, PurpleMediaState state,
                                  gchar *sid, gchar *name, GoogleChatMedia *gc_media)
{
	(void)media;
	purple_debug_info("googlechat", "googlechat_media_state_changed_cb: state=%d, sid=%s, name=%s\n",
	                  state, sid ? sid : "(null)", name ? name : "(null)");

	if (state == PURPLE_MEDIA_STATE_END) {
		googlechat_media_destroy(gc_media);
	}
}

static void
googlechat_media_stream_info_cb(PurpleMedia *media, PurpleMediaInfoType type,
                                gchar *sid, gchar *name, gboolean local,
                                GoogleChatMedia *gc_media)
{
	(void)gc_media;
	purple_debug_info("googlechat", "googlechat_media_stream_info_cb: type=%d, sid=%s, name=%s, local=%d\n",
	                  type, sid ? sid : "(null)", name ? name : "(null)", local);

	if (type == PURPLE_MEDIA_INFO_HANGUP || type == PURPLE_MEDIA_INFO_REJECT) {
		if (_purple_media_end != NULL) {
			_purple_media_end(media, sid, name);
		}
	}
}

static void
googlechat_media_candidates_prepared_cb(PurpleMedia *media, gchar *sid, gchar *name, GoogleChatMedia *gc_media)
{
	(void)media;
	(void)gc_media;
	purple_debug_info("googlechat", "googlechat_media_candidates_prepared_cb: sid=%s, name=%s\n",
	                  sid ? sid : "(null)", name ? name : "(null)");
}

static void
googlechat_media_codecs_changed_cb(PurpleMedia *media, gchar *sid, GoogleChatMedia *gc_media)
{
	(void)media;
	(void)gc_media;
	purple_debug_info("googlechat", "googlechat_media_codecs_changed_cb: sid=%s\n",
	                  sid ? sid : "(null)");
}

gboolean
googlechat_media_init(void)
{
	if (media_initialized && media_supported) {
		return TRUE;
	}

	_purple_media_manager_get = (gpointer) googlechat_media_find_symbol("purple_media_manager_get");
	_purple_media_manager_create_media = (gpointer) googlechat_media_find_symbol("purple_media_manager_create_media");
	_purple_media_manager_get_pipeline = (gpointer) googlechat_media_find_symbol("purple_media_manager_get_pipeline");
	_purple_media_manager_get_type = (gpointer) googlechat_media_find_symbol("purple_media_manager_get_type");
	_purple_media_get_type = (gpointer) googlechat_media_find_symbol("purple_media_get_type");
	_purple_media_add_stream = (gpointer) googlechat_media_find_symbol("purple_media_add_stream");
	_purple_media_end = (gpointer) googlechat_media_find_symbol("purple_media_end");
	_purple_media_stream_info = (gpointer) googlechat_media_find_symbol("purple_media_stream_info");
	_purple_media_get_session_type = (gpointer) googlechat_media_find_symbol("purple_media_get_session_type");
	_purple_media_get_session_ids = (gpointer) googlechat_media_find_symbol("purple_media_get_session_ids");
	_purple_media_get_local_candidates = (gpointer) googlechat_media_find_symbol("purple_media_get_local_candidates");
	_purple_media_add_remote_candidates = (gpointer) googlechat_media_find_symbol("purple_media_add_remote_candidates");
	_purple_media_get_codecs = (gpointer) googlechat_media_find_symbol("purple_media_get_codecs");
	_purple_media_set_remote_codecs = (gpointer) googlechat_media_find_symbol("purple_media_set_remote_codecs");
	_purple_media_candidates_prepared = (gpointer) googlechat_media_find_symbol("purple_media_candidates_prepared");
	_purple_media_codecs_ready = (gpointer) googlechat_media_find_symbol("purple_media_codecs_ready");
	_purple_media_set_send_rtcp_mux = (gpointer) googlechat_media_find_symbol("purple_media_set_send_rtcp_mux");
	_purple_media_set_send_codec = (gpointer) googlechat_media_find_symbol("purple_media_set_send_codec");
	_purple_media_set_encryption_parameters = (gpointer) googlechat_media_find_symbol("purple_media_set_encryption_parameters");
	_purple_media_set_decryption_parameters = (gpointer) googlechat_media_find_symbol("purple_media_set_decryption_parameters");

	_purple_media_accepted = (gpointer) googlechat_media_find_symbol("purple_media_accepted");
	_purple_media_is_initiator = (gpointer) googlechat_media_find_symbol("purple_media_is_initiator");

	/* purple_media_set_protocol_data (purple3) or purple_media_set_prpl_data (purple2) */
	_purple_media_set_protocol_data = (gpointer) googlechat_media_find_symbol("purple_media_set_protocol_data");
	if (_purple_media_set_protocol_data == NULL) {
		_purple_media_set_protocol_data = (gpointer) googlechat_media_find_symbol("purple_media_set_prpl_data");
	}

	_purple_media_get_protocol_data = (gpointer) googlechat_media_find_symbol("purple_media_get_protocol_data");
	if (_purple_media_get_protocol_data == NULL) {
		_purple_media_get_protocol_data = (gpointer) googlechat_media_find_symbol("purple_media_get_prpl_data");
	}

	_purple_media_candidate_new = (gpointer) googlechat_media_find_symbol("purple_media_candidate_new");
	_purple_media_candidate_get_username = (gpointer) googlechat_media_find_symbol("purple_media_candidate_get_username");
	_purple_media_candidate_get_password = (gpointer) googlechat_media_find_symbol("purple_media_candidate_get_password");
	_purple_media_candidate_get_protocol = (gpointer) googlechat_media_find_symbol("purple_media_candidate_get_protocol");
	_purple_media_candidate_get_component_id = (gpointer) googlechat_media_find_symbol("purple_media_candidate_get_component_id");
	_purple_media_candidate_get_ip = (gpointer) googlechat_media_find_symbol("purple_media_candidate_get_ip");
	_purple_media_candidate_get_port = (gpointer) googlechat_media_find_symbol("purple_media_candidate_get_port");
	_purple_media_candidate_get_candidate_type = (gpointer) googlechat_media_find_symbol("purple_media_candidate_get_candidate_type");
	_purple_media_candidate_get_priority = (gpointer) googlechat_media_find_symbol("purple_media_candidate_get_priority");

	_purple_media_codec_new = (gpointer) googlechat_media_find_symbol("purple_media_codec_new");
	_purple_media_codec_get_id = (gpointer) googlechat_media_find_symbol("purple_media_codec_get_id");
	_purple_media_codec_get_encoding_name = (gpointer) googlechat_media_find_symbol("purple_media_codec_get_encoding_name");
	_purple_media_codec_get_clock_rate = (gpointer) googlechat_media_find_symbol("purple_media_codec_get_clock_rate");
	_purple_media_codec_get_channels = (gpointer) googlechat_media_find_symbol("purple_media_codec_get_channels");
	_purple_media_codec_get_optional_parameters = (gpointer) googlechat_media_find_symbol("purple_media_codec_get_optional_parameters");
	_purple_media_codec_add_optional_parameter = (gpointer) googlechat_media_find_symbol("purple_media_codec_add_optional_parameter");

	_purple_media_backend_fs2_get_type = (gpointer) googlechat_media_find_symbol("purple_media_backend_fs2_get_type");

	_purple_prpl_got_media_caps = (gpointer) googlechat_media_find_symbol("purple_prpl_got_media_caps");

	media_initialized = TRUE;

	if (_purple_media_manager_get != NULL &&
	    _purple_media_manager_create_media != NULL &&
	    _purple_media_add_stream != NULL) {
		PurpleMediaManager *manager = _purple_media_manager_get();
		if (manager != NULL) {
			media_supported = TRUE;
			purple_debug_info("googlechat", "libpurple media subsystem dynamically linked and active\n");
			return TRUE;
		}
	}

	media_supported = FALSE;
	purple_debug_info("googlechat", "libpurple media subsystem not available\n");
	return FALSE;
}

void
googlechat_media_uninit(void)
{
	media_initialized = FALSE;
	media_supported = FALSE;

#ifndef _WIN32
	if (libpurple_module != NULL) {
		dlclose(libpurple_module);
	}
#else
	g_free(last_dlopen_error);
	last_dlopen_error = NULL;
#endif
	libpurple_module = NULL;
}

gboolean
googlechat_media_is_supported(void)
{
	if (!media_initialized) {
		googlechat_media_init();
	}

	if (!media_supported) {
		/* Try re-initializing once in case pidgin-winvv or media provider was loaded after us */
		googlechat_media_init();
	}

	return media_supported;
}

PurpleMediaCaps
googlechat_get_media_caps(PurpleAccount *account, const char *who)
{
	(void)account;
	(void)who;

	if (!googlechat_media_is_supported()) {
		return PURPLE_MEDIA_CAPS_NONE;
	}

	return PURPLE_MEDIA_CAPS_AUDIO |
	       PURPLE_MEDIA_CAPS_VIDEO |
	       PURPLE_MEDIA_CAPS_AUDIO_VIDEO |
	       PURPLE_MEDIA_CAPS_MODIFY_SESSION;
}

gboolean
googlechat_initiate_media(PurpleAccount *account, const gchar *who, PurpleMediaSessionType type)
{
	PurpleConnection *pc;
	GoogleChatAccount *ha;
	const gchar *conv_id = NULL;
	PurpleMediaManager *manager;
	PurpleMedia *media;
	GoogleChatMedia *gc_media;

	if (account == NULL || who == NULL) {
		return FALSE;
	}

	pc = purple_account_get_connection(account);
	if (pc == NULL) {
		return FALSE;
	}

	ha = purple_connection_get_protocol_data(pc);
	if (ha == NULL) {
		return FALSE;
	}

	/* Try to resolve conv_id for the remote user */
	if (ha->one_to_ones_rev != NULL) {
		conv_id = g_hash_table_lookup(ha->one_to_ones_rev, who);
	}
	if (conv_id == NULL) {
		/* Maybe who is already a conv_id or gaia_id */
		conv_id = who;
	}

	/* Initiate the Google Chat Meet / video call via the chat API */
	if (conv_id != NULL) {
		googlechat_video_call_conversation(ha, conv_id);
	}

	/* If libpurple media is not supported or not loaded, we still initiated via Google Chat */
	if (!googlechat_media_is_supported()) {
		purple_debug_info("googlechat", "libpurple media not available; initiated Meet call via chat\n");
		return TRUE;
	}

	manager = _purple_media_manager_get();
	if (manager == NULL) {
		return TRUE;
	}

	/* Create the PurpleMedia instance */
	media = _purple_media_manager_create_media(manager, account, "fsrtpconference", who, TRUE);
	if (media == NULL) {
		purple_debug_warning("googlechat", "purple_media_manager_create_media returned NULL\n");
		return TRUE;
	}

	gc_media = g_new0(GoogleChatMedia, 1);
	gc_media->ha = ha;
	gc_media->media = media;
	gc_media->who = g_strdup(who);
	gc_media->type = type;
	gc_media->conv_id = g_strdup(conv_id);

	if (_purple_media_set_protocol_data != NULL) {
		_purple_media_set_protocol_data(media, gc_media);
	}

	g_signal_connect(G_OBJECT(media), "candidates-prepared",
	                 G_CALLBACK(googlechat_media_candidates_prepared_cb), gc_media);
	g_signal_connect(G_OBJECT(media), "codecs-changed",
	                 G_CALLBACK(googlechat_media_codecs_changed_cb), gc_media);
	g_signal_connect(G_OBJECT(media), "state-changed",
	                 G_CALLBACK(googlechat_media_state_changed_cb), gc_media);
	g_signal_connect(G_OBJECT(media), "stream-info",
	                 G_CALLBACK(googlechat_media_stream_info_cb), gc_media);

	if (type & PURPLE_MEDIA_AUDIO) {
		if (_purple_media_add_stream != NULL &&
		    !_purple_media_add_stream(media, "audio", who, PURPLE_MEDIA_AUDIO, TRUE, "nice", 0, NULL)) {
			purple_debug_error("googlechat", "Error adding audio stream to media session\n");
			if (_purple_media_end != NULL) {
				_purple_media_end(media, NULL, NULL);
			}
			return TRUE;
		}
		if (_purple_media_set_send_rtcp_mux != NULL) {
			_purple_media_set_send_rtcp_mux(media, "audio", who, TRUE);
		}
	}

	if (type & PURPLE_MEDIA_VIDEO) {
		if (_purple_media_add_stream != NULL &&
		    !_purple_media_add_stream(media, "video", who, PURPLE_MEDIA_VIDEO, TRUE, "nice", 0, NULL)) {
			purple_debug_error("googlechat", "Error adding video stream to media session\n");
			if (_purple_media_end != NULL) {
				_purple_media_end(media, NULL, NULL);
			}
			return TRUE;
		}
		if (_purple_media_set_send_rtcp_mux != NULL) {
			_purple_media_set_send_rtcp_mux(media, "video", who, TRUE);
		}
	}

	return TRUE;
}

void
googlechat_media_got_caps(PurpleAccount *account, const gchar *who)
{
	if (_purple_prpl_got_media_caps != NULL) {
		_purple_prpl_got_media_caps(account, who);
	}
}
