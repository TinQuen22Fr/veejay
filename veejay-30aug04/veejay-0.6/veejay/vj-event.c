/* 
 * Linux VeeJay
 *
 * Copyright(C)2002-2004 Niels Elburg <nelburg@looze.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License , or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 */


#include <config.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <SDL/SDL.h>
#include <stdarg.h>
#include "vj-effect.h"
#include "vj-common.h"
#include "vj-rgb.h"
#include "vj-vloopback.h"
#include "vj-tag.h"
#include "sampleadm.h"
#include "samplerecord.h"
#include "vj-motion.h"
#include "vj-event.h"
#include "vj-lib.h"
#include "vj-keyframe.h"
#include "vj-perform.h"
#include "hash.h"
#include "libveejay.h"
#include <mpegconsts.h>
#include <mpegtimecode.h>
#include "effects/diff.h" 

/* Highest possible SDL Key identifier */
#define MAX_SDL_KEY	350
#define NET_MAX		256
#define MSG_MIN_LEN	  5


static hash_t *BundleHash;
static hash_t *MacroHash;

static int vj_event_valid_mode(int mode) {
	switch(mode) {
	 case VJ_PLAYBACK_MODE_CLIP:
	 case VJ_PLAYBACK_MODE_TAG:
	 case VJ_PLAYBACK_MODE_PLAIN:
	 return 1;
	}

	return 0;
}

/* define the function pointer to any event */
typedef void (*vj_event)(void *ptr, const char format[], va_list ap);

void vj_event_create_effect_bundle(veejay_t * v,char *buf, int key_id );
int  register_new_bundle_as_key(int sdl_key, int modifier); 

/* struct for runtime initialization of event handlers */
typedef struct {
	int list_id;
	vj_event act;
} vj_events;

/* 4 arrays for 4 keyboard layers, depending on key modifier */

static vj_events sdl_normal[MAX_SDL_KEY];
static vj_events sdl_alt[MAX_SDL_KEY];
static vj_events sdl_ctrl[MAX_SDL_KEY];
static vj_events sdl_shift[MAX_SDL_KEY];
static int sdl_parameter[MAX_SDL_KEY]; /* need at most 1 parameter for custom identifying custom events */
static int _recorder_format = -1;
static int  argument_list[16];

static vj_events net_list[NET_MAX];
static long windowID = 0;
#define SEND_BUF 125000
static char _print_buf[SEND_BUF];
static char _s_print_buf[SEND_BUF];


int vj_event_get_video_format()
{
	return _recorder_format;
}

enum {
	VJ_ERROR_NONE=0,	
	VJ_ERROR_MODE=1,
	VJ_ERROR_EXISTS=2,
	VJ_ERROR_EVENT=3,
	VJ_ERROR_DIMEN=4,
	VJ_ERROR_MEM=5,
	VJ_ERROR_INVALID_MODE = 6,
};

/* some error information */

static struct {
	int error_id;
	char *str;
} vj_event_errors[] = {
	{ VJ_ERROR_NONE	, 	"Success" 							},
	{ VJ_ERROR_MODE ,	"Cannot perform this action in current play back mode"		},
	{ VJ_ERROR_EXISTS,	"Requested clip/tag does not exist"				},
	{ VJ_ERROR_EVENT,	"No such event"							},
	{ VJ_ERROR_DIMEN,	"Invalid dimensions given"					},
	{ VJ_ERROR_MEM,		"Cannot allocate free memory!"					},
	{ VJ_ERROR_INVALID_MODE,"Invalid playback mode. 1=Plain 2=Video clip 4=Stream"		},
};


enum {				/* all events, network/keyboard crossover */
	VJ_EVENT_VIDEO_PLAY_FORWARD			=	1001,
	VJ_EVENT_VIDEO_PLAY_BACKWARD			=	1002,
	VJ_EVENT_VIDEO_PLAY_STOP			=	1003,
	VJ_EVENT_VIDEO_SKIP_FRAME			=	1004,
	VJ_EVENT_VIDEO_PREV_FRAME			=	1005,
	VJ_EVENT_VIDEO_SKIP_SECOND			=	1006,
	VJ_EVENT_VIDEO_PREV_SECOND			=	1007,
	VJ_EVENT_VIDEO_SPEED1				=	1008,
	VJ_EVENT_VIDEO_SPEED2				=	1009,
	VJ_EVENT_VIDEO_SPEED3				=	1010,
	VJ_EVENT_VIDEO_SPEED4				=	1011,
	VJ_EVENT_VIDEO_SPEED5				=	1012,
	VJ_EVENT_VIDEO_SPEED6				=	1013,
	VJ_EVENT_VIDEO_SPEED7				=	1014,
	VJ_EVENT_VIDEO_SPEED8				=	1015,
	VJ_EVENT_VIDEO_SPEED9				=	1016,
	VJ_EVENT_VIDEO_SLOW0				=	1017,
	VJ_EVENT_VIDEO_SLOW1				=	1018,
	VJ_EVENT_VIDEO_SLOW2				=	1019,
	VJ_EVENT_VIDEO_SLOW3				=	1020,
	VJ_EVENT_VIDEO_SLOW4				=	1021,
	VJ_EVENT_VIDEO_SLOW5				=	1022,
	VJ_EVENT_VIDEO_SLOW6				=	1023,
	VJ_EVENT_VIDEO_SLOW7				=	1024,
	VJ_EVENT_VIDEO_SLOW8				=	1025,
	VJ_EVENT_ENTRY_UP				=	1026,
	VJ_EVENT_ENTRY_DOWN				=	1027,
	VJ_EVENT_ENTRY_CHANNEL_UP			=	1028,
	VJ_EVENT_ENTRY_CHANNEL_DOWN			=	1029,
	VJ_EVENT_ENTRY_SOURCE_TOGGLE			=	1030,
	VJ_EVENT_SET_PLAIN_MODE				=	1031,
	VJ_EVENT_ENTRY_INC_ARG0				=	1032,
	VJ_EVENT_ENTRY_INC_ARG1				=	1033,
	VJ_EVENT_ENTRY_INC_ARG2				=	1034,
	VJ_EVENT_ENTRY_INC_ARG3				=	1035,
	VJ_EVENT_ENTRY_INC_ARG4				=	1036,
	VJ_EVENT_ENTRY_INC_ARG5				=	1037,
	VJ_EVENT_ENTRY_INC_ARG6				=	1038,
	VJ_EVENT_ENTRY_INC_ARG7				=	1039,
	VJ_EVENT_ENTRY_INC_ARG8				=	1040,
	VJ_EVENT_ENTRY_INC_ARG9				=	1041,
	VJ_EVENT_ENTRY_DEC_ARG0				=	1042,
	VJ_EVENT_ENTRY_DEC_ARG1				=	1043,
	VJ_EVENT_ENTRY_DEC_ARG2				=	1044,
	VJ_EVENT_ENTRY_DEC_ARG3				=	1045,
	VJ_EVENT_ENTRY_DEC_ARG4				=	1046,
	VJ_EVENT_ENTRY_DEC_ARG5				=	1047,
	VJ_EVENT_ENTRY_DEC_ARG6				=	1048,
	VJ_EVENT_ENTRY_DEC_ARG7				=	1049,
	VJ_EVENT_ENTRY_DEC_ARG8				=	1050,
	VJ_EVENT_ENTRY_DEC_ARG9				=	1051,
	VJ_EVENT_ENTRY_VIDEO_TOGGLE			=	1052,
	VJ_EVENT_ENTRY_AUDIO_TOGGLE			=	1053,
	VJ_EVENT_ENTRY_DEL				=	1054,
	VJ_EVENT_CHAIN_TOGGLE				=	1060,
	VJ_EVENT_SET_CLIP_START				=	1061,
	VJ_EVENT_SET_CLIP_END				=	1062,
	VJ_EVENT_SET_MARKER_START			=	1063,
	VJ_EVENT_SET_MARKER_END				=	1064,
	VJ_EVENT_SELECT_EFFECT_INC			=	1070,
	VJ_EVENT_SELECT_EFFECT_DEC			=	1071,
	VJ_EVENT_SELECT_EFFECT_ADD			=	1072,
	VJ_EVENT_SELECT_BANK1				=	1080,
	VJ_EVENT_SELECT_BANK2				=	1081,
	VJ_EVENT_SELECT_BANK3				=	1082,
	VJ_EVENT_SELECT_BANK4				=	1083,
	VJ_EVENT_SELECT_BANK5				=	1084,
	VJ_EVENT_SELECT_BANK6				=	1085,
	VJ_EVENT_SELECT_BANK7				=	1086,
	VJ_EVENT_SELECT_BANK8				=	1087,
	VJ_EVENT_SELECT_BANK9				=	1088,
	VJ_EVENT_SELECT_ID1				=	1090,
	VJ_EVENT_SELECT_ID2				=	1091,
	VJ_EVENT_SELECT_ID3				=	1092,
	VJ_EVENT_SELECT_ID4				=	1093,
	VJ_EVENT_SELECT_ID5				=	1094,
	VJ_EVENT_SELECT_ID6				=	1095,
	VJ_EVENT_SELECT_ID7				=	1096,
	VJ_EVENT_SELECT_ID8				=	1097,
	VJ_EVENT_SELECT_ID9				=	1098,
	VJ_EVENT_SELECT_ID10				=	1099,
	VJ_EVENT_SELECT_ID11				=	1100,
	VJ_EVENT_SELECT_ID12				=	1101,
	VJ_EVENT_REC_AUTO_START				=	1102,
	VJ_EVENT_REC_START				=	1103,
	VJ_EVENT_REC_STOP				=	1104,
	VJ_EVENT_VIDEO_GOTO_END				=	1105,
	VJ_EVENT_VIDEO_GOTO_START			=	1106,
	VJ_EVENT_CLIP_TOGGLE_LOOP			=	1107,
	VJ_EVENT_SWITCH_CLIP_TAG			=	1109,
	VJ_EVENT_PRINT_INFO				=	1110,
	VJ_EVENT_VIDEO_SET_FRAME			=	1111,
	VJ_EVENT_VIDEO_SET_SPEED			=	1112,
	VJ_EVENT_VIDEO_SET_SLOW				=	1113,
	VJ_EVENT_CLIP_SET_LOOPTYPE			=	2001,
	VJ_EVENT_CLIP_SET_DESCRIPTION			=	2025,
	VJ_EVENT_CLIP_SET_SPEED				=	2002,
	VJ_EVENT_CLIP_SET_START				=	2003,
	VJ_EVENT_CLIP_SET_END				=	2004,
	VJ_EVENT_CLIP_SET_DUP				=	2005,
	VJ_EVENT_CLIP_SET_FREEZE			=	2006,
	VJ_EVENT_CLIP_SET_FREEZE_MODE			=	2007,
	VJ_EVENT_CLIP_SET_FREEZE_PFRAMES		=	2008,
	VJ_EVENT_CLIP_SET_FREEZE_NFRAMES		=	2009,
	VJ_EVENT_CLIP_SET_AUDIO_VOL			= 	2010,
	VJ_EVENT_CLIP_SET_MARKER_START			=	2011,
	VJ_EVENT_CLIP_SET_MARKER_END			=	2012,
	VJ_EVENT_CLIP_SET_MARKER			=	2026,
	VJ_EVENT_CLIP_CLEAR_MARKER			=	2013,
	VJ_EVENT_CLIP_CLEAR_FREEZE			=	2014,
	VJ_EVENT_CLIP_HISTORY_SET_ENTRY			=	2015,
	VJ_EVENT_CLIP_HISTORY_ENTRY_AS_NEW		=	2016,
	VJ_EVENT_CLIP_HISTORY_CLEAR_ENTRY		=	2017,
	VJ_EVENT_CLIP_HISTORY_LOCK_ENTRY		=	2018,
	VJ_EVENT_CLIP_HISTORY_UNLOCK_ENTRY		=	2019,
	VJ_EVENT_CLIP_HISTORY_PLAY_ENTRY		=	2020,
	VJ_EVENT_CLIP_LOAD_CLIPLIST			=	2021,
	VJ_EVENT_CLIP_SAVE_CLIPLIST			=	2022,
	VJ_EVENT_CLIP_HISTORY_LIST			=	2023,
	VJ_EVENT_CLIP_LIST				=	2024,
	VJ_EVENT_CLIP_DEL				=	2030,
	VJ_EVENT_CLIP_REC_START				=	2031,
	VJ_EVENT_CLIP_REC_STOP				=	2032,
	VJ_EVENT_CLIP_NEW				=	2033,
	VJ_EVENT_CLIP_SELECT				=	2034,
	VJ_EVENT_CLIP_CHAIN_ENABLE			=	2035,
	VJ_EVENT_CLIP_CHAIN_DISABLE			=	2036,
	VJ_EVENT_CHAIN_TOGGLE_ALL			=	2037,
	VJ_EVENT_CLIP_UPDATE				=	2038,
	VJ_EVENT_CLIP_DEL_ALL				=	2039,
	VJ_EVENT_CLIP_ADD_WAVE				=	2041,
	VJ_EVENT_CLIP_DEL_WAVE				=	2042,
	VJ_EVENT_CLIP_COPY				=	2040,
	VJ_EVENT_TAG_SELECT				=	2100,
	VJ_EVENT_TAG_ACTIVATE				=	2101,
	VJ_EVENT_TAG_DEACTIVATE				=	2102,
	VJ_EVENT_TAG_DELETE				=	2103,
	VJ_EVENT_TAG_NEW_V4L				=	2104,
	VJ_EVENT_TAG_NEW_VLOOP_BY_NAME			=	2105,
	VJ_EVENT_TAG_NEW_VLOOP_BY_ID			=	2106,
	VJ_EVENT_TAG_NEW_Y4M				=	2107,
	VJ_EVENT_TAG_NEW_RAW				=	2108,
	VJ_EVENT_TAG_OFFLINE_REC_START			=	2109,
	VJ_EVENT_TAG_OFFLINE_REC_STOP			=	2110,
	VJ_EVENT_TAG_REC_START				=	2111,
	VJ_EVENT_TAG_REC_STOP				=	2112,
	VJ_EVENT_TAG_LOAD_TAGLIST			=	2113,
	VJ_EVENT_TAG_SAVE_TAGLIST			=	2114,
	VJ_EVENT_TAG_LIST				=	2115,
	VJ_EVENT_TAG_DEVICES				=	2116,
	VJ_EVENT_TAG_CHAIN_DISABLE			=	2117,
	VJ_EVENT_TAG_CHAIN_ENABLE			=	2118,
	VJ_EVENT_TAG_SET_BRIGHTNESS			=	2301,
	VJ_EVENT_TAG_SET_CONTRAST			=	2302,
	VJ_EVENT_TAG_SET_HUE				=	2303,
	VJ_EVENT_TAG_SET_COLOR				=	2304,
	VJ_EVENT_CHAIN_ENTRY_SET_EFFECT			=	2201,
	VJ_EVENT_CHAIN_ENTRY_SET_PRESET			=	2202,
	VJ_EVENT_CHAIN_ENTRY_SET_ARG_VAL		=	2203,
	VJ_EVENT_CHAIN_ENTRY_SET_VIDEO_ON		=	2204,
	VJ_EVENT_CHAIN_ENTRY_SET_VIDEO_OFF		=	2225,
	VJ_EVENT_CHAIN_ENTRY_SET_AUDIO_ON		=	2206,
	VJ_EVENT_CHAIN_ENTRY_SET_AUDIO_OFF		=	2207,
	VJ_EVENT_CHAIN_ENTRY_SET_AUDIO_VOL		=	2208,
	VJ_EVENT_CHAIN_ENTRY_SET_DEFAULTS		=	2209,
	VJ_EVENT_CHAIN_ENTRY_SET_CHANNEL		=	2210,
	VJ_EVENT_CHAIN_ENTRY_SET_SOURCE			=	2211,
	VJ_EVENT_CHAIN_ENTRY_SET_SOURCE_CHANNEL		=	2212,
	VJ_EVENT_CHAIN_ENTRY_CLEAR			=	2213,
	VJ_EVENT_CHAIN_COPY_TO_BUF			=	2221,
	VJ_EVENT_CHAIN_PASTE_AS_NEW			=	2220,
	VJ_EVENT_CHAIN_ENABLE				=	2214,
	VJ_EVENT_CHAIN_DISABLE				=	2219,
	VJ_EVENT_CHAIN_CLEAR				=	2205,
	VJ_EVENT_CHAIN_FADE_IN				=	2216,
	VJ_EVENT_CHAIN_FADE_OUT				=	2217,
	VJ_EVENT_CHAIN_SET_ENTRY			=	2218,
	VJ_EVENT_CHAIN_LIST				=	2215,
	VJ_EVENT_CHAIN_ENTRY_SET_AUTOMATIC		=	2230,
	VJ_EVENT_CHAIN_ENTRY_DEL_AUTOMATIC		=	2231,
	VJ_EVENT_CHAIN_ENTRY_ENABLE_AUTOMATIC		=	2232,	
	VJ_EVENT_CHAIN_ENTRY_DISABLE_AUTOMATIC		=	2233,
	VJ_EVENT_CHAIN_GET_ENTRY			=	2250,
	VJ_EVENT_EFFECT_LIST				=	2500,
	VJ_EVENT_EDITLIST_LIST				=	2501,
	VJ_EVENT_VIDEO_INFORMATION			=	2502,
	VJ_EVENT_OUTPUT_Y4M_START			=	3001,
	VJ_EVENT_OUTPUT_Y4M_STOP			=	3002,
	VJ_EVENT_OUTPUT_RAW_START			=	3003,
	VJ_EVENT_OUTPUT_RAW_STOP			=	3004,
	VJ_EVENT_OUTPUT_VLOOPBACK_START			=	3005,
	VJ_EVENT_OUTPUT_VLOOPBACK_STARTN		=	3006,
	VJ_EVENT_OUTPUT_VLOOPBACK_STOP			=	3007,
	VJ_EVENT_RESIZE_SDL_SCREEN			=	3008,
	VJ_EVENT_SET_PLAY_MODE				=	3009,
	VJ_EVENT_SET_MODE_AND_GO			=	3010,
	VJ_EVENT_AUDIO_ENABLE				=	3011,
	VJ_EVENT_AUDIO_DISABLE				=	3012,
	VJ_EVENT_EDITLIST_PASTE_AT			=	4001,
	VJ_EVENT_EDITLIST_SEL_START			=	4002,
	VJ_EVENT_EDITLIST_SEL_END			=	4003,
	VJ_EVENT_EDITLIST_COPY				=	4004,
	VJ_EVENT_EDITLIST_DEL				=	4005,
	VJ_EVENT_EDITLIST_CROP				=	4006,
	VJ_EVENT_EDITLIST_CUT				=	4007,
	VJ_EVENT_EDITLIST_ADD				=	4008,
	VJ_EVENT_EDITLIST_ADD_CLIP			=	4009,
	VJ_EVENT_EDITLIST_ADD_HIS			=	4010,
	VJ_EVENT_EDITLIST_SAVE				=	4011,
	VJ_EVENT_EDITLIST_LOAD				=	4012,
	/* message bundles are bundles of events, start at 5000 */
	VJ_EVENT_EFFECT_SET_BG				=	4050,
	VJ_EVENT_SET_VOLUME				=	4080, 
	VJ_EVENT_BUNDLE					=	5000,
	VJ_EVENT_BUNDLE_DEL				=	6000,
	VJ_EVENT_BUNDLE_ADD				=	6001,
	VJ_EVENT_BUNDLE_ATTACH_KEY			=	6002,
	VJ_EVENT_BUNDLE_FILE				=	6005,
	VJ_EVENT_BUNDLE_SAVE				=	6006,
	VJ_EVENT_SCREENSHOT				=	6010,
	VJ_EVENT_INIT_GUI_SCREEN			=	6011,
	VJ_EVENT_RECORD_DATAFORMAT			=	6200,
	VJ_EVENT_SHM_OPEN				=	6210,
    VJ_EVENT_SAMPLE_MODE			=	6995,
	VJ_EVENT_BEZERK					=	6996,
	VJ_EVENT_DEBUG_LEVEL				=	6997,
	VJ_EVENT_SUSPEND				=	6998,
	VJ_EVENT_QUIT					=	6999,
};


static struct {		      /* internal message relay for remote host */
	int event_id;
	int internal_msg_id;
} vj_event_remote_list[] = {
	{0,0},
	{ VJ_EVENT_VIDEO_PLAY_FORWARD,			NET_VIDEO_PLAY_FORWARD			},
	{ VJ_EVENT_VIDEO_PLAY_BACKWARD,			NET_VIDEO_PLAY_BACKWARD			},
	{ VJ_EVENT_VIDEO_PLAY_STOP,			NET_VIDEO_PLAY_STOP			},
	{ VJ_EVENT_VIDEO_SKIP_FRAME,			NET_VIDEO_SKIP_FRAME			},
	{ VJ_EVENT_VIDEO_PREV_FRAME,			NET_VIDEO_PREV_FRAME			},
	{ VJ_EVENT_VIDEO_SKIP_SECOND,			NET_VIDEO_SKIP_SECOND			},
	{ VJ_EVENT_VIDEO_PREV_SECOND,			NET_VIDEO_PREV_SECOND			},
	{ VJ_EVENT_VIDEO_GOTO_START,			NET_VIDEO_GOTO_START			},
	{ VJ_EVENT_VIDEO_GOTO_END,			NET_VIDEO_GOTO_END			},
	{ VJ_EVENT_VIDEO_SET_FRAME,			NET_VIDEO_SET_FRAME			},
	{ VJ_EVENT_VIDEO_SET_SPEED,			NET_VIDEO_SET_SPEED			},
	{ VJ_EVENT_VIDEO_SET_SLOW,			NET_VIDEO_SET_SLOW			},
	{ VJ_EVENT_CHAIN_SET_ENTRY,			NET_CHAIN_SET_ENTRY			},
	{ VJ_EVENT_SET_PLAIN_MODE,			NET_SET_PLAIN_MODE			},
	{ VJ_EVENT_SET_CLIP_START,			NET_SET_CLIP_START			},
	{ VJ_EVENT_SET_CLIP_END,			NET_SET_CLIP_END			},
	{ VJ_EVENT_SET_MARKER_START,			NET_SET_MARKER_START			},
	{ VJ_EVENT_SET_MARKER_END,			NET_SET_MARKER_END			},
	{ VJ_EVENT_CLIP_SET_LOOPTYPE,			NET_CLIP_SET_LOOPTYPE			},
	{ VJ_EVENT_CLIP_SET_DESCRIPTION,		NET_CLIP_SET_DESCRIPTION		},
	{ VJ_EVENT_CLIP_SET_SPEED,			NET_CLIP_SET_SPEED			},
	{ VJ_EVENT_CLIP_SET_START,			NET_CLIP_SET_START			},
	{ VJ_EVENT_CLIP_SET_END,			NET_CLIP_SET_END			},
	{ VJ_EVENT_CLIP_SET_DUP,			NET_CLIP_SET_DUP			},
	{ VJ_EVENT_CLIP_SET_FREEZE,			NET_CLIP_SET_FREEZE			},
	{ VJ_EVENT_CLIP_SET_FREEZE_MODE,		NET_CLIP_SET_FREEZE_MODE		},
	{ VJ_EVENT_CLIP_SET_FREEZE_PFRAMES,		NET_CLIP_SET_FREEZE_PFRAMES		},
	{ VJ_EVENT_CLIP_SET_FREEZE_NFRAMES,		NET_CLIP_SET_FREEZE_NFRAMES		},
	{ VJ_EVENT_CLIP_SET_AUDIO_VOL,			NET_CLIP_SET_AUDIO_VOL		},
	{ VJ_EVENT_CLIP_SET_MARKER_START,		NET_CLIP_SET_MARKER_START		},
	{ VJ_EVENT_CLIP_SET_MARKER_END,			NET_CLIP_SET_MARKER_END		},
	{ VJ_EVENT_CLIP_SET_MARKER,			NET_CLIP_SET_MARKER			},
	{ VJ_EVENT_CLIP_CLEAR_MARKER,			NET_CLIP_CLEAR_MARKER			},
	{ VJ_EVENT_CLIP_CLEAR_FREEZE,			NET_CLIP_CLEAR_FREEZE			},
	{ VJ_EVENT_CLIP_HISTORY_SET_ENTRY,		NET_CLIP_HISTORY_SET_ENTRY		},
	{ VJ_EVENT_CLIP_HISTORY_ENTRY_AS_NEW,		NET_CLIP_HISTORY_ENTRY_AS_NEW		},
	{ VJ_EVENT_CLIP_HISTORY_CLEAR_ENTRY,		NET_CLIP_HISTORY_CLEAR_ENTRY		},
	{ VJ_EVENT_CLIP_HISTORY_LOCK_ENTRY,		NET_CLIP_HISTORY_LOCK_ENTRY		},
	{ VJ_EVENT_CLIP_HISTORY_UNLOCK_ENTRY,		NET_CLIP_HISTORY_UNLOCK_ENTRY		},
	{ VJ_EVENT_CLIP_HISTORY_PLAY_ENTRY,		NET_CLIP_HISTORY_PLAY_ENTRY		},
	{ VJ_EVENT_CLIP_LOAD_CLIPLIST,			NET_CLIP_LOAD_CLIPLIST		},
	{ VJ_EVENT_CLIP_SAVE_CLIPLIST,			NET_CLIP_SAVE_CLIPLIST		},
	{ VJ_EVENT_CLIP_HISTORY_LIST,			NET_CLIP_HISTORY_LIST			},
	{ VJ_EVENT_CLIP_LIST,				NET_CLIP_LIST				},
	{ VJ_EVENT_CLIP_DEL,				NET_CLIP_DEL				},
	{ VJ_EVENT_CLIP_DEL_ALL,			NET_CLIP_DEL_ALL			},
	{ VJ_EVENT_CLIP_COPY,				NET_CLIP_COPY				},
	{ VJ_EVENT_CLIP_SELECT,				NET_CLIP_SELECT			},
	{ VJ_EVENT_CLIP_REC_START,			NET_CLIP_REC_START			},
	{ VJ_EVENT_CLIP_REC_STOP,			NET_CLIP_REC_STOP			},
	{ VJ_EVENT_CLIP_NEW,				NET_CLIP_NEW				},
	{ VJ_EVENT_TAG_SELECT,				NET_TAG_SELECT 				},
	{ VJ_EVENT_ENTRY_CHANNEL_UP,			NET_CHAIN_CHANNEL_INC			},
	{ VJ_EVENT_ENTRY_CHANNEL_DOWN,			NET_CHAIN_CHANNEL_DEC			},
	{ VJ_EVENT_TAG_ACTIVATE,			NET_TAG_ACTIVATE			},
	{ VJ_EVENT_TAG_DEACTIVATE,			NET_TAG_DEACTIVATE			},
	{ VJ_EVENT_TAG_DELETE,				NET_TAG_DELETE				},
	{ VJ_EVENT_TAG_NEW_V4L,				NET_TAG_NEW_V4L				},
	{ VJ_EVENT_TAG_NEW_VLOOP_BY_NAME,		NET_TAG_NEW_VLOOP_BY_NAME		},
	{ VJ_EVENT_TAG_NEW_VLOOP_BY_ID,			NET_TAG_NEW_VLOOP_BY_ID			},
	{ VJ_EVENT_TAG_NEW_Y4M,				NET_TAG_NEW_Y4M				},
	{ VJ_EVENT_TAG_NEW_RAW,				NET_TAG_NEW_RAW				},
	{ VJ_EVENT_TAG_OFFLINE_REC_START,		NET_TAG_OFFLINE_REC_START		},
	{ VJ_EVENT_TAG_OFFLINE_REC_STOP,		NET_TAG_OFFLINE_REC_STOP		},
	{ VJ_EVENT_TAG_REC_START,			NET_TAG_REC_START			},
	{ VJ_EVENT_TAG_REC_STOP,			NET_TAG_REC_STOP			},
	{ VJ_EVENT_TAG_LOAD_TAGLIST,			NET_TAG_LOAD_TAGLIST			},
	{ VJ_EVENT_TAG_SAVE_TAGLIST,			NET_TAG_SAVE_TAGLIST			},
	{ VJ_EVENT_TAG_LIST,				NET_TAG_LIST				},
	{ VJ_EVENT_TAG_DEVICES,				NET_TAG_DEVICES				},
	{ VJ_EVENT_CHAIN_ENTRY_SET_EFFECT,		NET_CHAIN_ENTRY_SET_EFFECT		},
	{ VJ_EVENT_CHAIN_ENTRY_SET_PRESET,		NET_CHAIN_ENTRY_SET_PRESET		},
	{ VJ_EVENT_CHAIN_ENTRY_SET_ARG_VAL,		NET_CHAIN_ENTRY_SET_ARG_VAL		},
	{ VJ_EVENT_CHAIN_ENTRY_SET_VIDEO_ON,		NET_CHAIN_ENTRY_SET_VIDEO_ON		},
	{ VJ_EVENT_CHAIN_ENTRY_SET_VIDEO_OFF,		NET_CHAIN_ENTRY_SET_VIDEO_OFF		},
	{ VJ_EVENT_CHAIN_ENTRY_SET_AUDIO_ON,		NET_CHAIN_ENTRY_SET_AUDIO_ON		},	
	{ VJ_EVENT_CHAIN_ENTRY_SET_AUDIO_OFF,		NET_CHAIN_ENTRY_SET_AUDIO_OFF		},
	{ VJ_EVENT_CHAIN_ENTRY_SET_AUDIO_VOL,		NET_CHAIN_ENTRY_SET_AUDIO_VOL		},
	{ VJ_EVENT_CHAIN_ENTRY_SET_DEFAULTS,		NET_CHAIN_ENTRY_SET_DEFAULTS		},
	{ VJ_EVENT_CHAIN_ENTRY_SET_CHANNEL,		NET_CHAIN_ENTRY_SET_CHANNEL		},
	{ VJ_EVENT_CHAIN_ENTRY_SET_SOURCE,		NET_CHAIN_ENTRY_SET_SOURCE		},
	{ VJ_EVENT_CHAIN_ENTRY_SET_SOURCE_CHANNEL,	NET_CHAIN_ENTRY_SET_SOURCE_CHANNEL	},
	{ VJ_EVENT_CHAIN_ENTRY_CLEAR,			NET_CHAIN_ENTRY_CLEAR			},
	{ VJ_EVENT_CHAIN_COPY_TO_BUF,			NET_CHAIN_COPY_TO_BUF			},
	{ VJ_EVENT_CHAIN_PASTE_AS_NEW,			NET_CHAIN_PASTE_AS_NEW			},
	{ VJ_EVENT_CHAIN_ENABLE,			NET_CHAIN_ENABLE			},
	{ VJ_EVENT_CHAIN_DISABLE,			NET_CHAIN_DISABLE			},
	{ VJ_EVENT_CHAIN_CLEAR	,			NET_CHAIN_CLEAR				},
	{ VJ_EVENT_CHAIN_FADE_IN,			NET_CHAIN_FADE_IN			},
	{ VJ_EVENT_CHAIN_FADE_OUT,			NET_CHAIN_FADE_OUT			},
	{ VJ_EVENT_CHAIN_LIST,				NET_CHAIN_LIST				},
	{ VJ_EVENT_EFFECT_LIST,				NET_EFFECT_LIST				},
	{ VJ_EVENT_EDITLIST_LIST,			NET_EDITLIST_LIST			},
	{ VJ_EVENT_VIDEO_INFORMATION,			NET_VIDEO_INFORMATION			},
	{ VJ_EVENT_OUTPUT_Y4M_START,			NET_OUTPUT_Y4M_START			},
	{ VJ_EVENT_OUTPUT_Y4M_STOP,			NET_OUTPUT_Y4M_STOP			},
	{ VJ_EVENT_OUTPUT_RAW_START,			NET_OUTPUT_RAW_START			},
	{ VJ_EVENT_OUTPUT_RAW_STOP,			NET_OUTPUT_RAW_STOP			},
	{ VJ_EVENT_OUTPUT_VLOOPBACK_START,		NET_OUTPUT_VLOOPBACK_START		},
	{ VJ_EVENT_OUTPUT_VLOOPBACK_STOP,		NET_OUTPUT_VLOOPBACK_STOP		},
	{ VJ_EVENT_OUTPUT_VLOOPBACK_STARTN,		NET_OUTPUT_VLOOPBACK_STARTN		},
	{ VJ_EVENT_RESIZE_SDL_SCREEN,			NET_RESIZE_SDL_SCREEN			},
	{ VJ_EVENT_SET_PLAY_MODE,			NET_SET_PLAY_MODE			},
	{ VJ_EVENT_SET_MODE_AND_GO,			NET_SET_MODE_AND_GO			},
	{ VJ_EVENT_AUDIO_ENABLE,			NET_AUDIO_ENABLE			},
	{ VJ_EVENT_AUDIO_DISABLE,			NET_AUDIO_DISABLE			},
	{ VJ_EVENT_EDITLIST_PASTE_AT,			NET_EDITLIST_PASTE_AT			},
	{ VJ_EVENT_EDITLIST_COPY,			NET_EDITLIST_COPY			},
	{ VJ_EVENT_EDITLIST_DEL,			NET_EDITLIST_DEL			},
	{ VJ_EVENT_EDITLIST_CROP,			NET_EDITLIST_CROP			},
	{ VJ_EVENT_EDITLIST_CUT,			NET_EDITLIST_CUT			},
	{ VJ_EVENT_EDITLIST_ADD,			NET_EDITLIST_ADD			},
	{ VJ_EVENT_EDITLIST_ADD_CLIP,			NET_EDITLIST_ADD_CLIP			},
	{ VJ_EVENT_EDITLIST_SAVE,			NET_EDITLIST_SAVE			},
	{ VJ_EVENT_EDITLIST_LOAD,			NET_EDITLIST_LOAD			},
	{ VJ_EVENT_BUNDLE,				NET_BUNDLE				},
	{ VJ_EVENT_BUNDLE_DEL,				NET_DEL_BUNDLE				},
	{ VJ_EVENT_BUNDLE_ADD,				NET_ADD_BUNDLE				},
	{ VJ_EVENT_BUNDLE_FILE,				NET_BUNDLE_FILE				},
	{ VJ_EVENT_BUNDLE_ATTACH_KEY,			NET_BUNDLE_ATTACH_KEY			},
	{ VJ_EVENT_BUNDLE_SAVE,				NET_BUNDLE_SAVE				},
	{ VJ_EVENT_SCREENSHOT,				NET_SCREENSHOT				},
	{ VJ_EVENT_TAG_CHAIN_ENABLE,			NET_TAG_CHAIN_ENABLE			},
	{ VJ_EVENT_TAG_CHAIN_DISABLE,			NET_TAG_CHAIN_DISABLE			},
	{ VJ_EVENT_CLIP_CHAIN_ENABLE,			NET_CLIP_CHAIN_ENABLE			},
	{ VJ_EVENT_CLIP_CHAIN_DISABLE,			NET_CLIP_CHAIN_DISABLE		},
	{ VJ_EVENT_CHAIN_GET_ENTRY,			NET_CHAIN_GET_ENTRY			},
	{ VJ_EVENT_CHAIN_TOGGLE_ALL,			NET_CHAIN_TOGGLE_ALL			},
	{ VJ_EVENT_CLIP_UPDATE,				NET_CLIP_UPDATE			},	
	{ VJ_EVENT_TAG_SET_BRIGHTNESS,			NET_TAG_SET_BRIGHTNESS			},
	{ VJ_EVENT_TAG_SET_CONTRAST,			NET_TAG_SET_CONTRAST			},
	{ VJ_EVENT_TAG_SET_HUE,				NET_TAG_SET_HUE				},
	{ VJ_EVENT_CLIP_ADD_WAVE,			NET_CLIP_ADD_WAVE			},
	{ VJ_EVENT_CLIP_DEL_WAVE,			NET_CLIP_DEL_WAVE			},
	{ VJ_EVENT_RECORD_DATAFORMAT,			NET_RECORD_DATAFORMAT			},
	{ VJ_EVENT_TAG_SET_COLOR,			NET_TAG_SET_COLOR			},
	{ VJ_EVENT_QUIT,  				NET_QUIT				},
	{ VJ_EVENT_INIT_GUI_SCREEN,			NET_INIT_GUI_SCREEN			},
	{ VJ_EVENT_CHAIN_ENTRY_SET_AUTOMATIC,		NET_CHAIN_ENTRY_SET_AUTOMATIC		},
	{ VJ_EVENT_CHAIN_ENTRY_DEL_AUTOMATIC,		NET_CHAIN_ENTRY_DEL_AUTOMATIC		},
	{ VJ_EVENT_CHAIN_ENTRY_ENABLE_AUTOMATIC,	NET_CHAIN_ENTRY_ENABLE_AUTOMATIC	},
	{ VJ_EVENT_CHAIN_ENTRY_DISABLE_AUTOMATIC,	NET_CHAIN_ENTRY_DISABLE_AUTOMATIC	},
	{ VJ_EVENT_EFFECT_SET_BG,			NET_EFFECT_SET_BG			},
	{ VJ_EVENT_SWITCH_CLIP_TAG,			NET_SWITCH_CLIP_TAG	},
	{ VJ_EVENT_SET_VOLUME,				NET_SET_VOLUME				},
	{ VJ_EVENT_SHM_OPEN,				NET_SHM_OPEN				},
	{ VJ_EVENT_SUSPEND			,	NET_SUSPEND				},
	{ VJ_EVENT_DEBUG_LEVEL,				NET_DEBUG_LEVEL				},
	{ VJ_EVENT_BEZERK,				NET_BEZERK				},
	{ VJ_EVENT_SAMPLE_MODE,			NET_SAMPLE_MODE			},
	{0,0}
};

static struct {			/* hardcoded keyboard layout (the default keys) */
	int event_id;
	int sdl_key;
	int alt_mod;
	int ctrl_mod;
	int shift_mod;
} vj_event_sdl_keys[] = {
	{ 0,0,0,0,0 },
	{ VJ_EVENT_EFFECT_SET_BG,		SDLK_b		,1	,0	,0 },
	{ VJ_EVENT_VIDEO_PLAY_FORWARD,		SDLK_KP6	,0	,0	,0 },
	{ VJ_EVENT_VIDEO_PLAY_BACKWARD,		SDLK_KP4 	,0	,0	,0 },
	{ VJ_EVENT_VIDEO_PLAY_STOP,		SDLK_KP5 	,0	,0	,0 },
	{ VJ_EVENT_VIDEO_SKIP_FRAME,		SDLK_KP9 	,0	,0	,0 },
	{ VJ_EVENT_VIDEO_PREV_FRAME,		SDLK_KP7 	,0	,0	,0 },
	{ VJ_EVENT_VIDEO_SKIP_SECOND,		SDLK_KP8 	,0	,0	,0 },
	{ VJ_EVENT_VIDEO_PREV_SECOND,		SDLK_KP2 	,0	,0	,0 },
	{ VJ_EVENT_VIDEO_GOTO_START,		SDLK_KP1 	,0 	,0	,0 },
	{ VJ_EVENT_VIDEO_GOTO_END,		SDLK_KP3 	,0	,0	,0 },
	{ VJ_EVENT_VIDEO_SPEED1,		SDLK_a 		,0	,0	,0 },
	{ VJ_EVENT_VIDEO_SPEED2,		SDLK_s 		,0	,0	,0 },
	{ VJ_EVENT_VIDEO_SPEED3,		SDLK_d 		,0	,0	,0 },
	{ VJ_EVENT_VIDEO_SPEED4,		SDLK_f 		,0	,0	,0 },
	{ VJ_EVENT_VIDEO_SPEED5,		SDLK_g 	 	,0	,0	,0 },
	{ VJ_EVENT_VIDEO_SPEED6,		SDLK_h 		,0	,0	,0 },
	{ VJ_EVENT_VIDEO_SPEED7,		SDLK_j 		,0	,0	,0 },
	{ VJ_EVENT_VIDEO_SPEED8,		SDLK_k 		,0	,0	,0 },
	{ VJ_EVENT_VIDEO_SPEED9,		SDLK_l 		,0	,0	,0 },
	{ VJ_EVENT_VIDEO_SLOW0,			SDLK_a		,1	,0	,0 },
	{ VJ_EVENT_VIDEO_SLOW1,			SDLK_s		,1	,0	,0 },
	{ VJ_EVENT_VIDEO_SLOW2,			SDLK_d		,1	,0	,0 },
	{ VJ_EVENT_VIDEO_SLOW3,			SDLK_f		,1	,0	,0 },
	{ VJ_EVENT_VIDEO_SLOW4,			SDLK_g		,1	,0	,0 },
	{ VJ_EVENT_VIDEO_SLOW5,			SDLK_h		,1	,0	,0 },
	{ VJ_EVENT_VIDEO_SLOW6,			SDLK_j		,1	,0	,0 },
	{ VJ_EVENT_VIDEO_SLOW7,			SDLK_k		,1	,0	,0 },
	{ VJ_EVENT_VIDEO_SLOW8,			SDLK_l		,1	,0	,0 },
	{ VJ_EVENT_ENTRY_UP,			SDLK_KP_PLUS	,0	,0	,0 },
	{ VJ_EVENT_ENTRY_DOWN,			SDLK_KP_MINUS	,0	,0	,0 },
	{ VJ_EVENT_ENTRY_CHANNEL_UP,		SDLK_EQUALS	,0	,0	,0 },
	{ VJ_EVENT_ENTRY_CHANNEL_DOWN,		SDLK_MINUS	,0	,0	,0 },
	{ VJ_EVENT_ENTRY_SOURCE_TOGGLE,		SDLK_SLASH	,0	,0	,0 },
	{ VJ_EVENT_ENTRY_INC_ARG0,		SDLK_PAGEUP	,0	,0	,0 },
	{ VJ_EVENT_ENTRY_DEC_ARG0,		SDLK_PAGEDOWN	,0	,0	,0 },
	{ VJ_EVENT_ENTRY_INC_ARG1,		SDLK_KP_PERIOD  ,0	,0	,0 },
	{ VJ_EVENT_ENTRY_DEC_ARG1,		SDLK_KP0	,0	,0	,0 },
	{ VJ_EVENT_ENTRY_INC_ARG2,		SDLK_PERIOD	,0	,0	,0 },
	{ VJ_EVENT_ENTRY_DEC_ARG2,		SDLK_COMMA	,0	,0	,0 },
	{ VJ_EVENT_ENTRY_INC_ARG3,		SDLK_w		,0	,0	,0 },
	{ VJ_EVENT_ENTRY_DEC_ARG3,		SDLK_q		,0	,0	,0 },
	{ VJ_EVENT_ENTRY_INC_ARG4,		SDLK_r		,0	,0	,0 },
	{ VJ_EVENT_ENTRY_DEC_ARG4,		SDLK_e		,0	,0	,0 },
	{ VJ_EVENT_ENTRY_INC_ARG5,		SDLK_y		,0	,0	,0 },
	{ VJ_EVENT_ENTRY_DEC_ARG5,		SDLK_t		,0	,0	,0 },
	{ VJ_EVENT_ENTRY_INC_ARG6,		SDLK_i		,0	,0	,0 },
	{ VJ_EVENT_ENTRY_DEC_ARG6,		SDLK_u		,0	,0	,0 },
	{ VJ_EVENT_ENTRY_INC_ARG7,		SDLK_p		,0	,0	,0 },
	{ VJ_EVENT_ENTRY_DEC_ARG7,		SDLK_o		,0	,0	,0 },
	{ VJ_EVENT_ENTRY_INC_ARG8,		SDLK_QUOTE	,0	,0 	,0 },
	{ VJ_EVENT_ENTRY_DEC_ARG8,		SDLK_SEMICOLON	,0	,0	,0 },		
	{ VJ_EVENT_SELECT_BANK1,		SDLK_1		,0	,0	,0 },
	{ VJ_EVENT_SELECT_BANK2,		SDLK_2		,0	,0	,0 },
	{ VJ_EVENT_SELECT_BANK3,		SDLK_3		,0	,0	,0 },
	{ VJ_EVENT_SELECT_BANK4,		SDLK_4		,0	,0	,0 },
	{ VJ_EVENT_SELECT_BANK5,		SDLK_5		,0	,0	,0 },
	{ VJ_EVENT_SELECT_BANK6,		SDLK_6		,0	,0	,0 },
	{ VJ_EVENT_SELECT_BANK7,		SDLK_7		,0	,0	,0 },
	{ VJ_EVENT_SELECT_BANK8,		SDLK_8		,0	,0	,0 },
	{ VJ_EVENT_SELECT_BANK9,		SDLK_9		,0	,0	,0 },
	{ VJ_EVENT_SELECT_ID1,			SDLK_F1		,0	,0	,0 },
	{ VJ_EVENT_SELECT_ID2,			SDLK_F2		,0	,0	,0 },
	{ VJ_EVENT_SELECT_ID3,			SDLK_F3		,0	,0	,0 },
	{ VJ_EVENT_SELECT_ID4,			SDLK_F4		,0	,0	,0 },
	{ VJ_EVENT_SELECT_ID5,			SDLK_F5		,0	,0	,0 },
	{ VJ_EVENT_SELECT_ID6,			SDLK_F6		,0	,0	,0 },
	{ VJ_EVENT_SELECT_ID7,			SDLK_F7		,0	,0	,0 },
	{ VJ_EVENT_SELECT_ID8,			SDLK_F8		,0	,0	,0 },
	{ VJ_EVENT_SELECT_ID9,			SDLK_F9		,0	,0	,0 },
	{ VJ_EVENT_SELECT_ID10,			SDLK_F10	,0	,0	,0 },
	{ VJ_EVENT_SELECT_ID11,			SDLK_F11	,0	,0	,0 },
	{ VJ_EVENT_SELECT_ID12,			SDLK_F12	,0	,0	,0 },
	{ VJ_EVENT_SET_PLAIN_MODE,		SDLK_KP_DIVIDE  ,0	,0	,0 },
	{ VJ_EVENT_REC_AUTO_START,		SDLK_p		,1	,0	,0 },
	{ VJ_EVENT_REC_STOP,			SDLK_t		,1	,0	,0 },
	{ VJ_EVENT_REC_START,			SDLK_r		,1	,0	,0 },
	{ VJ_EVENT_CHAIN_TOGGLE,		SDLK_END	,0	,0	,0 },
	{ VJ_EVENT_ENTRY_VIDEO_TOGGLE,		SDLK_END	,1	,0	,0 },
	{ VJ_EVENT_ENTRY_AUDIO_TOGGLE,		SDLK_END	,0	,0	,1 },
	{ VJ_EVENT_ENTRY_DEL,			SDLK_DELETE	,0	,0	,0 },
	{ VJ_EVENT_SELECT_EFFECT_INC,		SDLK_UP		,0	,0	,0 },
	{ VJ_EVENT_SELECT_EFFECT_DEC,		SDLK_DOWN	,0	,0	,0 },
	{ VJ_EVENT_SELECT_EFFECT_ADD,		SDLK_RETURN	,0	,0	,0 },
	{ VJ_EVENT_SET_CLIP_START,		SDLK_LEFTBRACKET,0	,0	,0 },
	{ VJ_EVENT_SET_CLIP_END,		SDLK_RIGHTBRACKET,0	,0	,0 },		
	{ VJ_EVENT_SET_MARKER_START,		SDLK_LEFTBRACKET,1	,0	,0 },
	{ VJ_EVENT_SET_MARKER_END,		SDLK_RIGHTBRACKET,1	,0	,0 },
	{ VJ_EVENT_CLIP_TOGGLE_LOOP,		SDLK_KP_MULTIPLY,0	,0	,0 },
	{ VJ_EVENT_SWITCH_CLIP_TAG,		SDLK_ESCAPE	,0	,0	,0 },	
	{ VJ_EVENT_PRINT_INFO,			SDLK_HOME	,0	,0	,0 },
	{ VJ_EVENT_CLIP_CLEAR_MARKER,		SDLK_BACKSPACE	,0	,0	,0 },
	{ 0,0,0,0,0 },
};

/* the main event list */


typedef struct {
	int event_id;
	char *format;
	int *args;
	int *str;
} vj_event_custom;
	
/*
#define STR_CLIP   "<clip_id>"
#define STR_ENTRY  "<chain_entry>"
#define STR_STREAM "<stream_id>"
#define STR_SOURCE "<source>"
#define STR_NUMERIC "<number>"
#define STR_STR	    "<string>"
#define STR_CHANNEL "<channel>"
#define STR_PLAYMODE "<mode>"

#define STR_CLIP_HELP 	"Clip number (use -1 for last clip , 0 for current playing clip"
#define STR_ENTRY_HELP 	"Chain entry number, (use 0 - 20 to select an entry or -1 for current entry"
#define STR_STREAM_HELP "Stream number (use -1 for last stream, 0 for current playing stream"
#define STR_SOURCE_HELP "Source type, use 0 for clip, 1 for streams"
#define STR_NUMERIC_HELP     "A numeric value"
#define STR_STR_HELP	"A serie of characters, no whitespaces"
#define STR_CHANNEL_HELP  "Depending on the source type, a channel is either a clip- or a stream number"
#define STR_PLAYMODE_HELP "Playback mode, use 0 for clips, 1 for streams, 2 for plain video"
*/


static struct {
	int event_id;
	char *name;
	void (*function)(void *ptr, const char format[], va_list ap);
	int num_params;
	char *format;
	int args[2];
} vj_event_list[] = {
	{ 0,NULL,0,0,NULL, {0,0} },
	{ VJ_EVENT_VIDEO_PLAY_FORWARD,		 "Video: play forward",		  		 vj_event_play_forward,		0 ,	NULL,		{0,0} },
	{ VJ_EVENT_VIDEO_PLAY_BACKWARD,		 "Video: play backward", 	  		 vj_event_play_reverse,		0 ,	NULL,		{0,0} },
	{ VJ_EVENT_VIDEO_PLAY_STOP,		 "Video: play/stop",				 vj_event_play_stop,		0 ,	NULL,		{0,0} },
	{ VJ_EVENT_VIDEO_SKIP_FRAME,		 "Video: skip frame forward",			 vj_event_inc_frame,		0 ,	NULL,		{0,0} },
	{ VJ_EVENT_VIDEO_PREV_FRAME,		 "Video: skip frame backward",			 vj_event_dec_frame,		0 ,	NULL,		{0,0} },
	{ VJ_EVENT_VIDEO_SKIP_SECOND,		 "Video: skip one second forward",		 vj_event_next_second,		0 ,	NULL,		{0,0} },
	{ VJ_EVENT_VIDEO_PREV_SECOND,		 "Video: skip one second backward",		 vj_event_prev_second,		0 ,	NULL,		{0,0} },
	{ VJ_EVENT_VIDEO_GOTO_START,		 "Video: go to starting position",		 vj_event_goto_start,		0 ,	NULL,		{0,0} },
	{ VJ_EVENT_VIDEO_GOTO_END,		 "Video: go to ending position",		 vj_event_goto_end,		0 ,	NULL,		{0,0} },
	{ VJ_EVENT_VIDEO_SPEED1,		 "Video: change speed to 1", 	  		 vj_event_play_speed,		1 , 	"%d",		{1,0} },
	{ VJ_EVENT_VIDEO_SPEED2,		 "Video: change speed to 2",	  		 vj_event_play_speed,		1 ,	"%d",		{2,0} },
	{ VJ_EVENT_VIDEO_SPEED3,		 "Video: change speed to 3",	  		 vj_event_play_speed,		1 ,	"%d",		{3,0} },
	{ VJ_EVENT_VIDEO_SPEED4,		 "Video: change speed to 4",	  		 vj_event_play_speed,		1 ,	"%d", 		{4,0} },
	{ VJ_EVENT_VIDEO_SPEED5,		 "Video: change speed to 5",	  		 vj_event_play_speed,		1 ,	"%d",		{5,0} },
	{ VJ_EVENT_VIDEO_SPEED6,		 "Video: change speed to 6",	  		 vj_event_play_speed,		1 ,	"%d",   	{6,0} },
 	{ VJ_EVENT_VIDEO_SPEED7,		 "Video: change speed to 7",	  		 vj_event_play_speed, 		1 ,	"%d",		{7,0} },
	{ VJ_EVENT_VIDEO_SPEED8,		 "Video: change speed to 8",	  		 vj_event_play_speed, 		1 ,	"%d",		{8,0} },
	{ VJ_EVENT_VIDEO_SPEED9,		 "Video: change speed to 9",	  		 vj_event_play_speed, 		1 ,	"%d",		{9,0} },
	{ VJ_EVENT_VIDEO_SET_SPEED,		 "Video: change speed to X",			 vj_event_play_speed,		1,	"%d",		{0,0} },
	{ VJ_EVENT_VIDEO_SET_SLOW,		 "Video: display frame n times",		 vj_event_play_slow,		1,	"%d",		{0,0}   },
	{ VJ_EVENT_VIDEO_SLOW1,			 "Video: display frame x2",	   	  	 vj_event_play_slow,		1,	"%d",		{1,0}	},
	{ VJ_EVENT_VIDEO_SLOW2,			 "Video: display frame x3",     		 vj_event_play_slow,		1,	"%d",		{2,0}	},
	{ VJ_EVENT_VIDEO_SLOW3,			 "Video: display frame x4",       		 vj_event_play_slow,		1,	"%d",		{3,0}	},
	{ VJ_EVENT_VIDEO_SLOW4,			 "Video: display frame x5",      		 vj_event_play_slow,		1,	"%d",		{4,0}	},
	{ VJ_EVENT_VIDEO_SLOW5,			 "Video: display frame x6",     		 vj_event_play_slow,		1,	"%d",		{5,0}	},
	{ VJ_EVENT_VIDEO_SLOW6,			 "Video: display frame x7",    		   	 vj_event_play_slow,		1,	"%d",		{6,0}	},
 	{ VJ_EVENT_VIDEO_SLOW7,			 "Video: display frame x8",    	 		 vj_event_play_slow,		1,	"%d",		{7,0}	},
	{ VJ_EVENT_VIDEO_SLOW8,			 "Video: display frame x9",      		 vj_event_play_slow,		1,	"%d",		{8,0}	},
	{ VJ_EVENT_VIDEO_SLOW0,			 "Video: display frame x1",      		 vj_event_play_slow,		1,	"%d",		{0,0}	},
	{ VJ_EVENT_VIDEO_SET_FRAME,		 "Video: set frame",				 vj_event_set_frame,		1,	"%d",		{0,0}   },
	{ VJ_EVENT_ENTRY_UP,	 		 "ChainEntry: select next entry",		 vj_event_entry_up,    		0,	NULL,		{0,0}	},
	{ VJ_EVENT_ENTRY_DOWN,			 "ChainEntry: select previous entry",		 vj_event_entry_down,  		0,	NULL,		{0,0}	},
	{ VJ_EVENT_ENTRY_CHANNEL_UP,		 "ChainEntry: select next channel to mix in", vj_event_chain_entry_channel_inc,1,	"%d",		{1,0}	},
	{ VJ_EVENT_ENTRY_CHANNEL_DOWN,		 "ChainEntry: select previous channel to mix in",vj_event_chain_entry_channel_dec,1,"%d",		{1,0}	},
	{ VJ_EVENT_ENTRY_SOURCE_TOGGLE,		 "ChainEntry: toggle mixing source between stream and clip", vj_event_chain_entry_src_toggle,2,	"%d %d",	{0,-1}  },
		//this is silly ...
	{ VJ_EVENT_ENTRY_INC_ARG0,		 "Parameter0: increment current value",		 vj_event_chain_arg_inc,	2,	"%d %d",	{0,1}	},
	{ VJ_EVENT_ENTRY_INC_ARG1,		 "Parameter1: increment current value",		 vj_event_chain_arg_inc,	2,	"%d %d",	{1,1}	},
	{ VJ_EVENT_ENTRY_INC_ARG2,		 "Parameter2: increment current value",		 vj_event_chain_arg_inc,	2,	"%d %d", 	{2,1}	},
	{ VJ_EVENT_ENTRY_INC_ARG3,		 "Parameter3: increment current value",		 vj_event_chain_arg_inc,	2,	"%d %d",	{3,1}	},
	{ VJ_EVENT_ENTRY_INC_ARG4,		 "Parameter4: increment current value",		 vj_event_chain_arg_inc,	2,	"%d %d",	{4,1}	},
	{ VJ_EVENT_ENTRY_INC_ARG5,		 "Parameter5: increment current value",		 vj_event_chain_arg_inc,	2,	"%d %d",	{5,1}	},
	{ VJ_EVENT_ENTRY_INC_ARG6,		 "Parameter6: increment current value",		 vj_event_chain_arg_inc,	2,	"%d %d",	{6,1}	},
	{ VJ_EVENT_ENTRY_INC_ARG7,		 "Parameter7: increment current value",		 vj_event_chain_arg_inc,	2,	"%d %d",	{7,1}	},
	{ VJ_EVENT_ENTRY_INC_ARG8,		 "Parameter8: increment current value",	 	 vj_event_chain_arg_inc,	2,	"%d %d",	{8,1}	},
	{ VJ_EVENT_ENTRY_INC_ARG9,		 "Parameter8: increment current value",		 vj_event_chain_arg_inc,	2,	"%d %d",	{9,1}	},
	{ VJ_EVENT_ENTRY_DEC_ARG0,		 "Parameter0: decrement current value",	 vj_event_chain_arg_inc,	2,	"%d %d",	{0,-1}	},
	{ VJ_EVENT_ENTRY_DEC_ARG1,		 "Parameter1: decrement current value",	 vj_event_chain_arg_inc,  	2,	"%d %d",	{1,-1}	},
	{ VJ_EVENT_ENTRY_DEC_ARG2,		 "Parameter2: decrement current value",	 vj_event_chain_arg_inc,	2,	"%d %d",	{2,-1}	},
	{ VJ_EVENT_ENTRY_DEC_ARG3,		 "Parameter3: decrement current value",	 vj_event_chain_arg_inc,  	2,	"%d %d",	{3,-1}	},
	{ VJ_EVENT_ENTRY_DEC_ARG4,		 "Parameter4: decrement current value",	 vj_event_chain_arg_inc, 	2,	"%d %d",	{4,-1}	},
	{ VJ_EVENT_ENTRY_DEC_ARG5,		 "Parameter5: decrement current value",	 vj_event_chain_arg_inc,  	2,	"%d %d",	{5,-1}	},
	{ VJ_EVENT_ENTRY_DEC_ARG6,		 "Parameter6: decrement current value",	 vj_event_chain_arg_inc,  	2,	"%d %d",	{6,-1}	},
	{ VJ_EVENT_ENTRY_DEC_ARG7,		 "Parameter7: decrement current value",	 vj_event_chain_arg_inc,  	2,	"%d %d",	{7,-1}	},
	{ VJ_EVENT_ENTRY_DEC_ARG8,		 "Parameter8: decrement current value",	 vj_event_chain_arg_inc,  	2,	"%d %d",	{8,-1}	},
	{ VJ_EVENT_ENTRY_DEC_ARG9,		 "Parameter9: decrement p9 on current entry",	 vj_event_chain_arg_inc,  	2,	"%d %d",	{9,-1}	},
	{ VJ_EVENT_ENTRY_VIDEO_TOGGLE,		 "ChainEntry: toggle effect on/off current entry",vj_event_chain_entry_video_toggle,0,	NULL,		{0,0}	},
//	{ VJ_EVENT_ENTRY_AUDIO_TOGGLE,		 "ChainEntry: toggle audio on current entry",vj_event_chain_entry_audio_toggle,0,	NULL,		{0,0}	},
	{ VJ_EVENT_ENTRY_DEL,			 "ChainEntry: delete effect on current entry",		 vj_event_chain_entry_del,	2,	"%d %d",	{0,-1}	},
	{ VJ_EVENT_CHAIN_TOGGLE,		 "Chain: toggle effect chain on/off",			 vj_event_chain_toggle,		0,	NULL,		{0,0}	},
	{ VJ_EVENT_SET_CLIP_START,		 "Clip: set starting frame at current position",	 vj_event_clip_start,		0,	NULL,		{0,0}   },
	{ VJ_EVENT_SET_CLIP_END,		 "Clip: set ending frame at current position and create new clip",vj_event_clip_end,		0,	NULL,		{0,0}   },
	{ VJ_EVENT_SET_MARKER_START,		 "Clip: set marker start at current position",	 vj_event_clip_set_marker_start,2,	"%d %d",	{0,0}	},
	{ VJ_EVENT_SET_MARKER_END,		 "Clip: set marker end at current position",	 vj_event_clip_set_marker_end,2,	"%d %d",	{0,0}	},
	{ VJ_EVENT_SELECT_EFFECT_INC,		 "EffectList: select next effect",		 vj_event_effect_inc,		0,	NULL,		{0,0}	},
	{ VJ_EVENT_SELECT_EFFECT_DEC,	   	 "EffectList: select previous effect",		 vj_event_effect_dec,		0,	NULL,		{0,0}	},
	{ VJ_EVENT_SELECT_EFFECT_ADD,		 "EffectList: add selected effect on current chain enry",	 vj_event_effect_add,		0,	NULL,		{0,0}	},
	{ VJ_EVENT_SELECT_BANK1,		 "Set clip/stream bank 1",		 	 vj_event_select_bank,		1,	"%d",		{1,0}	},
	{ VJ_EVENT_SELECT_BANK2,		 "Set clip/stream bank 2",			 vj_event_select_bank,		1,	"%d",		{2,0}	},
	{ VJ_EVENT_SELECT_BANK3,		 "Set clip/stream bank 3",			 vj_event_select_bank,		1,	"%d",		{3,0}	},
	{ VJ_EVENT_SELECT_BANK4,		 "Set clip/stream bank 4",			 vj_event_select_bank,		1,	"%d",		{4,0}	},
	{ VJ_EVENT_SELECT_BANK5,		 "Set clip/stream bank 5",			 vj_event_select_bank,		1,	"%d",		{5,0}	},
	{ VJ_EVENT_SELECT_BANK6,		 "Set clip/stream bank 6",			 vj_event_select_bank,		1,	"%d",		{6,0}	},
	{ VJ_EVENT_SELECT_BANK7,		 "Set clip/stream bank 7",			 vj_event_select_bank,		1,	"%d",		{7,0}	},
	{ VJ_EVENT_SELECT_BANK8,		 "Set clip/stream bank 8",			 vj_event_select_bank,		1,	"%d",		{8,0}	},
	{ VJ_EVENT_SELECT_BANK9,		 "Set clip/stream bank 9",			 vj_event_select_bank,		1,	"%d",		{9,0}	},
	{ VJ_EVENT_SELECT_ID2,			 "Set clip/stream 2 of current bank",		 vj_event_select_id,		1,	"%d",		{2,0}	},
	{ VJ_EVENT_SELECT_ID1,			 "Set clip/stream 1 of current bank",		 vj_event_select_id,		1,	"%d",		{1,0}	},
	{ VJ_EVENT_SELECT_ID3,			 "Set clip/stream 3 of current bank",		 vj_event_select_id,		1,	"%d",		{3,0}	},
	{ VJ_EVENT_SELECT_ID4,			 "Set clip/stream 4 of current bank",		 vj_event_select_id,		1,	"%d",		{4,0}	},
	{ VJ_EVENT_SELECT_ID5,		 	 "Set clip/stream 5 of current bank",		 vj_event_select_id,		1,	"%d",		{5,0}	},
	{ VJ_EVENT_SELECT_ID6,			 "Set clip/stream 6 of current bank",		 vj_event_select_id,		1,	"%d",		{6,0}	},
	{ VJ_EVENT_SELECT_ID7,		  	 "Set clip/stream 7 of current bank",		 vj_event_select_id,		1,	"%d",		{7,0}	},
	{ VJ_EVENT_SELECT_ID8,			 "Set clip/stream 8 of current bank",		 vj_event_select_id,		1,	"%d",		{8,0}	},
	{ VJ_EVENT_SELECT_ID9,			 "Set clip/stream 9 of current bank",		 vj_event_select_id,		1,	"%d",		{9,0}	},
	{ VJ_EVENT_SELECT_ID10,			 "Set clip/stream 10 of current bank",		 vj_event_select_id,		1,	"%d",		{10,0}	},
	{ VJ_EVENT_SELECT_ID11,			 "Set clip/stream 11 of current bank",		 vj_event_select_id,		1,	"%d",		{11,0}	},
	{ VJ_EVENT_SELECT_ID12,			 "Set clip/stream 12 of current bank",		 vj_event_select_id,		1,	"%d",		{12,0}	},
	{ VJ_EVENT_CLIP_TOGGLE_LOOP,		 "Toggle looptype to normal or pingpong",			 vj_event_clip_set_loop_type, 2,	"%d %d",	{0,-1}   },
	{ VJ_EVENT_CLIP_ADD_WAVE,		 "Set a WAV file for the clip recorder",	vj_event_clip_add_wave,		1,	"%s",		{0,0}	},
	{ VJ_EVENT_CLIP_DEL_WAVE,		 "Unset a WAV file for the clip recorder",	vj_event_clip_del_wave,		0,	NULL,		{0,0}	},
	{ VJ_EVENT_RECORD_DATAFORMAT,		 "Set dataformat for stream/clip record",	vj_event_tag_set_format,	1,	"%s",		{0,0}	 },
	{ VJ_EVENT_REC_AUTO_START,		 "Record clip/stream and auto play after recording",		 vj_event_misc_start_rec_auto,	0,	NULL,		{0,0}	},
	{ VJ_EVENT_REC_START,			 "Record clip/stream start",		 vj_event_misc_start_rec,	0,	NULL,		{0,0}	},
	{ VJ_EVENT_REC_STOP,			 "Record clip/stream stop",		 vj_event_misc_stop_rec,	0,	NULL,		{0,0}	},

	{ VJ_EVENT_CLIP_NEW,			 "Clip: create new",			 vj_event_clip_new,		2,	"%d %d",	{0,0}	},

	{ VJ_EVENT_PRINT_INFO,			 "Info: output clip/stream details",	 vj_event_print_info,		1,	"%d",		{0,0}	},
	{ VJ_EVENT_SET_PLAIN_MODE,		 "Video: set plain video mode",		 vj_event_set_play_mode,	1,	"%d",		{2,0}	},
	/* end of keyboard compatible events */
	{ VJ_EVENT_CLIP_SET_LOOPTYPE,		 "Clip: set looptype",			 vj_event_clip_set_loop_type, 2,	"%d %d",	{0,0}   }, 
	{ VJ_EVENT_CLIP_SET_SPEED,		 "Clip: set speed",			 vj_event_clip_set_speed,	2,	"%d %d",	{0,0}   },
	{ VJ_EVENT_CLIP_SET_DESCRIPTION,	"Clip: set description",		 vj_event_clip_set_descr,	2,	"%d %s",	{0,0}   },
	{ VJ_EVENT_CLIP_SET_END,		"Clip: set ending position",		 vj_event_clip_set_end, 	2,	"%d %d",	{0,0}   },
	{ VJ_EVENT_CLIP_SET_START,		"Clip: set starting position",		 vj_event_clip_set_start,	2,	"%d %d",	{0,0}   },
	{ VJ_EVENT_CLIP_SET_DUP,		"Clip: set frame duplication",		 vj_event_clip_set_dup,	2,	"%d %d",	{0,0}   },
	{ VJ_EVENT_CLIP_SET_FREEZE,		"Clip: set playback mode (freeze/unfreeze)" ,vj_event_clip_set_freeze_play,4,	"%d %d %d %d",	{0,0}   },
	{ VJ_EVENT_CLIP_SET_MARKER_START,	"Clip: set marker starting position",	 vj_event_clip_set_marker_start,2,	"%d %d",	{0,0}	},
	{ VJ_EVENT_CLIP_SET_MARKER_END,		"Clip: set marker ending position",	 vj_event_clip_set_marker_end,  2,	"%d %d",	{0,0}	},
	{ VJ_EVENT_CLIP_SET_MARKER,		"Clip: set marker starting and ending position", vj_event_clip_set_marker,	  3,	"%d %d %d",	{0,0}	},
	{ VJ_EVENT_CLIP_CLEAR_MARKER,		"Clip: clear marker",			 vj_event_clip_set_marker_clear,1,	"%d",		{0,0}	},
	{ VJ_EVENT_CLIP_HISTORY_ENTRY_AS_NEW,	"ClipHistory: move entry to new clip",	 vj_event_clip_his_entry_to_new,2,	"%d %d",	{0,0}	}, 
	{ VJ_EVENT_CLIP_HISTORY_CLEAR_ENTRY,	"ClipHistory: clear entry",		 vj_event_clip_his_del_entry,   2,	"%d %d",	{0,0}	},
	{ VJ_EVENT_CLIP_HISTORY_LOCK_ENTRY,   	"ClipHistory: lock entry",		 vj_event_clip_his_lock_entry,  2,	"%d %d",	{0,0}	},
	{ VJ_EVENT_CLIP_HISTORY_UNLOCK_ENTRY, 	"ClipHistory: unlock entry",		 vj_event_clip_his_unlock_entry,2,	"%d %d",	{0,0}	},
	{ VJ_EVENT_CLIP_HISTORY_PLAY_ENTRY,	"ClipHistory: play entry",		 vj_event_clip_his_play_entry,  2,	"%d %d",	{0,0}	},
	{ VJ_EVENT_CLIP_LOAD_CLIPLIST,		"Clip: load clips from file",			 vj_event_clip_load_list,	  1,	"%s",		{0,0}	},
	{ VJ_EVENT_CLIP_SAVE_CLIPLIST,		"Clip: save clips to file",			 vj_event_clip_save_list,	  1,	"%s",		{0,0}	},
	{ VJ_EVENT_CLIP_CHAIN_ENABLE,		"Clip: enable effect chain",		 vj_event_clip_chain_enable,	  1,	"%d",		{0,0}	},
	{ VJ_EVENT_CLIP_CHAIN_DISABLE,		"Clip: disable effect chain",			 vj_event_clip_chain_disable,	  1,	"%d",		{0,0}	},
	{ VJ_EVENT_CLIP_REC_START,		"Clip: record this clip to new",	 vj_event_clip_rec_start,	  2,	"%d %d",	{0,0}   },
	{ VJ_EVENT_CLIP_REC_STOP,		"Clip: stop recording this clip",	 vj_event_clip_rec_stop,	  0,	NULL,
	  {0,0}   },
	{ VJ_EVENT_CLIP_DEL,			"Clip: delete",		 vj_event_clip_del,		  1,	"%d",		{0,0}	},
	{ VJ_EVENT_CLIP_DEL_ALL,		"Clip: delete all",	vj_event_clip_clear_all,		  0,    NULL,		{0,0}	},
	{ VJ_EVENT_CLIP_COPY,			"Clip: copy clip <num>", vj_event_clip_copy,		  1,    "%d",		{0,0}   },
	{ VJ_EVENT_CLIP_SELECT,			"Clip: select and play a clip",		 vj_event_clip_select,	 1,	"%d",		{0,0}	},
	{ VJ_EVENT_TAG_SELECT,			"Stream: select and play a stream",		 vj_event_tag_select,		 1,	"%d",		{0,0}	},
	{ VJ_EVENT_TAG_DELETE,			"Stream: delete",				 vj_event_tag_del,		 1,	"%d",		{0,0}	},		
	{ VJ_EVENT_TAG_NEW_V4L,			"Stream: open video4linux device (hw)",	 vj_event_tag_new_v4l,		 1,	"%s",		{0,0}	},	
	{ VJ_EVENT_TAG_NEW_VLOOP_BY_NAME,	"Stream: open vloopback by name (sw)",		 vj_event_tag_new_vloopback_name,2,	"%s %d",	{0,0}	},
	{ VJ_EVENT_TAG_NEW_VLOOP_BY_ID,		"Stream: open vloopback by id (sw)",		 vj_event_tag_new_vloopback_id,	 2,	"%d %d",	{0,0}	},
	{ VJ_EVENT_TAG_NEW_Y4M,			"Stream: open y4m stream by name (file)",	 vj_event_tag_new_y4m,		 1,	"%s",		{0,0}	}, 
	{ VJ_EVENT_TAG_NEW_RAW,			"Stream: open raw stream by name (file)",	 vj_event_tag_new_raw,		 1, 	"%s",		{0,0}	},
	{ VJ_EVENT_TAG_OFFLINE_REC_START,	"Stream: start record from an invisible stream", vj_event_tag_rec_offline_start, 3,	"%d %d %d",	{0,0}	}, 
	{ VJ_EVENT_TAG_OFFLINE_REC_STOP,	"Stream: stop record from an invisible stream",	 vj_event_tag_rec_offline_stop,  0,	NULL,		{0,0}	},
	{ VJ_EVENT_TAG_REC_START,		"Stream: start recording from stream",	 vj_event_tag_rec_start,	 2,	"%d %d",	{0,0}   },
	{ VJ_EVENT_TAG_REC_STOP,		"Stream: stop recording from stream",	 vj_event_tag_rec_stop,		 0,	NULL,		{0,0}	},
	{ VJ_EVENT_TAG_CHAIN_ENABLE,		"Stream: enable effect chain",			 vj_event_tag_chain_enable,	1,	"%d",		{0,0}	},
	{ VJ_EVENT_TAG_CHAIN_DISABLE,		"Stream: disable effect chain",		 vj_event_tag_chain_disable,	1,	"%d",		{0,0}	},
	{ VJ_EVENT_CHAIN_ENTRY_SET_EFFECT,	"ChainEntry: set effect with defaults",		 vj_event_chain_entry_set,	3,	"%d %d %d" ,	{0,-1}	},
	{ VJ_EVENT_CHAIN_ENTRY_SET_PRESET,	"ChainEntry: preset effect",			 vj_event_chain_entry_preset,   13,	"%d %d %d %d %d %d %d %d %d %d %d %d %d",{0,0} },
	{ VJ_EVENT_CHAIN_ENTRY_SET_ARG_VAL,	"ChainEntry: set parameter x value y",		 vj_event_chain_entry_set_arg_val,4,	"%d %d %d %d",	{0,-1}	},
	{ VJ_EVENT_CHAIN_ENTRY_SET_VIDEO_ON,	"ChainEntry: set video on entry on",		 vj_event_chain_entry_enable_video,2,	"%d %d",	{0,-1}	},
	{ VJ_EVENT_CHAIN_ENTRY_SET_VIDEO_OFF,	"ChainEntry: set video on entry off",		 vj_event_chain_entry_disable_video,2,	"%d %d",	{0,-1}	},
	{ VJ_EVENT_CHAIN_ENTRY_SET_AUDIO_ON,	"ChainEntry: set audio on entry on",		 vj_event_chain_entry_enable_audio,2,	"%d %d",	{0,-1}	},
	{ VJ_EVENT_CHAIN_ENTRY_SET_AUDIO_OFF,	"ChainEntry: set audio on entry off",		 vj_event_chain_entry_disable_audio,2,	"%d %d",	{0,-1}	},
	{ VJ_EVENT_CHAIN_ENTRY_SET_AUDIO_VOL,	"ChainEntry: set audio volume on entry to",	 vj_event_chain_entry_audio_volume,3,	"%d %d %d",	{0,-1}	},
	{ VJ_EVENT_CHAIN_ENTRY_SET_DEFAULTS,	"ChainEntry: set effect on entry to defaults",	 vj_event_chain_entry_set_defaults,2,	"%d %d",	{0,-1}	},
	{ VJ_EVENT_CHAIN_ENTRY_SET_CHANNEL,	"ChainEntry: set mixing channel on entry to",	 vj_event_chain_entry_channel,3,	"%d %d %d",	{0,-1}	},
	{ VJ_EVENT_CHAIN_ENTRY_SET_SOURCE,	"ChainEntry: set mixing source on entry to",	 vj_event_chain_entry_source,3,		"%d %d %d",	{0,-1}	},
	{ VJ_EVENT_CHAIN_ENTRY_SET_SOURCE_CHANNEL,"ChainEntry: set mixing source and channel on",vj_event_chain_entry_srccha,	 4,	"%d %d %d %d",	{0,-1}	},
	{ VJ_EVENT_CHAIN_ENTRY_CLEAR,		"ChainEntry: clear entry",			 vj_event_chain_entry_del,	 2,	"%d %d",	{0,-1}	},
	{ VJ_EVENT_CHAIN_ENTRY_SET_AUTOMATIC,	"ChainEntry: automate a parameter",		 vj_event_chain_entry_efk_set,	5,	"%d %d %d %d %d",{0,0}  },
  	{ VJ_EVENT_CHAIN_ENTRY_DEL_AUTOMATIC,	"ChainEntry: clear automated parameter",	 vj_event_chain_entry_efk_del,	3,	"%d %d %d",	{0,0}	},
	{ VJ_EVENT_CHAIN_ENTRY_ENABLE_AUTOMATIC,"ChainEntry: enable automated paramter",	 vj_event_chain_entry_efk_enable,3,	"%d %d %d",	{0,0}	},
	{ VJ_EVENT_CHAIN_ENTRY_DISABLE_AUTOMATIC,"ChainEntry: disable automated paramter",	 vj_event_chain_entry_efk_disable,3,	"%d %d %d",	{0,0}	},
	{ VJ_EVENT_CHAIN_ENABLE,		"Chain: enable chain",				 vj_event_chain_enable,		1,	"%d",		{0,0}	},
	{ VJ_EVENT_CHAIN_DISABLE,		"Chain: disable chain",				 vj_event_chain_disable,	1,	"%d",		{0,0}	},
	{ VJ_EVENT_CHAIN_CLEAR,			"Chain: clear chain",				 vj_event_chain_clear,		1,	"%d",		{0,0}	},
	{ VJ_EVENT_CHAIN_FADE_IN,		"Chain: fade in",				 vj_event_chain_fade_in,	2,	"%d %d",	{0,0}	}, 
	{ VJ_EVENT_CHAIN_FADE_OUT,		"Chain: fade out",				 vj_event_chain_fade_out,	2,	"%d %d",	{0,0}	},
	{ VJ_EVENT_CHAIN_SET_ENTRY,		"Chain: select active entry n1",		 vj_event_chain_entry_select,	1,	"%d",		{0,0}	},
	{ VJ_EVENT_OUTPUT_Y4M_START,		"Output: Yuv4Mpeg start writing to file",	 vj_event_output_y4m_start,	1,	"%s",		{0,0}	},
	{ VJ_EVENT_OUTPUT_Y4M_STOP,		"Output: Yuv4Mpeg stop writing to file",	 vj_event_output_y4m_stop,	0,	NULL,		{0,0}	},
	{ VJ_EVENT_OUTPUT_VLOOPBACK_START,	"Output: vloopback start writing to loop device",vj_event_output_vloopback_start,2,	"%d %d",		{0,0}	},
	{ VJ_EVENT_OUTPUT_VLOOPBACK_STARTN,	"Output: vloopback device by name",		 vj_event_output_vloopback_startn,2,	"%s %d",		{0,0}	},
	{ VJ_EVENT_OUTPUT_VLOOPBACK_STOP,	"Output: vloopback stop writing to loop device", vj_event_output_vloopback_stop, 0,	NULL,		{0,0}	},
	{ VJ_EVENT_OUTPUT_RAW_START,		"Output: RAW start writing to file",		 vj_event_output_raw_start,	1,	"%s",		{0,0}	},
	{ VJ_EVENT_OUTPUT_RAW_STOP,		"Output: RAW stop writing to file",		 vj_event_output_raw_stop,	1,	"%s",		{0,0}	},
	{ VJ_EVENT_RESIZE_SDL_SCREEN,		"Output: Resize SDL video screen",		 vj_event_set_screen_size,	2,	"%d %d", 	{0,0}	},
	{ VJ_EVENT_INIT_GUI_SCREEN,		"Output: Initialize GUI screen",		vj_event_init_gui_screen,	2,	"%s %d",	{0,0}	},
	{ VJ_EVENT_SET_PLAY_MODE,		"Playback: switch playmode clip/tag/plain", 	 vj_event_set_play_mode,	1,	"%d",		{2,0}	},
	{ VJ_EVENT_SET_MODE_AND_GO,		"Playback: set playmode (and fire clip/tag)",  vj_event_set_play_mode_go,	2,	"%d %d",	{0,0}	},
	{ VJ_EVENT_SWITCH_CLIP_TAG,		"Playback: switch between clips/tags",	 vj_event_switch_clip_tag,	0,	NULL,		{0,0}	},
	{ VJ_EVENT_AUDIO_DISABLE,		"Playback: disable audio",			 vj_event_disable_audio,	0,	NULL,		{0,0}   },
	{ VJ_EVENT_AUDIO_ENABLE,		"Playback: enable audio",			 vj_event_enable_audio,		0,	NULL,		{0,0}	},
	{ VJ_EVENT_EDITLIST_PASTE_AT,		"EditList: Paste frames from buffer at frame",	 vj_event_el_paste_at,		1,	"%d",		{0,0}	},
	{ VJ_EVENT_EDITLIST_CUT,		"EditList: Cut frames n1-n2 to buffer",		 vj_event_el_cut,		2,	"%d %d",	{0,0}   },
	{ VJ_EVENT_EDITLIST_COPY,		"EditList: Copy frames n1-n2 to buffer",	 vj_event_el_copy,		2,	"%d %d",	{0,0}	},
	{ VJ_EVENT_EDITLIST_CROP,		"EditList: Crop frames n1-n2",			 vj_event_el_crop,		2,	"%d %d",	{0,0}   },
	{ VJ_EVENT_EDITLIST_DEL,		"EditList: Del frames n1-n2",			 vj_event_el_del,		2,	"%d %d",	{0,0}	}, 
	{ VJ_EVENT_EDITLIST_SAVE,		"EditList: save EditList to new file",		 vj_event_el_save_editlist,	1,	"%s",		{0,0} 	},
	{ VJ_EVENT_EDITLIST_LOAD,		"EditList: load EditList into veejay",		 vj_event_el_load_editlist,	1,	"%s",		{0,0}	},
	{ VJ_EVENT_EDITLIST_ADD,		"EditList: add video file to editlist",		 vj_event_el_add_video,		1,	"%s",		{0,0}	},
	{ VJ_EVENT_EDITLIST_ADD_CLIP,		"EditList: add video file to editlist as clip", vj_event_el_add_video_clip,	1,	"%s",		{0,0}	},
	{ VJ_EVENT_EDITLIST_ADD_HIS,		"EditList: add video file to history list",	vj_event_el_add_video_his,	4,	"%s %d %d %d",	{0,0}	},
	{ VJ_EVENT_TAG_LIST,			"Stream: send list of all streams",		vj_event_send_tag_list,		1,	"%d",		{0,0}	},
	{ VJ_EVENT_CLIP_LIST,			"Clip: send list of Clips",			vj_event_send_clip_list,	1,	"%d",		{0,0}	},
	{ VJ_EVENT_EDITLIST_LIST,		"EditList: send list of all files",		vj_event_send_editlist,		0,	NULL,		{0,0}   },
	{ VJ_EVENT_TAG_DEVICES,			"Stream: get list of available devices",	vj_event_send_devices,		1,	"%s",		{0,0}	},
	{ VJ_EVENT_BUNDLE,			"Bundle: execute collection of messages",	vj_event_do_bundled_msg,	1,	"%d",		{0,0}	},
	{ VJ_EVENT_BUNDLE_FILE,			"Bundle: load action file",			vj_event_read_file,		1,	"%s",		{0,0}	},
	{ VJ_EVENT_BUNDLE_DEL,			"Bundle: delete a bundle of messages",		vj_event_bundled_msg_del,	1,	"%d",		{0,0}	},
	{ VJ_EVENT_BUNDLE_SAVE,			"Bundle: save action file",			vj_event_write_actionfile,	1,	"%s",		{0,0}   },
	{ VJ_EVENT_BUNDLE_ADD,			"Bundle: add a new bundle to event system",	vj_event_bundled_msg_add,	2,	"%s %d",	{0,0}	},
	{ VJ_EVENT_CHAIN_LIST,			"Chain: get all contents",			vj_event_send_chain_list,	1,	"%d",		{0,0}	},
	{ VJ_EVENT_CHAIN_GET_ENTRY,		"Chain: get entry contents",			vj_event_send_chain_entry,	2,	"%d %d",	{0,0}	},		
	{ VJ_EVENT_EFFECT_LIST,			"EffectList: list all effects",			vj_event_send_effect_list,	0,	NULL,		{0,0}	},
	{ VJ_EVENT_VIDEO_INFORMATION,		"Video: send properties",			vj_event_send_video_information,0,	NULL,		{0,0}	},
	{ VJ_EVENT_CLIP_HISTORY_LIST,		"Clip: send history list",			vj_event_send_clip_history_list,1,	"%d",		{0,0}	},
	{ VJ_EVENT_BUNDLE_ATTACH_KEY,		"Bundle: attach a Key to a bundle",		vj_event_attach_key_to_bundle,	3,	"%d %d %d",	{0,0} 	},
	{ VJ_EVENT_SCREENSHOT,			"Various: Save frame to jpeg",			vj_event_screenshot,	0,		NULL,		{0,0}   },
	{ VJ_EVENT_CHAIN_TOGGLE_ALL,		"Toggle Effect Chain on all clips or streams",	vj_event_all_clips_chain_toggle,1,   "%d",		{0,0}   },
	{ VJ_EVENT_CLIP_UPDATE,		"Clip: Update starting and ending position by offset",	vj_event_clip_rel_start,3,"%d %d %d",		{0,0}	},	 
	{ VJ_EVENT_TAG_SET_BRIGHTNESS,		"Video4Linux: set v4l brightness value",		vj_event_v4l_set_brightness,	2,	"%d %d",	{0,0} },
	{ VJ_EVENT_TAG_SET_CONTRAST,		"Video4Linux: set v4l contrast value",			vj_event_v4l_set_contrast,	2,	"%d %d",	{0,0} },
	{ VJ_EVENT_TAG_SET_HUE,			"Video4Linux: set v4l hue value",			vj_event_v4l_set_hue,		2,	"%d %d",	{0,0} },
	{ VJ_EVENT_TAG_SET_COLOR,		"Video4Linux: set v4l color value",			vj_event_v4l_set_color,		2,	"%d %d",	{0,0} },
	{ VJ_EVENT_EFFECT_SET_BG,		"Effect: set background (if supported)",		vj_event_effect_set_bg,		0,	NULL,		{0,0} },
	{ VJ_EVENT_SHM_OPEN,			"Shared memory",					vj_event_tag_new_shm,		2,	"%d %d",	{0,0} },
	{ VJ_EVENT_QUIT,			"Quit veejay",					vj_event_quit,			0, 	NULL, 		{0,0} },
	{ VJ_EVENT_SET_VOLUME,			"Veejay set audio volume",			vj_event_set_volume,		1,	"%d",	{0,0} },
	{ VJ_EVENT_SUSPEND,			"Suspend veejay",				vj_event_suspend,		0,
  NULL,		  {0,0} },     
	{ VJ_EVENT_DEBUG_LEVEL,			"Toggle more/less debugging information",	vj_event_debug_level,		0,	 NULL,		{0,0}},
	{ VJ_EVENT_BEZERK,			"Toggle bezerk mode",				vj_event_bezerk,		0,	NULL,		{0,0}},
	{ VJ_EVENT_SAMPLE_MODE,		"Toggle between box or triangle filter for 4:2:0 -> 4:4:4 sampling", vj_event_sample_mode, 0, NULL, {0,0}},
	{ 0,NULL,0,0,NULL,{0,0}},
};
	
	
static inline hash_val_t int_macro_hash(const void *key)
{
	return (hash_val_t) key;
}

static inline int int_macro_compare(const void *key1,const void *key2)
{
	return ((int)key1 < (int) key2 ? -1 : 
		((int) key1 > (int) key2 ? +1 : 0));
}


static inline hash_val_t int_bundle_hash(const void *key)
{
	return (hash_val_t) key;
}

static inline int int_bundle_compare(const void *key1,const void *key2)
{
	return ((int)key1 < (int) key2 ? -1 : 
		((int) key1 > (int) key2 ? +1 : 0));
}

typedef struct {
	int event_id;
	int accelerator;
	char *bundle;
} vj_msg_bundle;

/* forward declarations (former console clip/tag print info) */
void vj_event_print_clip_info(veejay_t *v, int id); 

void vj_event_print_tag_info(veejay_t *v, int id); 


/* some macros for commonly used checks */

#define CLIP_PLAYING(v) ( (v->uc->playback_mode == VJ_PLAYBACK_MODE_CLIP) )
#define TAG_PLAYING(v) ( (v->uc->playback_mode == VJ_PLAYBACK_MODE_TAG) )
#define PLAIN_PLAYING(v) ( (v->uc->playback_mode == VJ_PLAYBACK_MODE_PLAIN) )

#define p_no_clip(a) {  veejay_msg(VEEJAY_MSG_ERROR, "Clip %d does not exist",a); }
#define p_no_tag(a)    {  veejay_msg(VEEJAY_MSG_ERROR, "Stream %d does not exist",a); }
#define p_invalid_mode() {  veejay_msg(VEEJAY_MSG_DEBUG, "Invalid playback mode for this action"); }


/*
#define CURRENT_CLIP(v,s) ( (v->uc->playback_mode == VJ_PLAYBACK_MODE_CLIPS) && clip_exists(s))
#define CURRENT_TAG(v,t) ( (v->uc->playback_mode == VJ_PLAYBACK_MODE_TAG) && vj_tag_exists(t))
*/

int vj_event_bundle_update( vj_msg_bundle *bundle, int bundle_id )
{
	if(bundle) {
		hnode_t *n = hnode_create(bundle);
		hnode_put( n, (void*) bundle_id);
		hnode_destroy(n);
		return 1;
	}
	return 0;
}

static char *vj_macro_get(int frame_num)
{
	hnode_t *n = hash_lookup(MacroHash, (void*) frame_num);
	if(n)
	{
		char *msg = hnode_get(n);
		if(msg) return msg;
	}
	return NULL;
}

static int	vj_macro_store(char *msg, int frame_num)
{
	hnode_t *n;
	if(!msg) return -1;
	n = hnode_create( msg );
	if(!n) return -1;
	if ( !hash_lookup(MacroHash, (void*) frame_num ))
	{
		//todo: format message to bundle automatically
		hash_insert( MacroHash, n, (void*) frame_num );
	}
	else
	{
		hnode_put ( n, (void*) frame_num );
	}
	return 0; 
}


vj_msg_bundle *vj_event_bundle_get(int event_id)
{
	vj_msg_bundle *m;
	hnode_t *n = hash_lookup(BundleHash, (void*) event_id);
	if(n) 
	{
		m = (vj_msg_bundle*) hnode_get(n);
		if(m) return m;
	}
	return NULL;
}

int vj_event_bundle_exists(int event_id)
{
	return ( vj_event_bundle_get(event_id) == NULL ? 0 : 1);
}

int vj_event_suggest_bundle_id(void)
{
	int i;
	for(i=5000 ; i < 6000; i++)
	{
		if ( vj_event_bundle_exists(i ) == 0 ) return i;
	}
	return -1;
}

int vj_event_bundle_store( vj_msg_bundle *m )
{
	hnode_t *n;
	if(!m) return -1;
	n = hnode_create(m);
	if(!n) return -1;
	if(!vj_event_bundle_exists(m->event_id))
	{
		hash_insert( BundleHash, n, (void*) m->event_id);
	}
	else
	{
		hnode_put( n, (void*) m->event_id);
	}
	return 0;
}
int vj_event_bundle_del( int event_id )
{
	hnode_t *n;
	vj_msg_bundle *m = vj_event_bundle_get( event_id );
	if(!m) return -1;
	n = hash_lookup( BundleHash , (void*) event_id);
	if(n)
	{
		hash_delete( BundleHash, n );
		return 0;
	}
	return -1;
}

vj_msg_bundle *vj_event_bundle_new(char *bundle_msg, int event_id)
{
	vj_msg_bundle *m;
	int len = 0;
	if(!bundle_msg || strlen(bundle_msg) < 1)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Not storing empty bundles");
		return NULL;
	}	
	len = strlen(bundle_msg);
	if(len>1)
	{
		m = (vj_msg_bundle*) malloc(sizeof(vj_msg_bundle));
		if(!m) 
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Error allocating memory for bundled message");
			return NULL;
		}
		m->bundle = (char*) malloc(sizeof(char) * len+1);
		bzero(m->bundle, len+1);
		if(!m)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Error allocating memory for bundled message context");
			return NULL;
		}
		strncpy(m->bundle, bundle_msg, len);
	
		m->event_id = event_id;
		veejay_msg(VEEJAY_MSG_DEBUG, "VIMS bundle:[%d] [%s]", event_id,bundle_msg); 
		return m;
	}
	else
	{
			veejay_msg(VEEJAY_MSG_ERROR,"Cowardly refusing to store empty bundles!");
	}
	
	return NULL;	
}



void vj_event_trigger_function(void *ptr, vj_event f, int max_args, const char format[], ...) 
{
	va_list ap;
	va_start(ap,format);
	f(ptr, format, ap);	
	va_end(ap);
}

char *vj_event_err(int error_type) {
	int i;
	int n=0;
	for(i=1; vj_event_errors[i].str ; i++) 
	{
		if(vj_event_errors[i].error_id==VJ_ERROR_NONE) n=i;
	
		if(error_type==vj_event_errors[i].error_id) 
		{
			return vj_event_errors[i].str;
		}
	}
	return vj_event_errors[n].str;
}

vj_event vj_event_function_by_id(int id) 
{
	vj_event function;
	int i;
	for(i=1; vj_event_list[i].name; i++)
	{
		if( id == vj_event_list[i].event_id ) 	
		{
			function = vj_event_list[i].function;
			veejay_msg(VEEJAY_MSG_DEBUG, "Requested event [%s] with format [%s] , num parameters %d",
				vj_event_list[i].name,
				vj_event_list[i].format,
				vj_event_list[i].num_params
			);
			return function;
		}
	}
	veejay_msg(VEEJAY_MSG_DEBUG, "%s" , vj_event_err(VJ_ERROR_EVENT));
	function = vj_event_none;
	return function;
}

int vj_event_get_id(int event_id)
{
	int i;
	for(i=1; vj_event_list[i].name; i++)
	{
		if( vj_event_list[i].event_id == event_id ) return i;
	}
	return -1;
}

char  *vj_event_name_by_id(int id)
{
	int i;
	for(i=1; vj_event_list[i].name; i++)
	{
		if(id == vj_event_list[i].event_id)
		{
			return vj_event_list[i].name;
		}
	}
	return NULL;
}

/* error statistics struct */


void vj_event_parse_msg(veejay_t *v, char *msg);

/* parse a message received from network */
void vj_event_parse_bundle(veejay_t *v, char *msg ) {

	int num_msg = 0;
	int offset = 3;
	int i = 0;
	
	
	if ( msg[offset] == ':' )
	{
		int j = 0;
		//veejay_msg(VEEJAY_MSG_INFO, "(VIMS) received message bundle [%s] %s",msg,msg+offset);
		/* starting from 4th position there is number of messages */
		offset += 1; /* skip ':' */
		if( sscanf(msg+offset, "%03d", &num_msg )<= 0 )
		{
			veejay_msg(VEEJAY_MSG_ERROR,"(VIMS) Invalid number of messages. Skipping message [%s] ",msg);
		}
		if ( num_msg <= 0 ) 
		{
			veejay_msg(VEEJAY_MSG_ERROR,"(VIMS) Invalid number of message given to execute. Skipping message [%s]",msg);
			//veejay_msg(VEEJAY_MSG_ERROR,"(VIMS) Bad number of messages in bundle");
			return;
		}

		offset += 3;

		if ( msg[offset] != '{' )
		{
			veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) '{' expected. Skipping message [%s]",msg);
			return;
		}	

		offset+=1;	/* skip # */

		for( i = 1; i <= num_msg ; i ++ ) /* iterate through message bundle and invoke parse_msg */
		{				
			char atomic_msg[256];
			int found_end_of_msg = 0;
			int total_msg_len = strlen(msg);
			bzero(atomic_msg,256);
			while( /*!found_end_of_msg &&*/ (offset+j) < total_msg_len)
			{
				if(msg[offset+j] == '}')
				{
					return; /* dont care about semicolon here */
				}	
				else
				if(msg[offset+j] == ';')
				{
					found_end_of_msg = offset+j+1;
					strncpy(atomic_msg, msg+offset, (found_end_of_msg-offset));
					atomic_msg[ (found_end_of_msg-offset) ] ='\0';
					offset += j + 1;
					j = 0;
					//veejay_msg(VEEJAY_MSG_DEBUG, "(VIMS) Atomic message [%s]", atomic_msg);
					vj_event_parse_msg( v, atomic_msg );
				}
				j++;
			}
		}
	}
}

void vj_event_dump()
{

	int i;
	for(i=0; i < NET_MAX; i++)
	{
	   if(net_list[i].list_id>0)
	   {	
	   int id = net_list[i].list_id;
	   //int id = i;
	   printf("\t%s\n", vj_event_list[id].name);
	   printf("\t%03d\t\t%s\n\n", i , vj_event_list[id].format);
	   }
	}
	vj_osc_dump();
}

void vj_event_print_range(int n1, int n2)
{
	int i;
	for(i=n1; i < n2; i++)
	{
		if(net_list[i].list_id > 0)
		{
			int id = net_list[i].list_id;
			if(vj_event_list[id].name != NULL )
			{
				veejay_msg(VEEJAY_MSG_INFO,"%03d\t\t%s\t\t%s",
					i, vj_event_list[id].format,vj_event_list[id].name);
			}
		}
	}
}


int vj_event_get_num_args(int net_id)
{
	int id = net_list[ net_id ].list_id;
	return (int) vj_event_list[id].num_params;	
}

void vj_event_fire_net_event(veejay_t *v, int net_id, char *str_arg, int *args, int arglen)
{
	int id = net_list[ net_id ].list_id;
	int  i = 0;

	if( arglen <= 0 )
	{
		veejay_msg(VEEJAY_MSG_DEBUG, 
			"(VIMS) Network %03d:  '%s' (defaults)",
			net_id,
			vj_event_list[id].name
		);

		vj_event_trigger_function(
			(void*)v,
			net_list[net_id].act,
			vj_event_list[id].num_params,
			vj_event_list[id].format,
			vj_event_list[id].args[0],
			vj_event_list[id].args[1]
		);

		return;
	}	

	if( str_arg != NULL && vj_event_list[id].format[1] == 's' )
	{
		veejay_msg(VEEJAY_MSG_DEBUG, 
			"(VIMS) Network %03d:   '%s' [%s]",
			net_id,
			vj_event_list[id].name,
			str_arg
		);

		for(i=0; i < 16; i++)
		{
			argument_list[i] = 0;
		}
	
		vj_event_trigger_function(
			(void*) v,
			net_list[net_id].act,
			vj_event_list[id].num_params,
			vj_event_list[id].format,
			str_arg,
			argument_list[0],
			argument_list[1],
			argument_list[2],
			argument_list[3],
			argument_list[4],
			argument_list[5],
			argument_list[6],	
			argument_list[7],
			argument_list[8],
			argument_list[9],
			argument_list[10],
			argument_list[11],
			argument_list[12]
		);

	}
	else
	{
		if(arglen > vj_event_list[id].num_params)
		{
			veejay_msg(VEEJAY_MSG_ERROR,
			 "(VIMS) Network %03d: called with too many arguments",net_id); 
			return;
		}
		if(v->verbose)
		{
			char str_args[100];
			bzero(str_args,100);
			for(i=0; i < arglen; i++)
			{
				char tmp[10];
				sprintf(tmp, "[%d]", args[i]);
				strncat(str_args, tmp, strlen(tmp));
			}

			veejay_msg(VEEJAY_MSG_DEBUG, 
			"(VIMS) Network %03d:   '%s' , args: %s",
			net_id,
			vj_event_list[id].name,
			str_args
		);
	

		}

		for(i=0; i < arglen; i++) 
		{
			argument_list[i] = args[i];
		}

		for(i=arglen; i < 16; i++)
		{
			argument_list[i] = 0;
		}
			vj_event_trigger_function(
			(void*) v,
			net_list[net_id].act,
			vj_event_list[id].num_params,
			vj_event_list[id].format,
			argument_list[0],
			argument_list[1],
			argument_list[2],
			argument_list[3],
			argument_list[4],
			argument_list[5],
			argument_list[6],
			argument_list[7],	
			argument_list[8],
			argument_list[9],
			argument_list[10],
			argument_list[11],
			argument_list[12],
			argument_list[13]
		);
	}
}


void	vj_event_macro_store_msg(veejay_t *v, char *msg)
{
	int frame_num = (int)(v->settings->current_frame_num);
	// todo: arrange offset
	if ( vj_macro_store( msg, frame_num ) == 0)
	{
		veejay_msg(VEEJAY_MSG_DEBUG, " (VIMS) Stored message in Macro Recorder ");
	}
}

void vj_event_parse_msg(veejay_t *v, char *msg)
{
	char args[150];
	int net_id=0;
	bzero(args,150);  

	/* message is at least 5 bytes in length */
	if( msg == NULL || strlen(msg) < MSG_MIN_LEN)
	{
	//	veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) Message [%s] too small. Dropped",(msg==NULL? "(Empty)": msg));
		return;
	}
	if(msg[0]=='B' && msg[1] == 'U' && msg[2] == 'N')
	{
		vj_event_parse_bundle(v,msg);
		return;
	}
	else {
		/* first token is net_id */
		if(sscanf(msg,"%03d", &net_id) <= 0)
		{
			//veejay_msg(VEEJAY_MSG_ERROR,"Invalid request. Dropped");
			return;
		}
	}

	if( net_id <= 0 || net_id >= NET_MAX)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) '%d' is not a valid action identifier", net_id);
		return;
	}

	/* 4th position is ':' */
	if( msg[3] != ':' )
	{
		veejay_msg(VEEJAY_MSG_ERROR,"(VIMS) Unexpected character '%c' must be ':' to indicate start of argument list ",
			msg[3]);
		return;
	}
	
	/* message must end with ';' */
	if( msg[strlen(msg)-1] != ';')
	{
		veejay_msg(VEEJAY_MSG_ERROR,"(VIMS) Unexpected character '%c' must be ';' to indicate end of message",msg[strlen(msg)-1]);
		return;
	}

	/* remove ';' from string */
	msg[strlen(msg)-1] = '\0';	

	if( strlen(msg) == (MSG_MIN_LEN-1) )
	{
		int id = net_list[net_id].list_id;
		veejay_msg(VEEJAY_MSG_DEBUG, 
			"(VIMS) Network %03d: '%s' defaults",
			net_id,
			vj_event_list[id].name
		);

		vj_event_trigger_function(
			(void*)v,net_list[net_id].act,
			vj_event_list[id].num_params,
			vj_event_list[id].format,
			vj_event_list[id].args[0],
			vj_event_list[id].args[1]
		);	
	}



	if( strlen(msg) >= MSG_MIN_LEN)
	{
		int id = net_list[ net_id ].list_id;
		int max_param = vj_event_list[id].num_params;
		int dargs[15];
		char dstr[512];
		
		
		int n = 0;
		int p = 0;
		int offset = 0;
		
	
		memset(dargs,0,15);
		strcpy(args,msg+4);

		if(vj_event_list[id].format != NULL && vj_event_list[id].format[1] == 's')
		{
			if(sscanf(args,"%s",dstr))
			{
			//	veejay_msg(VEEJAY_MSG_DEBUG,"(VIMS) first argument is string [%s].",dstr);
				offset+=strlen(dstr);
				n++;
			}
		}
		else
		{
			if(sscanf(args,"%d",&dargs[0]))
			{
				char dec[8];
				bzero(dec,8);
			 	snprintf(dec, 8, "%d", dargs[0]);
				offset += strlen(dec);
			/*
				//veejay_msg(VEEJAY_MSG_DEBUG,"(VIMS) first argument is number [%d].",dargs[0]);
				if(dargs[0] < 0) {
					if(dargs[0] <= -9) offset += 2;
					if(dargs[0] <= -99 && dargs[0] > -9) offset += 3;
					if(dargs[0] <= -999 && dargs[0] > -99) offset += 4;
				}
				else 
				{
					if(dargs[0] <= 9) offset += 1;
					if(dargs[0] <= 99 && dargs[0] > 9) offset += 2;	 
					if(dargs[0] <= 999 && dargs[0] > 99) offset += 3;
					if(dargs[0] <= 9999 && dargs[0] > 999) offset += 4;
					if(dargs[0] <= 99999 && dargs[0] >  9999) offset += 5;
					if(dargs[0] <= 999999 && dargs[0] > 99999) offset += 6;
					if(dargs[0] <= 9999999 && dargs[0]> 999999) offset += 7; 
				}
			*/
			//	veejay_msg(VEEJAY_MSG_DEBUG,"(VIMS) next arguments start at [%s]",args+offset);
				n++;
			}

		}

		p = sscanf(args+offset,"%d %d %d %d %d %d %d %d %d %d %d %d %d %d",&dargs[1],&dargs[2],&dargs[3],&dargs[4],&dargs[5],&dargs[6],&dargs[7],
			&dargs[8],&dargs[9],&dargs[10],&dargs[11],&dargs[12],&dargs[13],&dargs[14]);

		if( p > 0 )
		{
			n += p;
		}
		if( n > vj_event_list[id].num_params)
		{
				veejay_msg(VEEJAY_MSG_ERROR, "(VIMS) Network: %03d has too many parameters (%d given, need %d)!",
					net_id, n,vj_event_list[id].num_params);
				return;
		}
		veejay_msg(VEEJAY_MSG_DEBUG,
			"(VIMS) Network: %03d '%s', with %d parameters",
			net_id,
			vj_event_list[id].name,
			n);
	
		/* va_list/va_arg do not support an arbitrary ordered argument list for a function,
		   template 'format' defines a fixed order of ints,chars*,..., so '%s' is defined to be the first
		   argument in a message , if not there are only numbers.
		   the otherway arround may be va_copy to have 2 tries but I think an expression would be
		   less expansive.
		*/ 
		   
		if ( vj_event_list[id].format[1] == 's' )
		{
			vj_event_trigger_function(
			(void*)v,
			net_list[ net_id ].act,
			max_param,
			vj_event_list[id].format,
			dstr,
			dargs[1],
			dargs[2],
			dargs[3],
			dargs[4],
			dargs[5],
			dargs[6],
			dargs[7],
			dargs[8],
			dargs[9],
			dargs[10],
			dargs[11],
			dargs[12],
			dargs[13],
			dargs[14],
			dargs[15]
		) ;	

		}
		else
		{
		vj_event_trigger_function(
			(void*)v,
			net_list[ net_id ].act,
			max_param,
			vj_event_list[id].format,
			dargs[0],
			dargs[1],
			dargs[2],
			dargs[3],
			dargs[4],
			dargs[5],
			dargs[6],
			dargs[7],
			dargs[8],
			dargs[9],
			dargs[10],
			dargs[11],
			dargs[12],
			dargs[13],
			dargs[14]
		) ;	
		}
	}	
}	

void vj_event_update_remote(void *ptr)
{
	veejay_t *v = (veejay_t*)ptr;
	int i;
	if(vj_server_poll(v->vjs))
	{

		int p = vj_server_update(v->vjs);	
		if( p==0 )
		{
			veejay_msg(VEEJAY_MSG_WARNING, "New connection %d",p);
		}
		else {
			char buf[MESSAGE_SIZE];
			bzero( buf, MESSAGE_SIZE );
			for(i=0; i < VJ_MAX_CONNECTIONS; i++)
			{
				while(vj_server_retrieve_msg(i,buf)==0)
				{
				v->uc->current_link = i;
				vj_event_parse_msg(v,buf);
			//	bzero(buf, MESSAGE_SIZE);	
				}
			}
		}
	}
}

void	vj_event_commit_bundle( veejay_t *v, unsigned int key_num)
{
	char bundle[1024];
	bzero(bundle,1024);
	vj_event_create_effect_bundle(v, bundle, key_num );
}

void vj_event_single_fire(void *ptr , SDL_Event event, int pressed)
{
	
	SDL_KeyboardEvent *key = &event.key;
	SDLMod mod = key->keysym.mod;
	vj_event f;
	int id;
	char word[30];

	if( (mod & KMOD_LSHIFT) || (mod & KMOD_RSHIFT))
	{
		sprintf(word, "SHIFT + %d",key->keysym.sym);
		f = sdl_shift[ key->keysym.sym ].act;
		id = sdl_shift[ key->keysym.sym].list_id;
	}
	else
	{
		if ( (mod & KMOD_LALT) || (mod & KMOD_RALT))
		{
			//veejay_msg(VEEJAY_MSG_DEBUG,"Alt modifier for key %d",key->keysym.sym);
			sprintf(word, "ALT + %d" , key->keysym.sym);
			f = sdl_alt[ key->keysym.sym].act;
			id = sdl_alt[ key->keysym.sym].list_id;
		}	
		else
		{
			if ( (mod & KMOD_LCTRL) || (mod & KMOD_RCTRL)) 
			{
				sprintf(word, "CTRL(burn) + %d",key->keysym.sym);
				f = sdl_ctrl[ key->keysym.sym].act;
				id = sdl_ctrl[ key->keysym.sym].list_id;
				vj_event_commit_bundle( (veejay_t*) ptr, key->keysym.sym);
				return; 
			}
			else
			{
				word[0] = '\0';
				f = sdl_normal[ key->keysym.sym ].act;
				id = sdl_normal[ key->keysym.sym ].list_id;
			}
		}
	}

	if( vj_event_list[id].event_id == VJ_EVENT_BUNDLE )
	{
		/* networked message, parameters in message
                   do_bundle finds content by looking in hash of custom events  
		   */
		veejay_msg(VEEJAY_MSG_DEBUG,"(VIMS) Key %s (custom bundle)",
			word[0] == '\0' ? " " : word);
		vj_event_trigger_function( 
			ptr, 
			f, 
			vj_event_list[id].num_params,
			vj_event_list[id].format, 
			sdl_parameter[key->keysym.sym ]
		);
	}
	else {

	if( vj_event_list[id].num_params == 0 )
	{
		//veejay_msg(VEEJAY_MSG_DEBUG,"[Key] Fire event %d [%s] ", 
		//	vj_event_list[id].event_id,
		//	vj_event_list[id].name);
		veejay_msg(VEEJAY_MSG_DEBUG, "(VIMS) Key %s [%s]",
			 word[0] == '\0' ? " " : word ,
			 vj_event_list[id].name );
		f(ptr,NULL,NULL);

	}	
	else 
	{
		veejay_msg(VEEJAY_MSG_DEBUG, "(VIMS) Key %s [%s]",
				word[0] == '\0' ? " " : word ,
				 vj_event_list[id].name );

		vj_event_trigger_function( ptr, f, vj_event_list[id].num_params, vj_event_list[id].format, vj_event_list[id].args[0], vj_event_list[id].args[1] );

//		veejay_msg(VEEJAY_MSG_DEBUG,"[Key] Fire event %d [%s] format [%s] default 0 [%d] default 1 [%d]",
//			vj_event_list[id].event_id,
//			vj_event_list[id].name,
//			vj_event_list[id].format,
//			vj_event_list[id].args[0],
//			vj_event_list[id].args[1]
//		);

	
	}

	}
}



void vj_event_none(void *ptr, const char format[], va_list ap)
{
	veejay_msg(VEEJAY_MSG_DEBUG, "No event attached on this key");
}

#define XMLTAG_KEY_ID "key"
#define XMLTAG_KEY_MODE "key_mod"
#define XMLTAG_KEY_EVENT "bundle_id"
#define XMLTAG_KEY_EVENT_MSG "bundle_msg"

void vj_event_xml_new_keyb_event( xmlDocPtr doc, xmlNodePtr cur )
{
	xmlChar *xmlTemp = NULL;
	unsigned char *chTemp = NULL;
	unsigned char msg[512];
		int key = -1;
		int event_id = 0;
		int key_mode = -1;

	
	while( cur != NULL )
	{
		if( !xmlStrcmp( cur->name, (const xmlChar *) XMLTAG_KEY_EVENT))
		{
			xmlTemp = xmlNodeListGetString( doc, cur->xmlChildrenNode, 1);
			chTemp = UTF8toLAT1( xmlTemp );
			if( chTemp )
			{
				event_id = atoi( chTemp );
			}
		}

		if( !xmlStrcmp( cur->name, (const xmlChar *) XMLTAG_KEY_EVENT_MSG))
		{
			xmlTemp = xmlNodeListGetString( doc, cur->xmlChildrenNode, 1);
			chTemp = UTF8toLAT1( xmlTemp );
			if( chTemp )
			{
				sprintf( msg, "%s", chTemp);	
			}
		}

		if( !xmlStrcmp( cur->name, (const xmlChar *) XMLTAG_KEY_ID ))
		{
			xmlTemp = xmlNodeListGetString( doc, cur->xmlChildrenNode, 1);
			chTemp = UTF8toLAT1( xmlTemp );
			if(chTemp)
			{
				key = atoi( chTemp );
			}
		}
	 	if( !xmlStrcmp( cur->name, (const xmlChar *) XMLTAG_KEY_MODE))
		{
			xmlTemp = xmlNodeListGetString( doc, cur->xmlChildrenNode, 1);
			chTemp = UTF8toLAT1( xmlTemp );
			if(chTemp)
			{
				key_mode = atoi(chTemp);
			}
		}

		if(xmlTemp) xmlFree(xmlTemp);
		if(chTemp) free(chTemp);	
		xmlTemp = NULL;
		chTemp = NULL;

		cur = cur->next;
	}

	if(event_id > 0) 
		{
			vj_msg_bundle *m = vj_event_bundle_new( msg, event_id);
			if(m)
			{
				vj_event_bundle_store(m);
				sdl_parameter[key] = event_id;
				if( key != -1 && key_mode != -1)
				{
					if( register_new_bundle_as_key( key, key_mode ))
					{
						veejay_msg(VEEJAY_MSG_INFO, "Attached key %d to Bundle %d ", key,event_id);
					}
				}
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR , "Unable to create new event %d", event_id);
			}
		}	
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR , "Invalid bundle contents ");
		}

}

int vj_event_register_keyb_event(int event_id, int event_entry)
{
	int i;
	for(i=1; vj_event_sdl_keys[i].event_id != 0 ; i++) 
	{	
		if(event_id == vj_event_sdl_keys[i].event_id)
		{
			int j = vj_event_sdl_keys[i].sdl_key;
			if( vj_event_sdl_keys[i].alt_mod) 
			{
				sdl_alt[ j ].act	= vj_event_list[event_entry].function;
				sdl_alt[ j ].list_id	= event_entry;
			}
			else 
			if( vj_event_sdl_keys[i].shift_mod) 
			{
				sdl_shift[ j ].act	= vj_event_list[event_entry].function;
				sdl_shift[ j ].list_id	= event_entry;
			}
			else
			if( vj_event_sdl_keys[i].ctrl_mod) 
			{
				sdl_ctrl[ j ].act = vj_event_list[event_entry].function;
				sdl_ctrl[ j ].list_id = event_entry;
			}
			else
			{
				sdl_normal[ j ].act = vj_event_list[event_entry].function;
				sdl_normal[ j ].list_id = event_entry;
			}	
			//veejay_msg(VEEJAY_MSG_DEBUG, "(VIMS) Registered SDL keybinding '%s'",vj_event_list[event_entry].name);
			return 1;
		}
	}
	return 0;
}

static void vj_event_get_key( int event_id, int *key_id, int *key_mod )
{
	int i = 0;
	for( i  = 0 ; i < MAX_SDL_KEY; i++)
	{
		if( sdl_parameter[i] == event_id )
		{
			int id = vj_event_get_id( VJ_EVENT_BUNDLE );
			*key_id = i;
	
			if(sdl_normal[i].list_id == id)
				*key_mod = 0;
			if(sdl_shift[i].list_id == id)
				*key_mod = 3;
			if(sdl_ctrl[i].list_id == id)
				*key_mod = 1;
		 	if(sdl_alt[i].list_id == id)
				*key_mod = 2;
			return;
		}
	}
	
	*key_id  = -1;
	*key_mod = -1;
}

void vj_event_set_new_sdl_key(int event_entry,int sdl_key, int modifier)
{

	switch(modifier)
	{
		case 0:	
			sdl_normal[ sdl_key ].act= vj_event_do_bundled_msg;
			sdl_normal[ sdl_key ].list_id = event_entry;
			break;
		case 1:
			sdl_ctrl[ sdl_key ].act = vj_event_do_bundled_msg;
			sdl_ctrl[ sdl_key ].list_id = event_entry;
			break;
		case 2: 
			sdl_alt[ sdl_key ].act = vj_event_do_bundled_msg;
			sdl_alt[ sdl_key ].list_id = event_entry;
			break;
		case 3:
			sdl_shift[ sdl_key ].act = vj_event_do_bundled_msg;
			sdl_shift[ sdl_key ].list_id = event_entry;
		break;
	}
}

int vj_event_register_network_event(int event_id, int event_entry)
{
	int i;
	for(i=1; vj_event_remote_list[i].event_id != 0 ; i++)
	{
		if(event_id == vj_event_remote_list[i].event_id)
		{
			net_list[ vj_event_remote_list[i].internal_msg_id ].act = vj_event_list[event_entry].function;
			net_list[ vj_event_remote_list[i].internal_msg_id ].list_id = event_entry;
			//veejay_msg(VEEJAY_MSG_DEBUG, "(VIMS) Registered Network event '%s'",vj_event_list[event_entry].name);
			return 1;
		}
	}
	return 0;
}
	

int register_new_bundle_as_key(int sdl_key, int modifier) 
{

	if( sdl_key > 0 && sdl_key <= MAX_SDL_KEY && modifier >=0 && modifier <= 3)
	{
		int event_entry = vj_event_get_id( VJ_EVENT_BUNDLE );
	
		vj_event_set_new_sdl_key( event_entry,sdl_key,modifier );

		return 1;
	}
	return 0;
}
void vj_event_init()
{
	int keyb_events = 0;
	int net_events = 0;
	int i;
	/*
	   intialize all SDL keybinding to none
           this sets up the sdl key to veejay function translation table
           the sdl key symbol is then the identifer directly to the 
	   desired function 
	*/
	for(i=0; i < MAX_SDL_KEY; i++)
	{
		sdl_normal[i].act = vj_event_none;
		sdl_normal[i].list_id = 0;
		sdl_shift[i].act = vj_event_none;
		sdl_shift[i].list_id = 0;
		sdl_alt[i].act = vj_event_none;
		sdl_alt[i].list_id = 0;
		sdl_ctrl[i].act = vj_event_none;
		sdl_ctrl[i].list_id = 0;
		sdl_parameter[i] = 0;
	}
	for(i=0; i < NET_MAX; i++)
	{
		net_list[i].act = vj_event_none;
		net_list[i].list_id = 0;
	}

	/* 
	  setup the SDL keybinding translation table
	*/
	for(i=1; vj_event_list[i].event_id != 0 ; i++) 
	{
		
	/*
		veejay_msg(VEEJAY_MSG_INFO, "Listing event [%d]\n\tDescription: [%s]\n\tString format:[%s]\n\tNumber of args:[%d]\n\tDefault 0:[%d]\n\tDefault 1:[%d]", 
			vj_event_list[i].event_id,
			vj_event_list[i].name,
			vj_event_list[i].format,
			vj_event_list[i].num_params,
			vj_event_list[i].args[0],
			vj_event_list[i].args[1]
		);
	*/
		if( vj_event_register_keyb_event( vj_event_list[i].event_id,i ) != 0 )
		{
//			veejay_msg(VEEJAY_MSG_ERROR, "Keyboard event %d does not exist", vj_event_list[i].event_id);
			keyb_events++;
		}
		if ( vj_event_register_network_event( vj_event_list[i].event_id, i ) != 0)
		{
//			veejay_msg(VEEJAY_MSG_ERROR, "Network event %d does not exist", vj_event_list[i].event_id);
			net_events++;
		}
	}
	/*
	  some statistics
	*/
	veejay_msg(VEEJAY_MSG_INFO, "(VIMS) Registered %d Network Events",net_events);
	veejay_msg(VEEJAY_MSG_INFO, "(VIMS) Registered %d Keyboard Events",keyb_events);
//	vj_event_dump();
	
	if( !(BundleHash = hash_create(HASHCOUNT_T_MAX, int_bundle_compare, int_bundle_hash)))
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot initialize hashtable for message bundles");
		return;
	}

}


/* function for parsing a va_list */

void vj_event_fmt_arg(int *args, char *str, const char format[], va_list ap)
{
	int d;
	int i=0;
	char *s;
	// fixme :: make it if statement
	while(*format) 
	{
		switch(*format++)
		{
			case 's':
				s = va_arg(ap, char*);
				if(str==NULL)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "vj event %d %s No string [%s] expected!",__LINE__,__FILE__, s );
				}
				else 
				{
					sprintf(str,"%s",s);
				}
				break;
			case 'd':
				d = va_arg(ap, int);
				if(args==NULL) 
				{
					veejay_msg(VEEJAY_MSG_ERROR, "vj event %d %s No integer [%d] expected!", __LINE__,__FILE__,d);
				}
				else 
				{
					args[i] = d;
					i++;
				}
				break;
			default:
				//veejay_msg(VEEJAY_MSG_ERROR, "vj event %d %s Unexpected type [%c]", __LINE__,__FILE__, (char) (*format));
				break;
		}
	}
} 

/* P_A: Parse Arguments. This macro is used in many functions */
// make it fixme:
#define P_A(a,b,c,d)\
{\
int __z = 0;\
if(a!=NULL){\
unsigned int __rp;\
unsigned int __rplen = (sizeof(a) / sizeof(int) );\
for(__rp = 0; __rp < __rplen; __rp++) a[__rp] = 0;\
}\
while(*c) { switch(*c++) { case 's': sprintf( b,"%s",va_arg(d,char*) ); break; case 'd': a[__z] = va_arg(d, int); __z++ ; break; } }\
}


#define DUMP_ARG(a)\
if(sizeof(a)>0){\
int __l = sizeof(a)/sizeof(int);\
int __i; for(__i=0; __i < __l; __i++) veejay_msg(VEEJAY_MSG_DEBUG,"[%02d]=[%06d], ",__i,a[__i]);}\
else { veejay_msg(VEEJAY_MSG_DEBUG,"arg has size of 0x0");}


void vj_event_quit(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	veejay_change_state(v, LAVPLAY_STATE_STOP);         
}

void  vj_event_sample_mode(void *ptr,	const char format[],	va_list ap)
{
	veejay_t *v = (veejay_t *) ptr;
	if(v->settings->sample_mode == SSM_420_JPEG_BOX)
		veejay_set_sampling( v, SSM_420_JPEG_TR );
	else
		veejay_set_sampling( v, SSM_420_JPEG_BOX );
	veejay_msg(VEEJAY_MSG_WARNING, "Sampling of 4:2:0 -> 4:4:4 is set to [%s]",
		(v->settings->sample_mode == SSM_420_JPEG_BOX ? "lame box filter" : "triangle linear filter")); 
}

void vj_event_bezerk(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if(v->no_bezerk) v->no_bezerk = 0; else v->no_bezerk = 1;
	veejay_msg(VEEJAY_MSG_INFO, "Bezerk mode is %s", (v->no_bezerk==0? "enabled" : "disabled"));
	if(v->no_bezerk==1)
	veejay_msg(VEEJAY_MSG_DEBUG,"No clip-restart when changing input channels");
	else
	veejay_msg(VEEJAY_MSG_DEBUG,"Clip-restart when changing input channels"); 
}

void vj_event_debug_level(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if(v->verbose) v->verbose = 0; else v->verbose = 1;
	if(v->verbose==0) veejay_msg(VEEJAY_MSG_INFO,"Not displaying debug information");  
	veejay_set_debug_level( v->verbose );

}

void vj_event_suspend(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	veejay_change_state(v, LAVPLAY_STATE_PAUSED);
}

void vj_event_set_play_mode_go(void *ptr, const char format[], va_list ap) 
{
	int args[2];
	veejay_t *v = (veejay_t*) ptr;
	char *s = NULL;

	P_A(args,s,format,ap);
	if(vj_event_valid_mode(args[0]))
	{
		if(args[0] == VJ_PLAYBACK_MODE_PLAIN) 
		{
			veejay_change_playback_mode(v, args[0], 0);
			return;
		}
	
		if(args[0] == VJ_PLAYBACK_MODE_CLIP) 
		{
			if(args[1]==0) args[1] = v->uc->clip_id;
			if(args[1]==-1) args[1] = clip_size()-1;
			if(clip_exists(args[1]))
			{
				veejay_change_playback_mode(v,args[0] ,args[1]);
			}
			else
			{	
				p_no_clip(args[1]);
			}
		}
		if(args[0] == VJ_PLAYBACK_MODE_TAG)
		{
			if(args[1]==0) args[1] = v->uc->clip_id;
			if(args[1]==-1) args[1] = clip_size()-1;
			if(vj_tag_exists(args[1]))
			{
				veejay_change_playback_mode(v,args[0],args[1]);
			}
			else
			{
				p_no_tag(args[1]);
			}
		}
	}
	else {
		veejay_msg(VEEJAY_MSG_ERROR,"%s",vj_event_err(VJ_ERROR_INVALID_MODE));
	}
}


void	vj_event_clip_add_wave( void *ptr,	const char format[], va_list ap)
{
	char file_name[512];
	int args[1];
	veejay_t *v = (veejay_t *) ptr;
	P_A(args,file_name,format,ap);

	if(CLIP_PLAYING(v))
	{
		if(v->settings->clip_record)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Recorder active, please stop the clip recorder first.");
			return;
		}
		if( !clip_start_external( v->uc->clip_id, file_name, v->editlist ) )
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Cannot use WAV file %s", file_name); 
		}

	}	
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR,"Add a WAVE file only for Clips");
	}
}

void	vj_event_clip_del_wave( void *ptr,	const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*) ptr;
	if(CLIP_PLAYING(v))
	{
		if(v->settings->clip_record)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Clip Recorder is active (WAV file is in use) ");
			return;
		}
		if(clip_use_external(v->uc->clip_id))
		{
			if(clip_close_external(v->uc->clip_id))
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Removed WAV file ");
			}
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Clip %d is not using an external wave file ", v->uc->clip_id);
		}
	}
}


void	vj_event_read_file( void *ptr, 	const char format[], va_list ap )
{
	char file_name[512];
	int args[1];
	veejay_t *v = (veejay_t*) ptr;

	P_A(args,file_name,format,ap);
	veejay_load_action_file( v, file_name );
}

void	vj_event_format_xml_bundle( xmlNodePtr node, int event_id )
{
	char buffer[512];
	vj_msg_bundle *m;
	
	int key_id, key_mod;
	m = vj_event_bundle_get( event_id );
	if(!m) return;

	vj_event_get_key( event_id, &key_id, &key_mod );

	sprintf(buffer, "%d", event_id);
	xmlNewChild(node, NULL, (const xmlChar*) XMLTAG_KEY_EVENT , 
		(const xmlChar*) buffer);
	sprintf(buffer, "%s", m->bundle );
	xmlNewChild(node, NULL, (const xmlChar*) XMLTAG_KEY_EVENT_MSG ,
		(const xmlChar*) buffer);


	if(key_id != -1 && key_mod !=-1 )
	{
		sprintf(buffer, "%d", key_id );
		xmlNewChild(node, NULL, (const xmlChar*) XMLTAG_KEY_ID ,
			(const xmlChar*) buffer);
		sprintf(buffer, "%d", key_mod );
		xmlNewChild(node, NULL, (const xmlChar*) XMLTAG_KEY_MODE ,
			(const xmlChar*) buffer);
	}
}

void vj_event_effect_set_bg(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
//	uint8_t *ptr[3];
	//veejay_msg(VEEJAY_MSG_INFO, "Warning experimental feature. Taking current frame as static background image");
//	vj_perform_get_primary_frame( v, ptr , v->settings->currently_processed_entry);
//	veejay_msg(VEEJAY_MSG_INFO, "Warning: taking current frame as static bg (%p)", ptr[0]);
//	diff_set_background( ptr[0], v->editlist->video_width * v->editlist->video_height );
	v->uc->take_bg = 1;
	veejay_msg(VEEJAY_MSG_INFO, "Next frame will be taken for static background\n");
}
void vj_event_write_actionfile(void *ptr, const char format[], va_list ap)
{
	char file_name[512];
	int args[1];
	int saved_bundles = 0;
	int i;
	//veejay_t *v = (veejay_t*) ptr;
	xmlDocPtr doc;
	xmlNodePtr rootnode,childnode;	
	P_A(args,file_name,format,ap);

	doc = xmlNewDoc( "1.0" );
	rootnode = xmlNewDocNode( doc, NULL, (const xmlChar*) XMLTAG_BUNDLE_FILE,NULL);
	xmlDocSetRootElement( doc, rootnode );
	
	// iterate trough bundles and stoer
	for( i = 5000; i < 6000; i++)
	{
		if( vj_event_bundle_exists(i))	
		{
			childnode = xmlNewChild( rootnode,NULL,(const xmlChar*) XMLTAG_EVENT_AS_KEY ,NULL);
			vj_event_format_xml_bundle( childnode, i );
			saved_bundles ++;
		}
	}

	xmlSaveFormatFile( file_name, doc, 1);
	xmlFreeDoc(doc);	
	if( saved_bundles )
	{
		veejay_msg(VEEJAY_MSG_INFO, "Wrote %d bundles to Action File %s", saved_bundles, file_name );
	}
}

void vj_event_clip_select(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *s = NULL;	
	P_A( args, s , format, ap);

	if(args[0] == 0 )
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1)
	{
		args[0] = clip_size()-1;
	}
	if(clip_exists(args[0]))
	{
		veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_CLIP,args[0] );
	}
	else
	{
		p_no_clip(args[0]);
	}
}

void vj_event_tag_select(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *s = NULL;	
	P_A( args, s , format, ap);

	if(args[0] == 0 )
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0]==-1)
	{
		args[0] = vj_tag_size()-1;
	}

	if(vj_tag_exists(args[0]))
	{
		veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_TAG,args[0]);
	}
	else
	{
		p_no_tag(args[0]);
	}
}


void vj_event_switch_clip_tag(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;

	if(!TAG_PLAYING(v) && !CLIP_PLAYING(v))
	{
		if(clip_exists(v->last_clip_id)) 
		{
			veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_CLIP, v->last_clip_id);
			return;
		}
		if(vj_tag_exists(v->last_tag_id))
		{
			veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_TAG, v->last_tag_id);
			return;
		}
		if(clip_size()-1 <= 0)
		{
			if(vj_tag_exists( vj_tag_size()-1 ))
			{
				veejay_change_playback_mode( v, VJ_PLAYBACK_MODE_TAG, vj_tag_size()-1);
				return;
			}
		}	
	}

	if(CLIP_PLAYING(v))
	{
		if(vj_tag_exists(v->last_tag_id))
		{
			veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_TAG, v->last_tag_id);
		}
		else
		{
			int id = vj_tag_size() - 1;
			if(id)
			{
				veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_TAG,id);
			}
			else
			{
				p_no_tag(id);
			}
		}
	}
	else
	if(TAG_PLAYING(v))
	{
		if(clip_exists(v->last_clip_id) )
		{
			veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_CLIP, v->last_clip_id);
		}
		else
		{
			int id = clip_size() - 1;
			if(id)
			{
				veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_CLIP,id);
			}
			else
			{
				p_no_clip(id);
			}
		}
	}
}

void	vj_event_macro_record_start( void *ptr, const char format[], va_list ap )
{

}

void	vj_event_macro_record_stop(	void *ptr, const char format[], va_list ap)
{

}

void	vj_event_macro_play_start( void *ptr, const char format[], va_list ap)
{

}

void	vj_event_macro_play_stop( void *ptr, const char format[], va_list ap)
{

}

void	vj_event_set_volume(void *ptr, const char format[], va_list ap)
{
	int args[1];	
	char *s = NULL;
	P_A(args,s,format,ap)
	if(args[0] >= 0 && args[0] <= 100)
	{
#ifdef HAVE_JACK
		if(vj_jack_set_volume(args[0]))
		{
			veejay_msg(VEEJAY_MSG_INFO, "Volume set to %d", args[0]);
		}
#endif
	}
}
void vj_event_set_play_mode(void *ptr, const char format[], va_list ap)
{
	int args[1];
	char *s = NULL;
	veejay_t *v = (veejay_t*) ptr;
	P_A(args,s,format,ap);

	if(vj_event_valid_mode(args[0]))
	{
		int mode = args[0];
		/* check if current playing ID is valid for this mode */
		if(mode == VJ_PLAYBACK_MODE_CLIP)
		{
			int last_id = clip_size()-1;
			if(last_id == 0)
			{
				veejay_msg(VEEJAY_MSG_ERROR, "There are no clips. Cannot switch to clip mode");
				return;
			}
			if(!clip_exists(v->last_clip_id))
			{
				v->uc->clip_id = last_id;
			}
			if(clip_exists(v->uc->clip_id))
			{
				veejay_change_playback_mode( v, VJ_PLAYBACK_MODE_CLIP, v->uc->clip_id );
			}
		}
		if(mode == VJ_PLAYBACK_MODE_TAG)
		{
			int last_id = vj_tag_size()-1;
			if(last_id == 0)
			{
				veejay_msg(VEEJAY_MSG_ERROR, "There are no streams. Cannot switch to stream mode");
				return;
			}
			
			if(!vj_tag_exists(v->last_tag_id)) /* jump to last used Tag if ok */
			{
				v->uc->clip_id = last_id;
			}
			if(vj_tag_exists(v->uc->clip_id))
			{
				veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_TAG, v->uc->clip_id);
			}
		}
		if(mode == VJ_PLAYBACK_MODE_PLAIN)
			veejay_change_playback_mode( v, VJ_PLAYBACK_MODE_PLAIN, 0);
	}
	else 
	{
		veejay_msg(VEEJAY_MSG_ERROR,"%s",vj_event_err(VJ_ERROR_INVALID_MODE));
	}
}

void vj_event_clip_new(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if(CLIP_PLAYING(v) || PLAIN_PLAYING(v)) 
	{
		int args[2];
		char *s = NULL;
		int num_frames = v->editlist->video_frames-1;
		P_A(args,s,format,ap);

		if(args[0] < 0)
		{
			/* count from last frame */
			int nframe = args[0];
			args[0] = v->editlist->video_frames - 1 + nframe;
		}
		if(args[0] == 0)
			args[0] = 1;

		if(args[1] == 0)
		{
			args[1] = v->editlist->video_frames - 1;
		}

		if(args[0] > 0 && args[1] > 0 && args[0] <= args[1] && args[0] <= num_frames &&
			args[1] <= num_frames ) 
		{
			clip_info *skel = clip_skeleton_new(args[0],args[1]);
			if(clip_store(skel)==0) 
			{
				veejay_msg(VEEJAY_MSG_INFO, "Created new clip [%d]", skel->clip_id);
				clip_set_looptype(skel->clip_id,1);
			}
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Invalid frame range given : %d - %d , range is %d - %d",
				args[0],args[1], 1,num_frames);
		}
	}
	else 
	{
		p_invalid_mode();
	}
}

void vj_event_set_screen_size(void *ptr, const char format[], va_list ap) 
{
	int args[2];
	veejay_t *v = (veejay_t*) ptr;
	char *s = NULL;
	P_A(args,s,format,ap);
	if( args[0] > 0 && args[1] > 0 && args[0] < 4096 && args[1] < 4096 ) 
	{
		char title[20];
		sprintf(title,"Veejay %s",VERSION);
		if(v->video_out == 0 || v->video_out == 2)
		{
			vj_sdl_free(v->sdl);
					vj_sdl_init(v->sdl,args[0],args[1],title, 1);
			veejay_msg(VEEJAY_MSG_INFO, "Resized SDL screen to %d x %d (video is %ld x %ld)",
				args[0],args[1],v->editlist->video_width,v->editlist->video_height);
		}
		if(v->gui_screen == 1)
		{
			vj_sdl_free(v->sdl_gui);
			if(windowID != 0)
			{
				char env[100];
				sprintf(env,"SDL_WINDOWID=%ld",windowID);
				putenv(env);
				veejay_msg(VEEJAY_MSG_INFO, "Using GUI's Window ID %ld",windowID);	
			}

			vj_sdl_init(v->sdl_gui,args[0],args[1],title,1);
			veejay_msg(VEEJAY_MSG_INFO,"Resized GUI SDL screen to %d x %d (video is %ld x %ld)",
				args[0],args[1],v->editlist->video_width,v->editlist->video_height);
		}
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR,"%s %dx%d", vj_event_err(VJ_ERROR_DIMEN) ,args[0],args[1]);
	}
}

void vj_event_play_stop(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*) ptr;
	if(!TAG_PLAYING(v))
	{
		veejay_set_speed(v, (v->uc->speed == 0 ? v->uc->speed = 1 : 0  ));
		veejay_msg(VEEJAY_MSG_INFO,"Video is %s", (v->uc->speed==0 ? "paused" : "playing"));
	}
}

void vj_event_play_reverse(void *ptr,const char format[],va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	if(!TAG_PLAYING(v))
	{
	if( v->uc->speed == 0 ) v->uc->speed = -1;
	else if(v->uc->speed > 0) v->uc->speed = -(v->uc->speed);

	veejay_set_speed(v,
			v->uc->speed );

	veejay_msg(VEEJAY_MSG_INFO, "Video is playing in reverse at speed %d.", v->uc->speed);
	}
}

void vj_event_play_forward(void *ptr, const char format[],va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	if(!TAG_PLAYING(v))
	{
	if(v->uc->speed == 0) v->uc->speed = 1;
	else if(v->uc->speed < 0 ) v->uc->speed = -1 * v->uc->speed;

 	veejay_set_speed(v,
		v->uc->speed );  

	veejay_msg(VEEJAY_MSG_INFO, "Video is playing forward at speed %d" , v->uc->speed);
	}
}

void vj_event_play_speed(void *ptr, const char format[], va_list ap)
{
	int args[1];
	veejay_t *v = (veejay_t*) ptr;
	if(!TAG_PLAYING(v))
	{
	char *s = NULL;
	int speed = v->uc->speed;
	P_A(args,s,format,ap);

	if(v->uc->speed >= 0)
	{
		speed = args[0];
	}
	else
	{
		speed = -1 * args[0];
	}
	
	veejay_set_speed(v, speed);

	veejay_msg(VEEJAY_MSG_INFO, "Video is playing at speed %d now (%s)",
		v->uc->speed , v->uc->speed == 0 ? "paused" : v->uc->speed < 0 ? "reverse" : "forward" );
	}
}

void vj_event_play_slow(void *ptr, const char format[],va_list ap)
{
	int args[1];
	veejay_t *v = (veejay_t*)ptr;
	char *s = NULL;
	P_A(args,s,format,ap);
	
	if(PLAIN_PLAYING(v) || CLIP_PLAYING(v))
	{
		if(veejay_set_framedup(v, args[0]))
		{
			veejay_msg(VEEJAY_MSG_INFO,"Video frame will be duplicated %d to output",args[0]);
		}
	}
	
}


void vj_event_set_frame(void *ptr, const char format[], va_list ap)
{
	int args[1];
	veejay_t *v = (veejay_t*) ptr;
	if(!TAG_PLAYING(v))
	{
	video_playback_setup *s = v->settings;
	char *str = NULL;
	P_A(args,str,format,ap);
	veejay_set_frame(v, args[0]);
	veejay_msg(VEEJAY_MSG_INFO, "Video frame %d set",s->current_frame_num);
	}
}

void vj_event_inc_frame(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if(!TAG_PLAYING(v))
	{
	video_playback_setup *s = v->settings;
	veejay_set_frame(v, (s->current_frame_num + 1));
	veejay_msg(VEEJAY_MSG_INFO, "Skipped frame");
	}
}

void vj_event_dec_frame(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *) ptr;
	if(!TAG_PLAYING(v))
	{
	video_playback_setup *s = v->settings;
	veejay_set_frame(v, (s->current_frame_num - 1));
	veejay_msg(VEEJAY_MSG_INFO, "Previous frame");
	}
}

void vj_event_prev_second(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;	
	if(!TAG_PLAYING(v))
	{
	video_playback_setup *s = v->settings;
	veejay_set_frame(v, (s->current_frame_num - (int) 
			    v->editlist->video_fps));
	
	}
}

void vj_event_next_second(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	video_playback_setup *s = v->settings;
	veejay_set_frame(v, (s->current_frame_num + (int)
			    v->editlist->video_fps));
}


void vj_event_clip_start(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	video_playback_setup *s = v->settings;
	if(CLIP_PLAYING(v) || PLAIN_PLAYING(v)) 
	{
		v->uc->clip_start = s->current_frame_num;
		veejay_msg(VEEJAY_MSG_INFO, "Clip starting position set to frame %ld", v->uc->clip_start);
	}	
	else
	{
		p_invalid_mode();
	}
}



void vj_event_clip_end(void *ptr, const char format[] , va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	video_playback_setup *s = v->settings;
	if(CLIP_PLAYING(v) || PLAIN_PLAYING(v))
	{
		v->uc->clip_end = s->current_frame_num;
		if( v->uc->clip_end > v->uc->clip_start) {
			clip_info *skel = clip_skeleton_new(v->uc->clip_start,v->uc->clip_end);
			if(clip_store(skel)==0) {
				veejay_msg(VEEJAY_MSG_INFO,"Created new clip [%d]", skel->clip_id);
				clip_set_looptype(skel->clip_id, 1);	
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR,"%s %d: Cannot store new clip!",__FILE__,__LINE__);
			}
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Clip ending position before starting position. Cannot create new clip");
		}
	}
	else
	{
		p_invalid_mode();
	}		
}
 
void vj_event_goto_end(void *ptr, const char format[], va_list ap)
{
  	veejay_t *v = (veejay_t*) ptr;
	if(TAG_PLAYING(v))
	{
		p_invalid_mode();
		return;
	} 
 	if(CLIP_PLAYING(v))
  	{	
		veejay_set_frame(v, clip_get_endFrame(v->uc->clip_id));
  	}
  	if(PLAIN_PLAYING(v)) 
 	{
		veejay_set_frame(v,(v->editlist->video_frames-1));
  	}
}

void vj_event_goto_start(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if(TAG_PLAYING(v))
	{
		p_invalid_mode();
		return;
	}
  	if( CLIP_PLAYING(v))
	{
		veejay_set_frame(v, clip_get_startFrame(v->uc->clip_id));
  	}
  	if ( PLAIN_PLAYING(v))
	{
		veejay_set_frame(v,0);
  	}
}

void vj_event_clip_set_loop_type(void *ptr, const char format[], va_list ap)
{
	int args[2];
	veejay_t *v = (veejay_t*) ptr;
	char *s = NULL;
	P_A(args,s,format,ap);

	if(!CLIP_PLAYING(v)) return;

	if( args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if(args[1] == -1)
	{
		if(clip_exists(args[0]))
		{
			if(clip_get_looptype(args[0])==2)
			{
				int lp;
				clip_set_looptype(args[0],1);
				lp = clip_get_looptype(args[0]);
				veejay_msg(VEEJAY_MSG_INFO, "Clip %d loop type is now %s",args[0],
		  			( lp==1 ? "Normal Looping" : (lp==2 ? "Pingpong Looping" : "No Looping" ) ) );
				return;
			}
			else
			{
				int lp;
				clip_set_looptype(args[0],2);
				lp = clip_get_looptype(args[0]);
				veejay_msg(VEEJAY_MSG_INFO, "Clip %d loop type is now %s",args[0],
		  			( lp==1 ? "Normal Looping" : lp==2 ? "Pingpong Looping" : "No Looping" ) );
				return;
			}
		}
		else
		{
			p_no_clip(args[0]);
			return;
		}
	}

	if(args[1] >= 0 && args[1] <= 2) 
	{
		if(clip_exists(args[0]))
		{
			int lp;
			clip_set_looptype( args[0] , args[1]);
			lp = clip_get_looptype(args[0]);
			veejay_msg(VEEJAY_MSG_INFO, "Clip %d loop type is now %s",args[0],
			  ( args[1]==1 ? "Normal Looping" : lp==2 ? "Pingpong Looping" : "No Looping" ) );
		}
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Clip %d does not exist or invalid looptype %d",args[1],args[0]);
	}
}

void vj_event_clip_set_speed(void *ptr, const char format[], va_list ap)
{
	int args[2];
	veejay_t *v = (veejay_t*) ptr;
	char *s = NULL;
	P_A(args, s, format, ap);

	if(args[0] == -1)
	{
		args[0] = clip_size() - 1;
	}

	if( args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}

	if(clip_exists(args[0]))
	{	
		if( clip_set_speed(args[0], args[1]) != -1)
		{
			veejay_msg(VEEJAY_MSG_INFO, "Clip %d speed set to %d",args[0],args[1]);
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Speed %d it too high to set on clip %d !",
				args[1],args[0]); 
		}
	}
	else
	{
		p_no_clip(args[0]);
	}
}

void vj_event_clip_set_marker_start(void *ptr, const char format[], va_list ap) 
{
	int args[2];
	veejay_t *v = (veejay_t*)ptr;
	video_playback_setup *s = v->settings;
	char *str = NULL;
	P_A(args,str,format,ap);
	
	if( args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}

	if(args[0] == -1) args[0] = clip_size()-1;

	if( clip_exists(args[0]) )
	{
		if ( args[1] == 0 ) args[1] = s->current_frame_num;

		if ( args[1] >= clip_get_startFrame(args[0]) && args[1] <= clip_get_endFrame(args[0]))
		{
			if( clip_set_marker_start( args[0], args[1] ) )
			{
				veejay_msg(VEEJAY_MSG_INFO, "Clip %d marker starting position at %d", args[0],args[1]);
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Cannot set marker position %d for clip %d",args[1],args[0]);
			}
		}
		else 
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Marker position out side of clip boundaries");
		}
	}	
	else 
	{
		p_no_clip(args[0]);
	}
}


void vj_event_clip_set_marker_end(void *ptr, const char format[], va_list ap) 
{
	int args[2];
	veejay_t *v = (veejay_t*) ptr;
	video_playback_setup *s = v->settings;
	char *str = NULL;
	P_A(args,str,format,ap);
	
	if( args[0] == 0 ) 
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if( clip_exists(args[0]) )
	{
		if ( args[1] == 0 ) args[1] = s->current_frame_num;
		if ( args[1] >= clip_get_startFrame(args[0]) && args[1] <= clip_get_endFrame(args[0]))
		{
			if( clip_set_marker_end( args[0], args[1] ) )
			{
				veejay_msg(VEEJAY_MSG_INFO, "Clip %d marker ending position at position %d", args[0],args[1]);
			}
		}
		else
		{
			veejay_msg(VEEJAY_MSG_INFO, "Marker position out side of clip boundaries");
		}
	}	
	else 
	{
		p_no_clip(args[0]);
	}
}


void vj_event_clip_set_marker(void *ptr, const char format[], va_list ap) 
{
	int args[3];
	veejay_t *v = (veejay_t*) ptr;
	char *s = NULL;
	P_A(args,s,format,ap);
	
	if( args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if( clip_exists(args[0]) )
	{
		if( clip_set_marker( args[0], args[1],args[2] ) )
		{
			veejay_msg(VEEJAY_MSG_INFO, "Clip %d marker starting position at %d, ending position at %d", args[0],args[1],args[2]);
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Cannot set marker %d-%d for clip %d",args[1],args[2],args[0]);
		}
	}	
	else
	{
		p_no_clip(args[0]);
	}
}


void vj_event_clip_set_marker_clear(void *ptr, const char format[],va_list ap) 
{
	int args[1];
	veejay_t *v = (veejay_t*) ptr;
	char *s = NULL;
	P_A(args,s,format,ap);
	
	if( args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if( clip_exists(args[0]) )
	{
		if( clip_marker_clear( args[0] ) )
		{
			veejay_msg(VEEJAY_MSG_INFO, "Clip %d marker cleared", args[0]);
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Cannot set marker %d-%d for clip %d",args[1],args[2],args[0]);
		}
	}	
	else
	{
		p_no_clip(args[0]);
	}
	
}

void vj_event_clip_set_dup(void *ptr, const char format[], va_list ap)
{
	int args[2];
	veejay_t *v = (veejay_t*) ptr;
	char *s = NULL;
	P_A(args,s,format,ap);

	if( args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if( clip_exists(args[0])) 
	{
		if( clip_set_framedup( args[0], args[1] ) != -1) 
		{
			veejay_msg(VEEJAY_MSG_INFO, "Clip %d frame duplicator set to %d", args[0],args[1]);
			if( args[0] == v->uc->clip_id)
			{
			    if(veejay_set_framedup(v, args[1]))
               		    {
                       		 veejay_msg(VEEJAY_MSG_INFO,
			"Video frame will be duplicated %d to output",args[1]);
                	    }
			}
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR,"Cannot set frame duplicator to %d for clip %d",args[0],args[1]);
		}
	}
	else
	{
		p_no_clip(args[0]);
	}
}


void vj_event_clip_set_descr(void *ptr, const char format[], va_list ap)
{
	char str[255];
	int args[1];
	veejay_t *v = (veejay_t*) ptr;

	P_A(args,str,format,ap);
	if( args[0] == 0 ) 
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if(clip_set_description(args[0],str) == 0)
	{
		veejay_msg(VEEJAY_MSG_INFO, "Clip %d description [%s]",args[0],str);
	}
}

void vj_event_clip_save_list(void *ptr, const char format[], va_list ap)
{
	char str[255];
	int *args = NULL;
	P_A(args,str,format,ap);
	if(clip_size()-1 < 1) 
	{
		veejay_msg(VEEJAY_MSG_ERROR, "No clips to save");
		return;
	}
	if(clip_writeToFile( str) )
	{
		veejay_msg(VEEJAY_MSG_INFO, "Wrote %d clips to file %s", clip_size()-1, str);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Error saving clips to file %s", str);
	}
}

void vj_event_clip_load_list(void *ptr, const char format[], va_list ap)
{
	char str[255];
	int *args = NULL;
	P_A( args, str, format, ap);

	if( clip_readFromFile( str ) ) 
	{
		veejay_msg(VEEJAY_MSG_INFO, "Loaded clip list [%s]", str);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Error loading clip list [%s]", str);
	}
}


void vj_event_clip_his_play_entry(void *ptr, const char format[], va_list ap)
{
	int args[2];
	veejay_t *v = (veejay_t *)ptr;
	char *str = NULL;
	P_A(args,str,format,ap);
	if(args[0] == 0 )
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if( clip_exists(args[0]))
	{
		if( clip_set_render_entry(args[0],args[1]) )	
		{
			veejay_set_clip(v, args[0]);
			veejay_msg(VEEJAY_MSG_INFO, "Playing render entry %d of clip %d", args[1],args[0]);
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Cannot set render entry to %d for clip %d",args[1],args[0]);
		}
	}
	else
	{
		p_no_clip(args[0]);
	}
}

void vj_event_clip_his_lock_entry(void *ptr, const char format[], va_list ap)
{
	int args[2];
	veejay_t *v=  (veejay_t*)ptr;
	char *s = NULL;
	P_A(args,s,format,ap);
	if(args[0] == 0 )
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if( clip_exists(args[0]))
	{
		if( clip_entry_set_is_rendering( args[0] , args[0] , 1))
		{
			veejay_msg(VEEJAY_MSG_INFO, "Locking render entry %d of clip %d",
				args[1],args[0]);
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Cannot lock render entry %d of clip %d",args[1],args[0]);
		}
	}	
	else 
	{
		p_no_clip(args[0]);
	}
}

void vj_event_clip_his_unlock_entry(void *ptr, const char format[],va_list ap)
{
	int args[2];
	veejay_t *v = (veejay_t*)ptr;
	char *s = NULL;
	P_A(args,s,format,ap);

	if(args[0] == 0 )
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if( clip_exists(args[0])) 
	{
		if(clip_entry_set_is_rendering( args[0],args[1],0))
		{
			veejay_msg(VEEJAY_MSG_INFO, "Unlocking render entry %d of clip %d",
				args[1],args[0]);
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Cannot unlock render entry %d of clip %d",args[1],args[0]);
		}
	}
	else
	{
		p_no_clip(args[0]);
	}
}

void vj_event_clip_his_del_entry(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *s = NULL;
	P_A(args,s,format,ap);
	if(args[0] == 0 )
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if(clip_exists(args[0]))
	{
		if(args[1] >= 0 && args[1] < CLIP_MAX_RENDER) 
		{
			int s,e;
			int ds = clip_get_startFrame(args[0]);
			int de = clip_get_endFrame(args[0]);
			if( clip_set_render_entry(args[0],0) ) 
			{
				s = clip_get_startFrame(args[0]);
				e = clip_get_endFrame(args[0]);

				if( clip_set_render_entry(args[0], args[1]) &&
				    clip_set_startframe(args[0],s) &&
				    clip_set_endframe(args[0],e) )
				{		
			
					if( veejay_edit_delete(v, ds,de) )
					{
						veejay_msg(VEEJAY_MSG_INFO, "Deleted rendered editlist frames from entry %d of clip %d",args[1],args[0]);
						v->uc->render_changed = 1;
					}
					else
					{
						veejay_msg(VEEJAY_MSG_ERROR,"Unable to delete frames %d-%d from EditList",ds,de);
					}	
				}
				else
				{
					veejay_msg(VEEJAY_MSG_ERROR, "Cannnot update clip %d 's render history entry %d with %d - %d",
						args[0],args[1],s,e); 
				}
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Cannot set Clip %d 's render entry to %d", args[0],args[1]);
			}
		}	
	}
	else 
	{
		p_no_clip(args[0]);
	}
}

void vj_event_clip_his_entry_to_new(void *ptr, const char format[],va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *s = NULL;
	P_A(args,s,format,ap);
	
	if(args[0] == 0 )
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;	
	if(clip_exists(args[0]))
	{
		int s,e;
		clip_info *skel;
		clip_set_render_entry(args[0], args[1]);
		
		s   = clip_get_startFrame( args[0] );
		e   = clip_get_endFrame(args[0]);
		if(args[1] == 0)
		{
			veejay_msg(VEEJAY_MSG_INFO, "This clip cannot be moved to a new clip");
				return;
		}

		/* move to original */
		clip_set_render_entry(args[0] , 0);
		skel = clip_skeleton_new( s,e);
		if( clip_store(skel) == 0)
		{
			veejay_msg(VEEJAY_MSG_INFO, "Moved rendered entry to new clip %d", clip_size()-1);
			clip_set_looptype(skel->clip_id, clip_get_looptype(args[0]));
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Cannot store newly created clip !");
		}
		v->uc->render_changed = 1;
	}
	else
	{
		p_no_clip(args[0]);
	}

}

void vj_event_clip_set_freeze_play(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[4];
	char *s = NULL;
	P_A(args,s,format,ap);

	if( args[0] == 0 ) 
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;	
	if( clip_exists(args[0])) 
	{
		if( clip_set_freeze_mode(args[0],args[1],args[2],args[3]) )
		{
			veejay_msg(VEEJAY_MSG_INFO, "Clip %d has freeze mode %d, after %d frames I pause for %d frames",
				args[0],args[1],args[2],args[3]);
		}
		else 
		{
			veejay_msg(VEEJAY_MSG_ERROR,"Cannot set freeze mode %d with %d-%d on Clip %d",
				args[1],args[2],args[3],args[0]);
		}
	}
	else 
	{
		p_no_clip(args[0]);
	}
}



void vj_event_clip_rec_start( void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	int args[2];
	int changed = 0;
	int result = 0;
	char *str = NULL;
	P_A(args,str,format,ap);


	if( CLIP_PLAYING(v) ) 
	{
		char tmp[255];
		sprintf(tmp, "clip-%d-%d-XXXXXX", v->uc->clip_id,args[0]);

		if( args[0] == 0 )
		{
			int n = clip_get_speed(v->uc->clip_id);
			if( n == 0) n = 1; else if (n < 0 ) n = n * -1;
			args[0] = clip_get_longest(v->uc->clip_id);
			//args[0] = (clip_get_endFrame(v->uc->clip_id) - clip_get_startFrame(v->uc->clip_id)) / n;  
			changed = 1;
		}

		if( mkstemp( tmp ) != -1 ) 
		{
			int format = _recorder_format;
			if(format==-1)
				format = el_video_frame_data_format(clip_get_startFrame(v->uc->clip_id),v->editlist);
			// wrong format
			if( clip_init_encoder( v->uc->clip_id, tmp, format, v->editlist, args[0]) == 1)
			{
				video_playback_setup *s = v->settings;
				s->clip_record_id = v->uc->clip_id;
				if(args[1])
				{
					s->clip_record_switch = 1;
				}
				result = 1;
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR,"Unable to initialize clip recorder");
				return;
			}   
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Unable to create temporary file name %s",tmp);
			return;
		}
	}
	else 
	{
		p_invalid_mode();
	}
	if(changed)
	{
		veejay_set_clip(v,v->uc->clip_id);
	}


	if(result == 1)
	{
		if(changed)
		{
			veejay_msg(VEEJAY_MSG_INFO,"Recording editlist frames %d - %d at speed %d  (%d frames and %s)",clip_get_startFrame(v->uc->clip_id),
				clip_get_endFrame(v->uc->clip_id),clip_get_speed(v->uc->clip_id), args[0],
				(args[1]==1 ? "autoplay" : "no autoplay")
			);
		}
		else
		{
			veejay_msg(VEEJAY_MSG_INFO, "Recording editlist frames starting from %d (%d frames and %s)", v->settings->current_frame_num, args[0], (args[1]==1? "autoplay" : "no autoplay"));
		}
		v->settings->clip_record = 1;
		v->settings->clip_record_switch = args[1];
	}

}

void vj_event_clip_rec_stop(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;

	if( CLIP_PLAYING(v)) 
	{
		video_playback_setup *s = v->settings;
		if( clip_stop_encoder( v->uc->clip_id ) == 1 ) 
		{
			char avi_file[255];
			int start = -1;
			int destination = v->editlist->video_frames - 1;

			if( clip_get_encoded_file(v->uc->clip_id, avi_file)!=0) return;
			
			clip_reset_encoder( v->uc->clip_id );

			if( veejay_edit_addmovie(
				v,avi_file,start,destination,destination))
			{
				int len = clip_get_encoded_frames(v->uc->clip_id) - 1;
				clip_info *skel = clip_skeleton_new(destination, destination+len);		
				if(clip_store(skel)==0) 
				{
					veejay_msg(VEEJAY_MSG_INFO, "Created new clip %d from file %s",
						skel->clip_id,avi_file);
					clip_set_looptype( skel->clip_id,1);
				}	
			}		
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Cannot add videofile %s to EditList!",avi_file);
			}

			clip_reset_encoder( v->uc->clip_id);
			s->clip_record = 0;	
			s->clip_record_id = 0;
			if(s->clip_record_switch) 
			{
				veejay_set_clip( v, clip_size()-1);
				s->clip_record_switch = 0;
				veejay_msg(VEEJAY_MSG_INFO, "Switching to clip %d (recording)", clip_size()-1);
			}
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Clip recorder was never started for clip %d",v->uc->clip_id);
		}
		
	}
	else 
	{
		p_invalid_mode();
	}
}


void vj_event_clip_set_num_loops(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t *)ptr;
	int args[2];
	char *s = NULL;
	P_A(args,s,format,ap);

	if(args[0] == 0) args[0] = v->uc->clip_id;
	if(args[0] == -1) args[0] = clip_size()-1;

	if(clip_exists(args[0]))
	{

		if(	clip_set_loops(v->uc->clip_id, args[1]))
		{	veejay_msg(VEEJAY_MSG_INFO, "Setted %d no. of loops for clip %d",
				clip_get_loops(v->uc->clip_id),args[0]);
		}
		else	
		{
			veejay_msg(VEEJAY_MSG_ERROR,"Cannot set %d loops for clip %d",args[1],args[0]);
		}

	}
	else
	{
		p_no_clip(args[0]);
	}
}


void vj_event_clip_rel_start(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	int args[4];
	//video_playback_setup *s = v->settings;
	char *str = NULL;
	int s_start;
	int s_end;

	P_A(args,str,format,ap);
	if(CLIP_PLAYING(v))
	{

		if(args[0] == 0)
		{
			args[0] = v->uc->clip_id;
		}
	if(args[0] == -1) args[0] = clip_size()-1;	
		if(!clip_exists(args[0]))
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Clip does not exist");
			return;
		}
		
		s_start = clip_get_startFrame(args[0]) + args[1];
		s_end = clip_get_endFrame(args[0]) + args[2];

		if(s_end > v->editlist->video_frames-1) s_end = v->editlist->video_frames - 1;

		if( s_start >= 1 && s_end <= (v->editlist->video_frames-1) )
		{ 
			if	(clip_set_startframe(args[0],s_start) &&
				clip_set_endframe(args[0],s_end))
			{
				veejay_msg(VEEJAY_MSG_INFO, "Clip update start %d end %d",
					s_start,s_end);
			}
	
		}
	}
	else
	{
		veejay_msg(VEEJAY_MSG_INFO, "Invalid playmode");
	}

}

void vj_event_clip_set_start(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t *)ptr;
	int args[2];
	video_playback_setup *s = v->settings;
	char *str = NULL;
	P_A(args,str,format,ap);
	if(args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;	
	if( (args[1] >= s->min_frame_num) && (args[1] <= (v->editlist->video_frames-1)) && clip_exists(args[0])) 
	{
	  if( args[1] < clip_get_endFrame(args[0])) {
		if( clip_set_startframe(args[0],args[1] ) ) {
			veejay_msg(VEEJAY_MSG_INFO, "Clip starting frame updated to frame %d",
			  clip_get_startFrame(args[0]));
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Unable to update clip %d 's starting position to %d",args[0],args[1]);
		}
	  }
	  else 
	  {
		veejay_msg(VEEJAY_MSG_ERROR, "Clip %d 's starting position %d must be greater than ending position %d.",
			args[0],args[1], clip_get_endFrame(args[0]));
	  }
	}
	else
	{
		if(!clip_exists(args[0])) p_no_clip(args[0]) else veejay_msg(VEEJAY_MSG_ERROR, "Invalid position %d given",args[1]);
	}
}

void vj_event_clip_set_end(void *ptr, const char format[] , va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	int args[2];
	video_playback_setup *s = v->settings;
	char *str = NULL;
	P_A(args,str,format,ap);
	if(args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}
	if(args[1] == -1)
	{
		args[1] = v->editlist->video_frames-1;
	}
	if( (args[1] >= s->min_frame_num) && (args[1] <= (v->editlist->video_frames-1)) && (clip_exists(args[0])))
	{
		if( args[1] >= clip_get_startFrame(v->uc->clip_id)) {
	       		if(clip_set_endframe(args[0],args[1])) {
	   			veejay_msg(VEEJAY_MSG_INFO,"Clip ending frame updated to frame %d",
	        		clip_get_endFrame(args[0]));
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Unable to update clip %d 's ending position to %d",
					args[0],args[1]);
			}
	      	}
		else 
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Clip %d 's ending position %d must be greater than or equal to starting position %d.",
				args[0],args[1], clip_get_startFrame(v->uc->clip_id));
		}
	}
	else
	{
		if(!clip_exists(args[0])) p_no_clip(args[0]) else veejay_msg(VEEJAY_MSG_ERROR, "Invalid position %d given",args[1]);
	}
	
}

void vj_event_clip_del(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *s = NULL;
	P_A(args,s,format,ap);

	if(CLIP_PLAYING(v)) 
	{
		if(v->uc->clip_id == args[0])
		{
			veejay_msg(VEEJAY_MSG_INFO,"Cannot delete clip while playing");
		}
		else
		{
			if(clip_del(args[0]))
			{
				veejay_msg(VEEJAY_MSG_INFO, "Deleted clip %d", args[0]);
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Unable to delete clip %d",args[0]);
			}
		}	
	}
	else
	{
		p_invalid_mode();
	}
}

void vj_event_clip_copy(void *ptr, const char format[] , va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *s = NULL;
	P_A(args,s,format,ap);

	if(CLIP_PLAYING(v)) 
	{
		int new_clip = clip_copy(args[0]);
		if(new_clip)
		{
			veejay_msg(VEEJAY_MSG_INFO, "Copied clip %d to %d",args[0],new_clip);
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Failed to copy clip %d.",args[0]);
		}
	}
	else
	{
		p_invalid_mode();
	}
}

void vj_event_clip_clear_all(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if( !CLIP_PLAYING(v)) 
	{
		clip_del_all();
	}
	else
	{
		p_invalid_mode();
	}
} 



void vj_event_chain_enable(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	if(CLIP_PLAYING(v))
	{
		clip_set_effect_status(v->uc->clip_id, 1);
	}
	if(TAG_PLAYING(v))
	{
		vj_tag_set_effect_status(v->uc->clip_id, 1);
	}
	veejay_msg(VEEJAY_MSG_INFO, "Effect chain is enabled");
	
}


void vj_event_chain_disable(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	if(CLIP_PLAYING(v) )
	{
		clip_set_effect_status(v->uc->clip_id, 0);
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain on Clip %d is disabled",v->uc->clip_id);
	}
	if(TAG_PLAYING(v) )
	{
		vj_tag_set_effect_status(v->uc->clip_id, 0);
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain on Stream %d is enabled",v->uc->clip_id);
	}

}

void vj_event_clip_chain_enable(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	int args[4];
	char *s = NULL;
	P_A(args,s,format,ap);
	if(args[0] == 0)
	{
		args[0] = v->uc->clip_id;
	}
	
	if(CLIP_PLAYING(v) && clip_exists(args[0]))
	{
		clip_set_effect_status(args[0], 1);
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain on Clip %d is enabled",args[0]);
	}
	
}

void	vj_event_all_clips_chain_toggle(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *s = NULL;
	P_A(args,s,format,ap);
	if(CLIP_PLAYING(v))
	{
		int i;
		for(i=0; i < clip_size()-1; i++)
		{
			clip_set_effect_status( i, args[0] );
		}
		veejay_msg(VEEJAY_MSG_INFO, "Effect Chain on all clips %s", (args[0]==0 ? "Disabled" : "Enabled"));
	}
	if(TAG_PLAYING(v))
	{
		int i;
		for(i=0; i < vj_tag_size()-1; i++)
		{
			vj_tag_set_effect_status(i,args[0]);
		}
		veejay_msg(VEEJAY_MSG_INFO, "Effect Chain on all streams %s", (args[0]==0 ? "Disabled" : "Enabled"));
	}
}


void vj_event_tag_chain_enable(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[4];
	char *s = NULL;
	P_A(args,s,format,ap);

	if(TAG_PLAYING(v) && vj_tag_exists(args[0]))
	{
		vj_tag_set_effect_status(args[0], 1);
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain on stream %d is enabled",args[0]);
	}

}
void vj_event_tag_chain_disable(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *s = NULL;
	P_A(args,s,format,ap);

	if(TAG_PLAYING(v) && vj_tag_exists(args[0]))
	{
		vj_tag_set_effect_status(args[0], 0);
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain on stream %d is disabled",args[0]);
	}

}

void vj_event_clip_chain_disable(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *s = NULL;
	P_A(args,s,format,ap);
	if(args[0] == 0)
	{
		args[0] = v->uc->clip_id;
	}
	
	if(CLIP_PLAYING(v) && clip_exists(args[0]))
	{
		clip_set_effect_status(args[0], 0);
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain on stream %d is disabled",args[0]);
	}
	if(TAG_PLAYING(v) && vj_tag_exists(args[0]))
	{
		vj_tag_set_effect_status(args[0], 0);
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain on stream %d is disabled",args[0]);
	}
	
}


void vj_event_chain_toggle(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	if(CLIP_PLAYING(v))
	{
		int flag = clip_get_effect_status(v->uc->clip_id);
		if(flag == 0) 
		{
			clip_set_effect_status(v->uc->clip_id,1); 
		}
		else
		{
			clip_set_effect_status(v->uc->clip_id,0);
		}
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain is %s.", (clip_get_effect_status(v->uc->clip_id) ? "enabled" : "disabled"));
	}
	if(TAG_PLAYING(v))
	{
		int flag = vj_tag_get_effect_status(v->uc->clip_id);
		if(flag == 0) 	
		{
			vj_tag_set_effect_status(v->uc->clip_id,1); 
		}
		else
		{
			vj_tag_set_effect_status(v->uc->clip_id,0);
		}
		veejay_msg(VEEJAY_MSG_INFO, "Effect chain is %s.", (vj_tag_get_effect_status(v->uc->clip_id) ? "enabled" : "disabled"));
	}
}	

void vj_event_chain_entry_video_toggle(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	if(CLIP_PLAYING(v))
	{
		int c = clip_get_selected_entry(v->uc->clip_id);
		int flag = clip_get_chain_status(v->uc->clip_id,c);
		if(flag == 0)
		{
			clip_set_chain_status(v->uc->clip_id, c,1);	
		}
		else
		{	
			clip_set_chain_status(v->uc->clip_id, c,0);
		}
		veejay_msg(VEEJAY_MSG_INFO, "Video on chain entry %d is %s", clip_get_chain_status(v->uc->clip_id,c),
			(flag==0 ? "Disabled" : "Enabled"));
	}
	if(TAG_PLAYING(v))
	{
		int c = vj_tag_get_selected_entry(v->uc->clip_id);
		int flag = vj_tag_get_chain_status( v->uc->clip_id,c);
		if(flag == 0)
		{
			vj_tag_set_chain_status(v->uc->clip_id, c,1);	
		}
		else
		{	
			vj_tag_set_chain_status(v->uc->clip_id, c,0);
		}
		veejay_msg(VEEJAY_MSG_INFO, "Video on chain entry %d is %s", vj_tag_get_chain_status(v->uc->clip_id,c),
			(flag==0 ? "Disabled" : "Enabled"));

	}
}

void vj_event_chain_entry_enable_video(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];	
	char *s = NULL;
	P_A(args,s,format,ap);

	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(clip_exists(args[0]))
		{
			if(clip_set_chain_status(args[0],args[1],1) != -1)	
			{
				veejay_msg(VEEJAY_MSG_INFO, "Clip %d: Video on chain entry %d is %s",args[0],args[1],
					( clip_get_chain_status(args[0],args[1]) == 1 ? "Enabled" : "Disabled"));
			}
		}
	}
	if(TAG_PLAYING(v))
	{
		if(args[0] == 0)args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
		if(vj_tag_exists(args[0])) 
		{
			if(vj_tag_set_chain_status(args[0],args[1],1)!=-1)
			{
				veejay_msg(VEEJAY_MSG_INFO, "Stream %d: Video on chain entry %d is %s",args[0],args[1],
					vj_tag_get_chain_status(args[0],args[1]) == 1 ? "Enabled" : "Disabled" );
			}
		}
	}
}
void vj_event_chain_entry_disable_video(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str,format,ap);
	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(clip_exists(args[0]))
		{
			if(clip_set_chain_status(args[0],args[1],0)!=-1)
			{	
				veejay_msg(VEEJAY_MSG_INFO, "Clip %d: Video on chain entry %d is %s",args[0],args[1],
				( clip_get_chain_status(args[0],args[1])==1 ? "Enabled" : "Disabled"));
			}
		}
	}
	if(TAG_PLAYING(v))
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;	
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
		if(vj_tag_exists(args[0]))
		{
			if(vj_tag_set_chain_status(args[0],args[1],0)!=-1)
			{
				veejay_msg(VEEJAY_MSG_INFO, "Stream %d: Video on chain entry %d is %s",args[0],args[1],
					vj_tag_get_chain_status(args[0],args[1]) == 1 ? "Enabled" : "Disabled" );
			}
		}
	}

}
void vj_event_chain_entry_enable_audio(void *ptr, const char format[], va_list ap)
{
	veejay_msg(VEEJAY_MSG_ERROR,"TODO - No chaining of Audio yet");
/*
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str,format,ap);

	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(clip_exists(args[0]))
		{
			if(clip_set_chain_audio(args[0],args[1],1) != -1)
			{
				veejay_msg(VEEJAY_MSG_INFO, "Clip %d: Audio on chain entry %d is %s",args[0],args[1],
					( clip_get_chain_audio(args[0],args[1]) == 1? "Enabled" : "Disabled"));
			}
		}
	}
	if(TAG_PLAYING(v))
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
		if(vj_tag_exists(args[0]))
		{
			veejay_msg(VEEJAY_MSG_INFO, "Stream: mixing with audio not yet supported");
		}
	}
*/
}

void vj_event_chain_entry_disable_audio(void *ptr, const char format[], va_list ap)
{
	veejay_msg(VEEJAY_MSG_ERROR, "TODO - No chaining of Audio yet");
/*
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(clip_exists(args[0]))
		{
			if(clip_set_chain_audio(args[0],args[1],0) != -1)
			{
				veejay_msg(VEEJAY_MSG_INFO, "Stream %d: Audio on chain entry %d is %s",args[0],args[1],
					( clip_get_chain_audio(args[0],args[1]) == 1? "Enabled" : "Disabled"));
			}
		}
	}
	if(TAG_PLAYING(v))
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
		if(vj_tag_exists(args[0]))
		{
			veejay_msg(VEEJAY_MSG_INFO, "Stream: mixing with audio not yet supported");
		}
	}
*/
}

void vj_event_chain_entry_audio_toggle(void *ptr, const char format[], va_list ap) 
{
	veejay_msg(VEEJAY_MSG_ERROR, "TODO - No Chaining of Audio yet");
/*
	veejay_t *v = (veejay_t*)ptr;

	if(CLIP_PLAYING(v)) 
	{
		int c = clip_get_selected_entry(v->uc->clip_id);
		clip_set_chain_audio(v->uc->clip_id, c, !(clip_get_chain_audio(v->uc->clip_id,c)));
		veejay_msg(VEEJAY_MSG_INFO, "%s audio mixing with chain entry %d",	
			(clip_get_chain_audio(v->uc->clip_id,c) == 1 ? "Enabled" : "Disabled"),
			c); 
	}
*/
}


void vj_event_chain_entry_audio_volume(void *ptr, const char format[], va_list ap)
{
	veejay_msg(VEEJAY_MSG_ERROR, "TODO - No chaining of Audio yet");
/*

	veejay_t *v = (veejay_t*)ptr;
	int args[3];
	char *str = NULL; P_A(args,str,format,ap);
	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(clip_exists(args[0]))
		{
			clip_set_chain_volume(args[0],args[1], args[2]);
			veejay_msg(VEEJAY_MSG_INFO, "Clip %d, Setted volume to %d on entry %d",args[0],args[2],args[1]);
		}
	}
	if(TAG_PLAYING(v) && vj_tag_exists(args[0]))
	{
		veejay_msg(VEEJAY_MSG_INFO, "Stream: Setting of audio volume not yet supported");
	}
*/
}



void vj_event_chain_entry_audio_vol_inc(void *ptr, const char format[], va_list ap) 
{
	int args[1];
	veejay_t *v = (veejay_t*)ptr;

	char *str = NULL; P_A(args,str,format,ap);
	if(CLIP_PLAYING(v))
	{
		int c = clip_get_selected_entry(v->uc->clip_id);
		int vol = clip_get_chain_volume(v->uc->clip_id,c);
		vol += args[0];
		clip_set_chain_volume(v->uc->clip_id,c, vol);
		veejay_msg(VEEJAY_MSG_INFO, "Setted volume to %d on entry %d",vol,c);
	}
	if(TAG_PLAYING(v))
	{
		veejay_msg(VEEJAY_MSG_INFO, "Stream: Increment audio volume not yet supported");
	}
}

void vj_event_chain_fade_in(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if(args[0] == 0 && (CLIP_PLAYING(v) || TAG_PLAYING(v)) )
	{
		args[0] = v->uc->clip_id;
	}

	if( CLIP_PLAYING(v) && clip_exists(args[0])) 
	{
		if( clip_set_fader_active( args[0], args[1],0 ) )
		{
			veejay_msg(VEEJAY_MSG_INFO, "Chain Fade in from clip to full effect chain in %d frames. Per frame %2.2f",
				args[1], clip_get_fader_inc(args[0]));
			if(clip_get_effect_status(args[0]==0))
			{
				clip_set_effect_status(args[0], 1);
			}
		}
	}
	if (TAG_PLAYING(v) && vj_tag_exists(args[0])) 
	{
		if( vj_tag_set_fader_active( args[0], args[1],0 ) )
		{
			veejay_msg(VEEJAY_MSG_INFO,"Chain Fade in from stream to full effect chain in %d frames. Per frame %2.2f",
				args[1], clip_get_fader_inc(args[0]));
			if(vj_tag_get_effect_status(args[0]==0))
			{
				vj_tag_set_effect_status(args[0],1);
			}
		}
	}

}


void vj_event_chain_fade_out(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if(args[0] == 0 && (CLIP_PLAYING(v) || TAG_PLAYING(v)) )
	{
		args[0] = v->uc->clip_id;
	}

	if( CLIP_PLAYING(v) && clip_exists(args[0])) 
	{
		if( clip_set_fader_active( args[0], args[1],1 ) )
		{
			veejay_msg(VEEJAY_MSG_INFO, "Chain Fade Out from clip to full effect chain in %d frames. Per frame %2.2f",
				args[1], clip_get_fader_inc(args[0]));
		}
	}
	if (TAG_PLAYING(v) && vj_tag_exists(args[0])) 
	{
		if( vj_tag_set_fader_active( args[0], args[1],1 ) )
		{
			veejay_msg(VEEJAY_MSG_INFO,"Chain Fade Out from stream to full effect chain in %d frames. Per frame %2.2f",
				args[1], clip_get_fader_inc(args[0]));
		}
	}
}



void vj_event_chain_clear(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[1];
	char *str = NULL; 
	P_A(args,str,format,ap);

	if(args[0] == 0 && (CLIP_PLAYING(v) || TAG_PLAYING(v)) )
	{
		args[0] = v->uc->clip_id;
	}

	if( CLIP_PLAYING(v) && clip_exists(args[0])) 
	{
		int i;
		for(i=0; i < CLIP_MAX_EFFECTS;i++)
		{
			int effect = clip_get_effect_any(args[0],i);
			if(vj_effect_is_valid(effect))
			{
				clip_chain_remove(args[0],i);
				veejay_msg(VEEJAY_MSG_INFO,"Clip %d: Deleted effect %s from entry %d",
					args[0],vj_effect_get_description(effect), i);
			}
		}
		v->uc->chain_changed = 1;
	}
	if (TAG_PLAYING(v) && vj_tag_exists(args[0])) 
	{
		int i;
		for(i=0; i < CLIP_MAX_EFFECTS;i++)
		{
			int effect = vj_tag_get_effect_any(args[0],i);
			if(vj_effect_is_valid(effect))
			{
				vj_tag_chain_remove(args[0],i);
				veejay_msg(VEEJAY_MSG_INFO,"Stream %d: Deleted effect %s from entry %d",	
					args[0],vj_effect_get_description(effect), i);
			}
		}
		v->uc->chain_changed = 1;
	}


}

void vj_event_chain_entry_del(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(clip_exists(args[0]))
		{
			int effect = clip_get_effect_any(args[0],args[1]);
			if( vj_effect_is_valid(effect)) 
			{
				clip_chain_remove(args[0],args[1]);
				v->uc->chain_changed = 1;
				veejay_msg(VEEJAY_MSG_INFO,"Clip %d: Deleted effect %s from entry %d",
					args[0],vj_effect_get_description(effect), args[1]);
			}
		}
	}

	if (TAG_PLAYING(v))
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
		if(vj_tag_exists(args[0])) 		
		{
			int effect = vj_tag_get_effect_any(args[0],args[1]);
			if(vj_effect_is_valid(effect))
			{
				vj_tag_chain_remove(args[0],args[1]);
				v->uc->chain_changed = 1;
				veejay_msg(VEEJAY_MSG_INFO,"Stream %d: Deleted effect %s from entry %d",	
					args[0],vj_effect_get_description(effect), args[1]);
			}
		}
	}
}

void vj_event_chain_entry_set_defaults(void *ptr, const char format[], va_list ap)
{

}

void vj_event_chain_entry_set(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[3];
	char *str = NULL; P_A(args,str,format,ap);

	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[0] == -1) args[0] = clip_size()-1;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(clip_exists(args[0]))
		{
			int real_id = vj_effect_get_real_id(args[2]);
			if(clip_chain_add(args[0],args[1],real_id) != -1) 
			{
				//veejay_msg(VEEJAY_MSG_DEBUG, "Clip %d chain entry %d has effect %s",
				//	args[0],args[1],vj_effect_get_description(real_id));
				v->uc->chain_changed = 1;
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Cannot set effect %d on clip %d chain %d",args[2],args[0],args[1]);
			}
		}
	}
	if( TAG_PLAYING(v)) 
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[0] == -1) args[0] = vj_tag_size()-1;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);	
		if(vj_tag_exists(args[0]))
		{
			int real_id = vj_effect_get_real_id(args[2]);
			if(vj_tag_set_effect(args[0],args[1], real_id) != -1)
			{
			//	veejay_msg(VEEJAY_MSG_INFO, "Stream %d chain entry %d has effect %s",
			//		args[0],args[1],vj_effect_get_description(real_id));
				v->uc->chain_changed = 1;
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Cannot set effect %d on stream %d chain %d",args[2],args[0],args[1]);
			}
		}
	}
}

void vj_event_chain_entry_select(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *str = NULL; P_A(args,str,format,ap);

	if( CLIP_PLAYING(v)  )
	{
		if(args[0] >= 0 && args[0] < CLIP_MAX_EFFECTS)
		{
			if( clip_set_selected_entry( v->uc->clip_id, args[0])) 
			{
			veejay_msg(VEEJAY_MSG_INFO,"Selected entry %d [%s]",
			  clip_get_selected_entry(v->uc->clip_id), 
			  vj_effect_get_description( 
				clip_get_effect_any(v->uc->clip_id,clip_get_selected_entry(v->uc->clip_id))));
			}
		}
	}
	if ( TAG_PLAYING(v))
	{
		if(args[0] >= 0 && args[0] < CLIP_MAX_EFFECTS)
		{
			if( vj_tag_set_selected_entry(v->uc->clip_id,args[0]))
			{
				veejay_msg(VEEJAY_MSG_INFO, "Selected entry %d [%s]",
			 	vj_tag_get_selected_entry(v->uc->clip_id),
				vj_effect_get_description( 
					vj_tag_get_effect_any(v->uc->clip_id,vj_tag_get_selected_entry(v->uc->clip_id))));
			}
		}	
	}
}

void vj_event_entry_up(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if(CLIP_PLAYING(v) || TAG_PLAYING(v))
	{
		int effect_id=-1;
		int c=-1;
		if(CLIP_PLAYING(v))
		{
			c = clip_get_selected_entry(v->uc->clip_id) + 1;
			if(c >= CLIP_MAX_EFFECTS) c = 0;
			clip_set_selected_entry( v->uc->clip_id, c);
			effect_id = clip_get_effect_any(v->uc->clip_id, c );
		}
		if(TAG_PLAYING(v))
		{
			c = vj_tag_get_selected_entry(v->uc->clip_id)+1;
			if( c>= CLIP_MAX_EFFECTS) c = 0;
			vj_tag_set_selected_entry(v->uc->clip_id,c);
			effect_id = vj_tag_get_effect_any(v->uc->clip_id,c);
		}
		veejay_msg(VEEJAY_MSG_INFO, "Selected entry %d with Effect %s",c,
			vj_effect_get_description(effect_id));
	}
}
void vj_event_entry_down(void *ptr, const char format[] ,va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if(CLIP_PLAYING(v) || TAG_PLAYING(v)) 
	{
		int effect_id=-1;
		int c = -1;
		
		if(CLIP_PLAYING(v))
		{
			c = clip_get_selected_entry( v->uc->clip_id ) - 1;
			if(c < 0) c = CLIP_MAX_EFFECTS-1;
			clip_set_selected_entry( v->uc->clip_id, c);
			effect_id = clip_get_effect_any(v->uc->clip_id, c );
		}
		if(TAG_PLAYING(v))
		{
			c = vj_tag_get_selected_entry(v->uc->clip_id) -1;
			if(c<0) c= CLIP_MAX_EFFECTS-1;
			vj_tag_set_selected_entry(v->uc->clip_id,c);
			effect_id = vj_tag_get_effect_any(v->uc->clip_id,c);
		}

		veejay_msg(VEEJAY_MSG_INFO,"Selected entry %d with Effect %s",c,
			vj_effect_get_description(effect_id));
	}
}

void vj_event_chain_entry_preset(void *ptr,const char format[], va_list ap)
{
	int args[13];
	veejay_t *v = (veejay_t*)ptr;
	char *str = NULL; P_A(args,str,format,ap);
//        DUMP_ARG(args);

	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(clip_exists(args[0]))
		{
			int real_id = args[2];
			int num_p   = vj_effect_get_num_params(real_id);
			int i;
			if(clip_chain_add( args[0],args[1],args[2])!=-1)
			{
				veejay_msg(VEEJAY_MSG_DEBUG, "Clip %d Chain entry %d has effect %s",
					args[0],args[1],vj_effect_get_description(real_id));
				int args_offset = 3;
				for(i=0; i < num_p; i++)
				{	
					if(vj_effect_valid_value(real_id,i,args[(i+args_offset)]) )
					{
				 		if(clip_set_effect_arg(args[0],args[1],i,args[(i+args_offset)] )==-1)	
						{
							veejay_msg(VEEJAY_MSG_ERROR,
							"Error setting effect %s",
							vj_effect_get_description(real_id));
							return;				
						}
					}
					else
					{
						veejay_msg(VEEJAY_MSG_ERROR, "Parameter %d value %d is invalid for effect %d (%d-%d)",
							i,args[(i+args_offset)], real_id,
							vj_effect_get_min_limit(real_id,i),
							vj_effect_get_max_limit(real_id,i)
							);
						return;
					}
				}
			}
		}
	}
	if( TAG_PLAYING(v)) 
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
		if(vj_tag_exists(v->uc->clip_id)) 
		{
			int real_id = args[2];
			int num_p   = vj_effect_get_num_params(real_id);
			int i;
		
			if(vj_tag_set_effect(args[0],args[1], args[2]) != -1)
			{
				//veejay_msg(VEEJAY_MSG_INFO, "Clip %d Chain entry %d has effect %s",
				//	args[0], args[1], vj_effect_get_description(real_id));
				for(i=0; i < num_p; i++) 
				{
					if(vj_effect_valid_value(real_id, i, args[i+3]) )
					{
						if(vj_tag_set_effect_arg(args[0],args[1],i,args[i+3]) == -1)
						{
							veejay_msg(VEEJAY_MSG_ERROR, "Error setting effect %s",
								vj_effect_get_description(real_id));
							return;
						}
					}
					else
					{
						veejay_msg(VEEJAY_MSG_ERROR, "Parameter %d value %d is invalid for effect %d (%d-%d)",
							i,args[(i+3)], real_id,
							vj_effect_get_min_limit(real_id,i),
							vj_effect_get_max_limit(real_id,i));
						return;
	
					}
				}
			}
		}
	}

}

void vj_event_chain_entry_src_toggle(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	if(CLIP_PLAYING(v))
	{
		int entry = clip_get_selected_entry(v->uc->clip_id);
		int src = clip_get_chain_source(v->uc->clip_id, entry);
		int cha = clip_get_chain_channel( v->uc->clip_id, entry );
		if(src == 0 ) // source is clip, toggle to stream
		{
			if(!vj_tag_exists(cha))
			{
				cha =vj_tag_size()-1;
				if(cha <= 0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No streams to mix with");
					return;
				}
			}
			veejay_msg(VEEJAY_MSG_DEBUG, "Switched from source Clip to Stream");
			src = vj_tag_get_type(cha);
		}
		else
		{
			src = 0; // source is stream, toggle to clip
			if(!clip_exists(cha))
			{
				cha = clip_size()-1;
				if(cha<=0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No clips to mix with");
					return;
				}
			}
			veejay_msg(VEEJAY_MSG_DEBUG, "Switched from source Stream to Clip");
		}
		clip_set_chain_source( v->uc->clip_id, entry, src );
		clip_set_chain_channel(v->uc->clip_id,entry,cha);
		veejay_msg(VEEJAY_MSG_INFO, "Chain entry %d uses %s %d", entry,(src==VJ_TAG_TYPE_NONE ? "Clip":"Stream"), cha);
		if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);
	} 

	if(TAG_PLAYING(v))
	{
		int entry = vj_tag_get_selected_entry(v->uc->clip_id);
		int src = vj_tag_get_chain_source(v->uc->clip_id, entry);
		int cha = vj_tag_get_chain_channel( v->uc->clip_id, entry );
		char description[100];

		if(src == VJ_TAG_TYPE_NONE ) 
		{
			if(!vj_tag_exists(cha))
			{
				cha = vj_tag_size()-1;
				if(cha <= 0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No streams to mix with");
					return;
				}
			}
			src = vj_tag_get_type(cha);
		}
		else
		{
			src = 0;
			if(!clip_exists(cha))
			{
				cha = clip_size()-1;
				if(cha<=0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No clips to mix with");
					return;
				}
			}
		}
		vj_tag_set_chain_source( v->uc->clip_id, entry, src );
		vj_tag_set_chain_channel(v->uc->clip_id,entry,cha);
		if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

		vj_tag_get_description(cha, description);
		veejay_msg(VEEJAY_MSG_INFO, "Chain entry %d uses channel %d (%s)", entry, cha,description);
	} 
}

void vj_event_chain_entry_source(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[3];
	char *str = NULL; P_A(args,str,format,ap);
	
	

	//DUMP_ARG(args);
	if(args[1] != -1)
	if(!(args[1] >= 0 && args[1]<= CLIP_MAX_EFFECTS))
	{//woops.. forget this one. FIXME (all events args[1] chain entry)
		return;
	}
	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(clip_exists(args[0]))
		{
			int src = args[2];
			int c = clip_get_chain_channel(args[0],args[1]);
			if(src == VJ_TAG_TYPE_NONE)
			{
				if(!clip_exists(c))
				{
					c = clip_size()-1;
					if(c<=0)
					{
						veejay_msg(VEEJAY_MSG_ERROR, "You should create a clip first\n");
						return;
					}
				}
			}
			else
			{
				if(!vj_tag_exists(c) )
				{
					c = vj_tag_size() - 1;
					if(c<=0)
					{
						veejay_msg(VEEJAY_MSG_ERROR, "You should create a stream first (there are none)");
						return;
					}
					src = vj_tag_get_type(c);
				}
			}

			if(c > 0)
			{
			   clip_set_chain_channel(args[0],args[1], c);
			   clip_set_chain_source (args[0],args[1],src);

				veejay_msg(VEEJAY_MSG_INFO, "Mixing with source %s (%d), Channel %d", 
					clip_get_chain_source(args[0],args[1])==VJ_TAG_TYPE_NONE ?"Clip" : "Stream",src,c);
				if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

			}
				
		}
	}
	if(TAG_PLAYING(v))
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);

		if(vj_tag_exists(args[0]))
		{
			int src = args[2];
			int c = vj_tag_get_chain_channel(args[0],args[1]);

			if(src == VJ_TAG_TYPE_NONE)
			{
				if(!clip_exists(c))
				{
					c = clip_size()-1;
					if(c<=0)
					{
						veejay_msg(VEEJAY_MSG_ERROR, "You should create a clip first\n");
						return;
					}
				}
			}
			else
			{
				if(!vj_tag_exists(c) )
				{
					c = vj_tag_size() - 1;
					if(c<=0)
					{
						veejay_msg(VEEJAY_MSG_ERROR, "You should create a stream first (there are none)");
						return;
					}
					src = vj_tag_get_type(c);
				}
			}

			if(c > 0)
			{
			   vj_tag_set_chain_channel(args[0],args[1], c);
			   vj_tag_set_chain_source (args[0],args[1],src);
				veejay_msg(VEEJAY_MSG_INFO, "Mixing with source %s (%d), Channel %d", 
					vj_tag_get_chain_source(args[0],args[1])==VJ_TAG_TYPE_NONE ?"Clip" : "Stream",src,c);
			if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

			}	
		}
	}
}



void	vj_event_chain_entry_efk_set	(	void *ptr,	const char format[],	va_list ap	)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[5];
	char *str = NULL;
	P_A(args,str,format,ap);
	if(CLIP_PLAYING(v))
	{
		if(args[0]==0) args[0] = v->uc->clip_id;
		if(args[0]==-1) args[0] = clip_size()-1;
		
		if(clip_exists(args[0]))
		{
			
			if(args[1]==-1) clip_get_selected_entry(args[0]);	
			if(clip_set_effect_key_frame(args[0],args[1],args[2],args[3],args[4])==1)
		        {
				veejay_msg(VEEJAY_MSG_INFO, "Automatic Parameter %d of Effect on entry %d set, duration is %d , end value %d",
					args[2],args[1],args[3],args[4]);
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Failed to set automatic parameter");
			}

		}
	}

}
void	vj_event_chain_entry_efk_del	(	void *ptr,	const char format[],	va_list ap	)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[4];
	char *str = NULL;
	P_A(args,str,format,ap);
	if(CLIP_PLAYING(v))
	{
		if(args[0]==0) args[0] = v->uc->clip_id;
		if(args[0]==-1) args[0] = clip_size()-1;
		
		if(clip_exists(args[0]))
		{
			
			if(args[1]==-1) clip_get_selected_entry(args[0]);	
			if(clip_clear_effect_key_frame(args[0],args[1],args[2])==1)
		        {
				veejay_msg(VEEJAY_MSG_INFO, "Automatic Parameter %d of Effect on entry %d clear",
					args[2],args[1]);
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Failed to set automatic parameter");
			}

		}
	}


}
void	vj_event_chain_entry_efk_enable  (	void *ptr,	const char format[],	va_list ap	)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[4];
	char *str = NULL;
	P_A(args,str,format,ap);
	if(CLIP_PLAYING(v))
	{
		if(args[0]==0) args[0] = v->uc->clip_id;
		if(args[0]==-1) args[0] = clip_size()-1;
		
		if(clip_exists(args[0]))
		{
			
			if(args[1]==-1) clip_get_selected_entry(args[0]);	
			if(clip_enable_effect_key_frame(args[0],args[1],args[2])==1)
		        {
				veejay_msg(VEEJAY_MSG_INFO, "Automatic Parameter %d of Effect on entry %d enabled",
					args[2],args[1]);
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Failed to set automatic parameter");
			}

		}
	}

}
void	vj_event_chain_entry_efk_disable (	void *ptr,	const char format[],	va_list ap	)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[4];
	char *str = NULL;
	P_A(args,str,format,ap);
	if(CLIP_PLAYING(v))
	{
		if(args[0]==0) args[0] = v->uc->clip_id;
		if(args[0]==-1) args[0] = clip_size()-1;
		
		if(clip_exists(args[0]))
		{
			
			if(args[1]==-1) clip_get_selected_entry(args[0]);	
			if(clip_disable_effect_key_frame(args[0],args[1],args[2])==1)
		        {
				veejay_msg(VEEJAY_MSG_INFO, "Automatic Parameter %d of Effect on entry %d disabled",
					args[2],args[1]);
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Failed to set automatic parameter");
			}

		}
	}

}


void vj_event_chain_entry_channel_dec(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[1];
	char *str = NULL; P_A(args,str,format,ap);

	//DUMP_ARG(args);

	if(CLIP_PLAYING(v))
	{
		int entry = clip_get_selected_entry(v->uc->clip_id);
		int cha = clip_get_chain_channel(v->uc->clip_id,entry);
		int src = clip_get_chain_source(v->uc->clip_id,entry);

		if(src==VJ_TAG_TYPE_NONE)
		{	//decrease clip id
			if(cha <= 1)
			{
				cha = clip_size()-1;
				if(cha <= 0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No clips to mix with");
					return;
				}		
			}
			else
			{
				cha = cha - args[0];
			}
		}
		else	
		{
			if( cha <= 1)
			{
				cha = vj_tag_size()-1;
				if(cha<=0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No streams to mix with");
					return;
				}
			}
			else
			{
				cha = cha - args[0];
			}
			src = vj_tag_get_type( cha );
			clip_set_chain_source( v->uc->clip_id,entry,src);
		}
		clip_set_chain_channel( v->uc->clip_id, entry, cha );
		veejay_msg(VEEJAY_MSG_INFO, "Chain entry %d uses %s %d",entry,
				(src==VJ_TAG_TYPE_NONE ? "Clip" : "Stream"),cha);
			 
		if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

	}
	if(TAG_PLAYING(v))
	{
		int entry = vj_tag_get_selected_entry(v->uc->clip_id);
		int cha   = vj_tag_get_chain_channel(v->uc->clip_id,entry);
		int src   = vj_tag_get_chain_source(v->uc->clip_id,entry);
		char description[100];
		if(src==VJ_TAG_TYPE_NONE)
		{	//decrease clip id
			if(cha <= 1)
			{
				cha = clip_size()-1;
				if(cha <= 0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No clips to mix with");
					return;
				}		
			}
			else
			{
				cha = cha - args[0];
			}
		}
		else	
		{
			if( cha <= 1)
			{
				cha = vj_tag_size()-1;
				if(cha<=0)
				{
					veejay_msg(VEEJAY_MSG_ERROR, "No streams to mix with");
					return;
				}
			}
			else
			{
				cha = cha - args[0];
			}
			src = vj_tag_get_type( cha );
			vj_tag_set_chain_source( v->uc->clip_id, entry, src);
		}
		vj_tag_set_chain_channel( v->uc->clip_id, entry, cha );
		vj_tag_get_description( cha, description);

		veejay_msg(VEEJAY_MSG_INFO, "Chain entry %d uses Stream %d (%s)",entry,cha,description);
		if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);
 
	}

}

void vj_event_chain_entry_channel_inc(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[1];
	char *str = NULL; P_A(args,str,format,ap);

	//DUMP_ARG(args);

	if(CLIP_PLAYING(v))
	{
		int entry = clip_get_selected_entry(v->uc->clip_id);
		int cha = clip_get_chain_channel(v->uc->clip_id,entry);
		int src = clip_get_chain_source(v->uc->clip_id,entry);
		if(src==VJ_TAG_TYPE_NONE)
		{
			int num_c = clip_size()-1;
			if(num_c <= 0)
			{
				veejay_msg(VEEJAY_MSG_ERROR, "No clips to mix with");
				return;
			}
			//decrease clip id
			if(cha >= num_c)
			{
				cha = 1;
			}
			else
			{
				cha = cha + args[0];
			}
		}
		else	
		{
			int num_c = vj_tag_size()-1;
			if(num_c <=0 )
			{
				veejay_msg(VEEJAY_MSG_ERROR, "No streams to mix with");	
				return;
			}
			if( cha >= num_c)
			{
				cha = 1;
			}
			else
			{
				cha = cha + args[0];
			}
			src = vj_tag_get_type( cha );
			clip_set_chain_source( v->uc->clip_id, entry,src );
		}
		clip_set_chain_channel( v->uc->clip_id, entry, cha );
		veejay_msg(VEEJAY_MSG_INFO, "Chain entry %d uses %s %d",entry,
			(src==VJ_TAG_TYPE_NONE ? "Clip" : "Stream"),cha);
		if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);
	
			 
	}
	if(TAG_PLAYING(v))
	{
		int entry = vj_tag_get_selected_entry(v->uc->clip_id);
		int cha   = vj_tag_get_chain_channel(v->uc->clip_id,entry);
		int src   = vj_tag_get_chain_source(v->uc->clip_id,entry);
		char description[100];
		if(src==VJ_TAG_TYPE_NONE)
		{
			int num_c = clip_size()-1;
			if(num_c <= 0)
			{
				veejay_msg(VEEJAY_MSG_ERROR, "No clips to mix with");
				return;
			}
			//decrease clip id
			if(cha >= num_c)
			{
				cha = 1;
			}
			else
			{
				cha = cha + args[0];
			}
		}
		else	
		{
			int num_c = vj_tag_size()-1;
			if(num_c <=0 )
			{
				veejay_msg(VEEJAY_MSG_ERROR, "No streams to mix with");	
				return;
			}
			if( cha >= num_c)
			{
				cha = 1;
			}
			else
			{
				cha = cha + args[0];
			}
			src = vj_tag_get_type( cha );
			vj_tag_set_chain_source( v->uc->clip_id, entry, src);
		}

		vj_tag_set_chain_channel( v->uc->clip_id, entry, cha );
		vj_tag_get_description( cha, description);
		if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

		veejay_msg(VEEJAY_MSG_INFO, "Chain entry %d uses Stream %d (%s)",entry,cha,description);
	}
}

void vj_event_chain_entry_channel(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[3];
	char *str = NULL; P_A(args,str,format,ap);
	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(clip_exists(args[0]))
		{
			int src = clip_get_chain_source( args[0],args[1]);
			if(src == VJ_TAG_TYPE_NONE)
			{
				if(clip_exists(args[2])) 
				{
					if(clip_set_chain_channel(args[0],args[1], args[2])!=-1)	
					{
						veejay_msg(VEEJAY_MSG_INFO, "Selected clip channel %d", args[2]);
						if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

					}
				}
			}
			else
			{
				if(vj_tag_exists(args[2])) 
				{
					if(clip_set_chain_channel(args[0],args[1],args[2])!=-1)
					{
						veejay_msg(VEEJAY_MSG_INFO, "Selected tag channel %d", args[2]);
						if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

					}
				}
			}
		}
	}
	if(TAG_PLAYING(v))
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
		if(vj_tag_exists(args[0]))
		{
			int src = vj_tag_get_chain_source(args[0],args[1]);
			if(src == VJ_TAG_TYPE_NONE)
			{
				if(clip_exists(args[2])) 
				{
					if(vj_tag_set_chain_channel(args[0],args[1],args[2])!=-1)
					{
						veejay_msg(VEEJAY_MSG_INFO, "Selected clip %d to mix in",args[2]);
						if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

					}
				}
			}
			else
			{
				if(vj_tag_exists(args[2])) 
				{
					if(vj_tag_set_chain_channel(args[0],args[1],args[2])!=-1)		
					{
						veejay_msg(VEEJAY_MSG_INFO, "Selected stream %d to mix in", args[2]);
						if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);

					}
				}

			}
		}
	}
}

void vj_event_chain_entry_srccha(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[4];
	char *str = NULL; P_A(args,str,format,ap);

	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[0] == -1) args[0] = clip_size()-1;
		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(clip_exists(args[0]))
		{
			if(args[2] == VJ_TAG_TYPE_NONE)
			{
				if(clip_exists(args[3])) {
					if(	clip_set_chain_source(args[0],args[1],0)!=-1 &&
						clip_set_chain_channel(args[0],args[1],args[3]) != -1)
					{
						veejay_msg(VEEJAY_MSG_INFO, "Selected clip %d to mix in", args[3]);
						if(v->no_bezerk) veejay_set_clip(v,v->uc->clip_id);
					}
				}
			}
			else
			{
				if(vj_tag_exists(args[3])) {
					int src = vj_tag_get_type( args[3] );
					if(	clip_set_chain_source(args[0],args[1],src ) != -1 &&
						clip_set_chain_channel(args[0],args[1],args[3]) != -1)
					{
						veejay_msg(VEEJAY_MSG_INFO, "Selected stream %d to mix in", args[3]);
						if(v->no_bezerk) veejay_set_clip(v,v->uc->clip_id);
					}
				}
			}
		}
	}
	if(TAG_PLAYING(v))
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[0] == -1) args[0] = clip_size()-1;

		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
	 	if(vj_tag_exists(args[0])) 
		{
			if(args[2] == VJ_TAG_TYPE_NONE)
			{
				if(clip_exists(args[3])) 
				{
					if(	vj_tag_set_chain_source(args[0],args[1],0) != -1 &&
						vj_tag_set_chain_channel(args[0],args[1],args[3]) != -1 )
					{
						veejay_msg(VEEJAY_MSG_INFO, "Selected clip %d to mix in",args[3]);
						if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);
					}
				}
			}
			else
			{
				if(vj_tag_exists(args[3])) 
				{
					int src = vj_tag_get_type( args[3]);
					if(	vj_tag_set_chain_source(args[0],args[1],src) != -1 &&
						vj_tag_set_chain_channel(args[0],args[1],args[3])!= -1)
					{
						veejay_msg(VEEJAY_MSG_INFO, "Selected stream %d to mix in", args[3]);
						if(v->no_bezerk) veejay_set_clip(v, v->uc->clip_id);
					}
				}
	
			}
		}
	}

}


void vj_event_chain_arg_inc(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*)ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if(CLIP_PLAYING(v)) 
	{
		int c = clip_get_selected_entry(v->uc->clip_id);
		int effect = clip_get_effect_any(v->uc->clip_id, c);
		int val = clip_get_effect_arg(v->uc->clip_id,c,args[0]);
		if ( vj_effect_is_valid( effect  ) ) {
		   if( vj_effect_valid_value( effect, args[0], (val+args[1])))
		   {
			if(clip_set_effect_arg( v->uc->clip_id, c,args[0],(val+args[1]))!=-1 )
			{
				veejay_msg(VEEJAY_MSG_INFO,"Set parameter %d value %d",args[0],(val+args[1]));
			}
		   }
		   else 
		   {
			/* see if we start at min or go to max */
			if( (val+args[1]) > vj_effect_get_max_limit( effect, args[0]))
			{
				args[1] = vj_effect_get_min_limit( effect, args[0]);
			}
			if ( (val+args[1]) < vj_effect_get_min_limit(effect, args[0])) 
			{
				args[1] = vj_effect_get_max_limit(effect, args[0]);
			}

			if(clip_set_effect_arg( v->uc->clip_id, c,args[0],args[1]) )
			{
				veejay_msg(VEEJAY_MSG_INFO,"Set parameter %d value %d",args[0],args[1]);
			}
				
			
		   }
		}
	}
	if(TAG_PLAYING(v)) 
	{
		int c = vj_tag_get_selected_entry(v->uc->clip_id);
		int effect = vj_tag_get_effect_any(v->uc->clip_id, c);
		int val = vj_tag_get_effect_arg(v->uc->clip_id, c, args[0]);
		if( vj_effect_valid_value( effect,args[0],(val+args[1]) )  )
		{
			if(vj_tag_set_effect_arg(v->uc->clip_id, c, args[0], (val+args[1])) )
			{
				veejay_msg(VEEJAY_MSG_INFO,"Set parameter %d value %d",args[0],(val+args[1]));
			}
		}
		else
		{
			if ( (val+args[1]) > vj_effect_get_max_limit( effect,args[0] ))
			{
				args[1] = vj_effect_get_min_limit( effect,args[0] );
			}
			if ( (val+args[1]) < vj_effect_get_min_limit( effect, args[0] ))
			{
				args[1] = vj_effect_get_max_limit( effect, args[0] );
			}
			if( clip_set_effect_arg( v->uc->clip_id, c,args[0],args[1]) )
			{
				veejay_msg(VEEJAY_MSG_INFO, "Set parameter %d to value %d",args[0],args[1]);
			}
		}
	}
}

void vj_event_chain_entry_set_arg_val(void *ptr, const char format[], va_list ap)
{
	int args[4];
	veejay_t *v = (veejay_t*)ptr;
	char *str = NULL; P_A(args,str,format,ap);
	
	if(CLIP_PLAYING(v)) 
	{
	 	if(args[0] == 0) args[0] = v->uc->clip_id;
		if(args[0] == -1) args[0] = clip_size()-1;

		if(args[1] == -1) args[1] = clip_get_selected_entry(v->uc->clip_id);
		if(clip_exists(args[0]))
		{
			int effect = clip_get_effect_any( args[0], args[1] );
			if( vj_effect_valid_value(effect,args[2],args[3]) )
			{
				if(clip_set_effect_arg( args[0], args[1], args[2], args[3])) {
				  veejay_msg(VEEJAY_MSG_INFO, "Set parameter %d to %d on Entry %d of Clip %d", args[2], args[3],args[1],args[0]);
				}
			}
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR,"Tried to set invalid parameter value/type: %d %d",
		args[2],args[3]);
			}
		} else { veejay_msg(VEEJAY_MSG_ERROR, "Clip %d does not exist", args[0]); }	
	}
	if(TAG_PLAYING(v))
	{
		if(args[0] == 0) args[0] = v->uc->clip_id;

		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(v->uc->clip_id);
		if(vj_tag_exists(args[0]))
		{
			int effect = vj_tag_get_effect_any(args[0],args[1] );
			if ( vj_effect_valid_value( effect,args[2],args[3] ) )
			{
				if(vj_tag_set_effect_arg(args[0],args[1],args[2],args[3])) {
					veejay_msg(VEEJAY_MSG_INFO,"Set parameter %d to %d on Entry %d of Stream %d", args[2],args[3],args[2],args[1]);
				}
			}
			else {
			veejay_msg(VEEJAY_MSG_ERROR, "Tried to set invalid parameter value/type : %d %d",
				args[2],args[3]);
			}
		}
		else {
			veejay_msg(VEEJAY_MSG_ERROR,"Stream %d does not exist", args[0]);
		}
	}
}

void vj_event_el_cut(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);
	if( PLAIN_PLAYING(v)) 
	{
		if(veejay_edit_cut( v, args[0], args[1] ))
		{
			veejay_msg(VEEJAY_MSG_INFO, "Cut frames %d-%d into buffer",args[0],args[1]);
		}
	}
}

void vj_event_el_copy(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);
	if( PLAIN_PLAYING(v)) 
	{
		if(veejay_edit_copy( v, args[0],args[1] )) 
		{
			veejay_msg(VEEJAY_MSG_INFO, "Copy frames %d-%d into buffer",args[0],args[1]);
		}
	}

}

void vj_event_el_del(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if( PLAIN_PLAYING(v)) 
	{
		if(veejay_edit_delete(v, args[0],args[1])) 
		{
			veejay_msg(VEEJAY_MSG_INFO, "Del frames %d-%d into buffer", args[0],args[1]);
		}
	}
}

void vj_event_el_crop(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if( PLAIN_PLAYING(v)) 
	{
		if(args[0] >= 1 && args[1] <= v->editlist->video_frames-1 && args[1] >= 1 && args[1] > args[0] && args[1] <= 
			v->editlist->video_frames-1) 
		{
			if(veejay_edit_delete(v, 1, args[0]) && veejay_edit_delete(v, args[1], v->editlist->video_frames-1) )
			{
				veejay_set_frame(v,1);
				veejay_msg(VEEJAY_MSG_INFO, "Cropped frames %d-%d", args[0],args[1]);
			}
		}
	}
}

void vj_event_el_paste_at(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *str = NULL; P_A(args,str,format,ap);
	if( PLAIN_PLAYING(v)) 
	{
		if( args[0] >= 1 && args[0] <= v->editlist->video_frames-1)
		{		
			if( veejay_edit_paste( v, args[0] ) ) 
			{
				veejay_msg(VEEJAY_MSG_INFO, "Pasted buffer at frame %d",args[0]);
			}
		}

	}
}

void vj_event_el_save_editlist(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	char str[1024];
	int *args = NULL;
	P_A(args,str,format,ap);
	if( veejay_save_all(v, str) )
	{
		veejay_msg(VEEJAY_MSG_INFO, "Saved EditList as %s",str);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR,"Unable to save EditList as %s",str);
	}
}
void vj_event_el_load_editlist(void *ptr, const char format[], va_list ap)
{
	veejay_msg(VEEJAY_MSG_ERROR, "EditList: Load not implemented");
}

void vj_event_el_add_video(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int start = -1;
	int destination = v->editlist->video_frames-1;
	char str[1024];
	int *args = NULL;
	P_A(args,str,format,ap);

	if ( veejay_edit_addmovie(v,str,start,destination,destination))
	{
		veejay_msg(VEEJAY_MSG_INFO, "Appended video file %s to EditList",str); 
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot append file %s to EditList",str);
	}
}

void vj_event_el_add_video_clip(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int start = -1;
	int destination = v->editlist->video_frames-1;
	char str[1024];
	int args[1];
	P_A(args,str,format,ap);

	if ( veejay_edit_addmovie(v,str,start,destination,destination))
	{
		int start_pos = destination;
		int end_pos = (args[0]==0 ? v->editlist->video_frames-1 : destination+args[0]);
		clip_info *skel = clip_skeleton_new(start_pos,end_pos);
		if(clip_store(skel) == 0)
		{
			veejay_msg(VEEJAY_MSG_INFO, "Appended video file %s to EditList as new clip %d",str,skel->clip_id); 
		}
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Appended file %s to EditList",str);
	}
	
}
void vj_event_el_add_video_his(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int start = -1;
	int destination = v->editlist->video_frames-1;
	char str[1024];
	int args[4];		/* clip_id, len, entry, file */
	P_A(args,str,format,ap);

	if ( !clip_exists(args[0]))
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Clip %d does not exist",args[0]);
		return;
	}
	if ( args[2] < 0 || args[2] >= CLIP_MAX_RENDER)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Invalid render entry %d given",args[2]);
		return;
	}
	if ( veejay_edit_addmovie(v,str,start,destination,destination))
	{
		int start_pos = destination;
		int end_pos = (args[1]==0 ? v->editlist->video_frames-1 : destination+args[1]);
		int cs = clip_get_render_entry( args[0] );
		clip_set_render_entry( args[0], args[2] );
		clip_set_startframe( args[0], start_pos );
		clip_set_endframe( args[0], end_pos);
		clip_set_render_entry( args[0], cs );
		veejay_msg(VEEJAY_MSG_INFO, "Clip %d R[%d,%d] added on Entry %d (%ld frames)",
			args[0],start_pos,end_pos,args[2],v->editlist->video_frames-1);
		v->uc->render_changed = 1;
		clip_entry_set_is_rendering( args[0], args[2], 0);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot add file %s to EditList",str);
	}

}

void vj_event_tag_del(void *ptr, const char format[] , va_list ap ) 
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char *str = NULL; P_A(args,str,format,ap);

	
	if(TAG_PLAYING(v) && v->uc->clip_id == args[0])
	{
		veejay_msg(VEEJAY_MSG_INFO,"Cannot delete stream while playing");
	}
	else 
	{
		if(vj_tag_exists(args[0]))	
		{
			if(vj_tag_del(args[0]))
			{
				veejay_msg(VEEJAY_MSG_INFO, "Deleted stream %d", args[0]);
			}
		}	
	}
}

void vj_event_tag_toggle(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[1];
	char *str = NULL; P_A(args,str,format,ap);
	if(TAG_PLAYING(v))
	{
		int active = vj_tag_get_active(v->uc->clip_id);
		vj_tag_set_active( v->uc->clip_id, !active);
		veejay_msg(VEEJAY_MSG_INFO, "Stream is %s", (vj_tag_get_active(v->uc->clip_id) ? "active" : "disabled"));
	}
}

void vj_event_tag_new_v4l(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	char str[255];
	int *args = NULL;
	P_A(args,str,format,ap);

	if( veejay_create_tag(v, VJ_TAG_TYPE_V4L, str, v->nstreams,0) == 0)
	{
		veejay_msg(VEEJAY_MSG_INFO, "Created new Video4Linux stream %d", vj_tag_size()-1);
	}	
}

void vj_event_tag_new_raw(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	char str[255];
	int *args = NULL;
	P_A(args,str,format,ap);
	if ( veejay_create_tag(v, VJ_TAG_TYPE_RAW, str, v->nstreams,0) == 0) 
	{
		veejay_msg(VEEJAY_MSG_INFO, "Created new RGB24/RAW stream %d", vj_tag_size()-1);
	}

}

void vj_event_tag_new_y4m(void *ptr, const char format[], va_list ap)
{
	veejay_t *v= (veejay_t*) ptr;
	char str[255];
	int *args = NULL;
	P_A(args,str,format,ap);
	if( veejay_create_tag(v, VJ_TAG_TYPE_YUV4MPEG, str, v->nstreams,0) == 0)
	{
		veejay_msg(VEEJAY_MSG_INFO, "Created new YUV4MPEG stream %d", vj_tag_size()-1);
	}
}

void vj_event_v4l_set_brightness(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str,format,ap);
	if(args[0]==0) args[0] = v->uc->clip_id;
	if(args[0]==-1) args[0] = vj_tag_size()-1;
	if(vj_tag_exists(args[0]) && TAG_PLAYING(v))
	{
		if(vj_tag_set_brightness(args[0],args[1]))
		{
			veejay_msg(VEEJAY_MSG_INFO,"Set brightness to %d",args[1]);
		}
	}
	
}
void vj_event_v4l_set_contrast(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str,format,ap);
	if(args[0]==0) args[0] = v->uc->clip_id;
	if(args[0]==-1)args[0] = vj_tag_size()-1;
	if(vj_tag_exists(args[0]) && TAG_PLAYING(v))
	{
		if(vj_tag_set_contrast(args[0],args[1]))
		{
			veejay_msg(VEEJAY_MSG_INFO,"Set contrast to %d",args[1]);
		}
	}

}
void vj_event_v4l_set_color(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str,format,ap);
	if(args[0] == 0) args[0] = v->uc->clip_id;
	if(args[0] == -1) args[0] = vj_tag_size()-1;
	if(vj_tag_exists(args[0]) && TAG_PLAYING(v))
	{
		if(vj_tag_set_color(args[0],args[1]))
		{
			veejay_msg(VEEJAY_MSG_INFO,"Set color to %d",args[1]);
		}
	}

}
void vj_event_v4l_set_hue(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str,format,ap);
	if(args[0] == 0) args[0] = v->uc->clip_id;
	if(args[0] == -1) args[0] = vj_tag_size()-1;
	if(vj_tag_exists(args[0]) && TAG_PLAYING(v))
	{
		if(vj_tag_set_hue(args[0],args[1]))
		{
			veejay_msg(VEEJAY_MSG_INFO,"Set hue to %d",args[1]);
		}
	}

}


void	vj_event_tag_new_shm(void *ptr, const char format[], va_list ap)
{
	int args[2];
	veejay_t * v = (veejay_t*) ptr;
	char *s = NULL;
	P_A(args,s,format,ap);

	if(v->client) free(v->client);
	v->client = new_c_segment( args[0], args[1] );
	if(!v->client)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot get shared memory segment");
	}	
	else
	{
		veejay_msg(VEEJAY_MSG_INFO, "Attached SHM %d , SEM %d", args[0],args[1]);
		attach_segment( v->client, 0 );
	}
	if( veejay_create_tag( v, VJ_TAG_TYPE_SHM, "/shm", v->nstreams,0) == 0)
	{
		veejay_msg(VEEJAY_MSG_INFO, "SHM stream ready");
	}
}

void vj_event_tag_new_vloopback_name(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	char str[255];	
	int args[4];
	P_A(args,str,format,ap);
	if(args[0] == 0)
	{
		args[0] = VIDEO_PALETTE_RGB24;
		veejay_msg(VEEJAY_MSG_INFO, "Using video palette RGB24");

	}
	else
	{
		args[1] = VIDEO_PALETTE_YUV420P;
		veejay_msg(VEEJAY_MSG_INFO, "Using video palette YUV 4:2:0 Planar");

	}
	if( veejay_create_tag(v, VJ_TAG_TYPE_VLOOPBACK, str,v->nstreams, args[0]) == 0)
	{
		veejay_msg(VEEJAY_MSG_INFO, "Created new vloopback device %s", str);
	}
}

void vj_event_tag_new_vloopback_id(void *ptr, const char format[] , va_list ap)
{
	int args[4];
	char filename[255];
	veejay_t *v = (veejay_t*) ptr;
	char *str = NULL; 
	P_A(args,str,format,ap);
	if(args[0] == 0)
	{
		args[0] = VIDEO_PALETTE_RGB24;
		veejay_msg(VEEJAY_MSG_INFO, "Using video palette RGB24");
	}
	else
	{
		args[0] = VIDEO_PALETTE_YUV420P;
		veejay_msg(VEEJAY_MSG_INFO, "Using video palette YUV 4:2:0 Planar");
	}

	if( (vj_vloopback_verify_pipe(0, args[0],filename) == 0) &&
	    (veejay_create_tag(v, VJ_TAG_TYPE_VLOOPBACK,filename,v->nstreams,args[0])==0) )
	{
		veejay_msg(VEEJAY_MSG_INFO,"Created new vloopback device %s", filename);
	}

}


void vj_event_tag_set_format(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char str[255]; 
	P_A(args,str,format,ap);

	if(v->settings->clip_record || v->settings->tag_record || v->settings->offline_record)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Cannot change data format while recording to disk");
		return;
	}

	if(strncasecmp(str,"mpeg4",5)==0 || strncasecmp(str,"divx",4)==0)
	{
		_recorder_format = DATAFORMAT_MPEG4;
		veejay_msg(VEEJAY_MSG_INFO, "Recorder writes in MPEG4 format");
	}
	if(strncasecmp(str,"msmpeg4v3",9)==0 || strncasecmp(str,"div3",4)==0)
	{
		_recorder_format = DATAFORMAT_DIVX;
		veejay_msg(VEEJAY_MSG_INFO,"Recorder writes in MSMPEG4v3 format");
	}
#ifdef SUPPORT_READ_DV2
	if(strncasecmp(str,"dv",2)==0||strncasecmp(str,"dvsd",4)==0)
	{
		if(v->editlist->video_width == 720 &&
		(v->editlist->video_height = 576 || v->editlist->video_height==480) )
		{
			_recorder_format = DATAFORMAT_DV2;
			veejay_msg(VEEJAY_MSG_INFO,"Recorder writes in DV2 format");
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR,"For this movie DV must have geometry 720x%d",	(v->editlist->video_norm == 'p' ? 576 : 480));
		}
	}
#endif
	if(strncasecmp(str,"mjpeg",5)== 0 || strncasecmp(str,"mjpg",4)==0 ||
		strncasecmp(str, "jpeg",4)==0)
	{
		_recorder_format = DATAFORMAT_MJPG;
		veejay_msg(VEEJAY_MSG_INFO, "Recorder writes in MJPEG format");
	}
	if(strncasecmp(str,"i420",4)==0 || strncasecmp(str,"yv12",4)==0 )
	{
		_recorder_format = DATAFORMAT_YUV420;
		veejay_msg(VEEJAY_MSG_INFO, "Recorder writes in uncompressed YV12/I420 (see swapping)");
	}
	

}

static void _vj_event_tag_record( veejay_t *v , int *args, char *str )
{
	if( TAG_PLAYING(v))
	{
		char tmp[255];
		if(args[0] <= 0) 
		{
			veejay_msg(VEEJAY_MSG_ERROR,"Number of frames to record must be > 0");
			return;
		}
		if(args[1] < 0 || args[1] > 1)
		{
			veejay_msg(VEEJAY_MSG_ERROR,"Auto play is either on or off");
			return;
		}	
		//todo: let user set base filename for recording
		sprintf(tmp, "tag-%d-%d-XXXXXX", v->uc->clip_id, args[0]);
		if( mkstemp(tmp) != -1)
		{
			int format = _recorder_format;
			if(_recorder_format == -1)
			{
				format = el_video_frame_data_format(0,v->editlist);
			}
			veejay_msg(VEEJAY_MSG_INFO, "Base name [%s]", tmp);	
			if( vj_tag_init_encoder( v->uc->clip_id, tmp, format,		
				args[0]) != 1 ) 
			{
				veejay_msg(VEEJAY_MSG_INFO, "Error trying to start recording from stream %d", v->uc->clip_id);
				return;
			} 

			if(args[1]==0) v->settings->tag_record_switch = 0;else v->settings->tag_record_switch = 1;
			v->settings->tag_record = 1;
	//		v->settings->tag_record_id = v->uc->clip_id;
		}
	}
}

void vj_event_tag_rec_start(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL; 
	P_A(args,str,format,ap);

	_vj_event_tag_record( v, args, str );
}

void vj_event_tag_rec_stop(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v = (veejay_t *)ptr;
	video_playback_setup *s = v->settings;
	if( TAG_PLAYING(v)  && (v->settings->tag_record != 0)) 
	{
		if(vj_tag_stop_encoder( v->uc->clip_id))
		{
			char avi_file[255];
			int start = -1;
			int destination = v->editlist->video_frames - 1;

			if( vj_tag_get_encoded_file(v->uc->clip_id, avi_file)!=0) return;
			

			if( veejay_edit_addmovie(
				v,avi_file,start,destination,destination))
			{
				int len = vj_tag_get_encoded_frames(v->uc->clip_id) - 1;
				clip_info *skel = clip_skeleton_new(destination, destination+len);		
				if(clip_store(skel)==0) 
				{
					veejay_msg(VEEJAY_MSG_INFO, "Created new clip %d from file %s",
						skel->clip_id,avi_file);
					clip_set_looptype( skel->clip_id,1);
				}	
			}		
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Cannot add videofile %s to EditList!",avi_file);
			}

			vj_tag_reset_encoder( v->uc->clip_id);

			veejay_msg(VEEJAY_MSG_INFO, "Recorded new clip %d from stream %d", 
				s->offline_created_clip,s->offline_tag_id);
			s->tag_record = 0;
			//FIXME
			if(s->tag_record_switch) 
			{
				veejay_change_playback_mode( v, VJ_PLAYBACK_MODE_CLIP, clip_size()-1 );
			}
			s->tag_record_switch = 0;

		}
	}
	else
	{
		if(v->settings->offline_record)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Perhaps you want to stop recording from a non visible stream ? See VIMS id %d",
				NET_TAG_OFFLINE_REC_STOP);
		}
		veejay_msg(VEEJAY_MSG_ERROR, "Stream record was even started!");
	}
}

void vj_event_tag_rec_offline_start(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[3];
	char *str = NULL; P_A(args,str,format,ap);

	if( v->settings->offline_record )
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Already recording from a stream");
		return;
	}
 	if( v->settings->tag_record)
        {
		veejay_msg(VEEJAY_MSG_ERROR ,"Please stop the stream recorder first");
		return;
	}

	if( TAG_PLAYING(v) && (args[0] == v->uc->clip_id) )
	{
		veejay_msg(VEEJAY_MSG_INFO,"Using stream recorder for stream  %d (is playing)",args[0]);
		_vj_event_tag_record(v, args+1, str);
		return;
	}


	if( vj_tag_exists(args[0]))
	{
		char tmp[255];
		int format = _recorder_format;
		sprintf(tmp, "stream-%d-%d-XXXXXX", args[0], args[1]);
		if( mkstemp(tmp) != -1)
		{
			if(format==-1)
			{
				format = el_video_frame_data_format(0,v->editlist);
			}
			if( vj_tag_init_encoder( args[0], tmp, format,		
				args[1]) ) 
			{
				video_playback_setup *s = v->settings;
				veejay_msg(VEEJAY_MSG_INFO, "(Offline) recording from stream %d", args[0]);
				s->offline_record = 1;
				s->offline_tag_id = args[0];
				s->offline_created_clip = args[1];
			} 
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "(Offline) error starting recording stream %d",args[0]);
			}
		}
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Stream %d does not exist",args[0]);
	}
}

void vj_event_tag_rec_offline_stop(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	video_playback_setup *s = v->settings;
	if(s->offline_record) 
	{
		if( vj_tag_stop_encoder( s->offline_tag_id ) == 0 )
		{
			char avi_file[255];
			int start = -1;
			int destination = v->editlist->video_frames - 1;

			if( vj_tag_get_encoded_file(v->uc->clip_id, avi_file)!=0) return;
			

			if( veejay_edit_addmovie(
				v,avi_file,start,destination,destination))
			{
				int len = vj_tag_get_encoded_frames(v->uc->clip_id) - 1;
				clip_info *skel = clip_skeleton_new(destination, destination+len);		
				if(clip_store(skel)==0) 
				{
					veejay_msg(VEEJAY_MSG_INFO, "Created new clip %d from file %s",
						skel->clip_id,avi_file);
					clip_set_looptype( skel->clip_id,1);
				}	
			}		
			else
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Cannot add videofile %s to EditList!",avi_file);
			}

			vj_tag_reset_encoder( v->uc->clip_id);

			veejay_msg(VEEJAY_MSG_INFO, "Recorded new clip %d from stream %d", 
				s->offline_created_clip,s->offline_tag_id);
			s->offline_record = 0;

			if(s->offline_created_clip) 
			{
				veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_CLIP , clip_size()-1);
			}
			s->offline_record = 0;
			s->offline_tag_id = 0;
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Clip recorder was never started for clip %d",v->uc->clip_id);
		}


	}
}

static int vloopback_out_set_palette(veejay_t *v, int palette)
{
	int rp;
	switch(palette)
	{
		case 1: rp = VIDEO_PALETTE_YUV420P; break;
		case 0: rp = VIDEO_PALETTE_RGB24; break;
		default:
		veejay_msg(VEEJAY_MSG_ERROR,"Only YV12/RGB24 supported");
		return -1;
		break;
	}

	v->lb_out->preferred_palette = rp;
	return 1;
}



void vj_event_output_vloopback_startn(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if(v->vloopback_out != 1) 
	{
		int args[5];
		char str[255];
		P_A(args,str,format,ap);
		if(args[0] == 0 || args[0] == 1)
		{
			if(vloopback_out_set_palette(v,args[0])==1)
			{
				veejay_msg(VEEJAY_MSG_INFO,"Video palette set to [%s]", (args[0]==0 ? "RGB24" : "YV12"));
			}
		}
		else
		{
			veejay_msg(VEEJAY_MSG_INFO, "Video palette unknown. Use 0 for YV12, 1 for RGB24.  Aborted vloopback output.");
			return;
		}

		sprintf(v->lb_out->pipepath, "/dev/%s",str);
		
		if( vj_vloopback_open( v->lb_out, v->editlist->video_width,v->editlist->video_height,v->editlist->video_norm)==0 )
		{
			veejay_msg(VEEJAY_MSG_INFO, "Started output to vloopback device %s", v->lb_out->pipepath);
			v->vloopback_out = 1;
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Failed to output video to vloopback device %s", v->lb_out->pipepath);
		}
		return;
	}
	veejay_msg(VEEJAY_MSG_INFO, "Vloopback output pipe already active. (only 1 output)");
		
}

void vj_event_output_vloopback_start(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[2];
	char *str = NULL; P_A(args,str,format,ap);

	if(args[1] == 0 || args[1] == 1)
	{
		if(vloopback_out_set_palette(v,args[0])==1)
		{
			veejay_msg(VEEJAY_MSG_INFO,"Video palette set to [%s]", (args[0]==1 ? "YV12" : "RGB24"));
		}
	}
	else
	{
		veejay_msg(VEEJAY_MSG_INFO, "Video palette unknown. Use 0 for YV12, 1 for RGB24.  Aborted vloopback output.");
		return;
	}

	if( vj_vloopback_verify_pipe(1, args[0], v->lb_out->pipepath) == 0) 
	{
		if( v->vloopback_out == 1 ) 
		{
			veejay_msg(VEEJAY_MSG_INFO, "Vloopback output pipe already active. (only 1 output)");
			return;
		}
		veejay_msg(VEEJAY_MSG_INFO, "Vloopback pipe %d exists, opening ...",args[0]);
		if( vj_vloopback_open( v->lb_out, v->editlist->video_width,v->editlist->video_height,v->editlist->video_norm)==0 )
		{
			veejay_msg(VEEJAY_MSG_INFO, "Started output to vloopback device %s", v->lb_out->pipepath);
			v->vloopback_out = 1;
		}
	}
}

void vj_event_output_vloopback_stop(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	if(v->vloopback_out == 1)
	{
		if( vj_vloopback_close(v->lb_out) == 0)
		{
			veejay_msg(VEEJAY_MSG_INFO, "Closed output to vloopback device %s", v->lb_out->pipepath);
		}
	}
	v->vloopback_out = 0;
}

void vj_event_output_y4m_start(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	char str[1024];
	int *args = NULL;
	P_A( args,str,format,ap);
	if(v->stream_enabled==0)
	{
		int n=0;
		strncpy(v->stream_outname, str,strlen(str));
		n= vj_yuv_stream_start_write(v->output_stream,str,v->editlist);
		if(n==1) 
		{
			int s = v->uc->speed;
			veejay_msg(VEEJAY_MSG_INFO, "Pausing veejay");
			
			veejay_set_speed(v,0);
			if(vj_yuv_stream_open_pipe(v->output_stream,str,v->editlist))
			{
				vj_yuv_stream_header_pipe(v->output_stream,v->editlist);
				v->stream_enabled = 1;
			}
			veejay_set_speed(v,s);
			
		}
		if(n==0)
		if( vj_yuv_stream_start_write(v->output_stream,str,v->editlist)==0)
		{
			v->stream_enabled = 1;
			veejay_msg(VEEJAY_MSG_INFO, "Started YUV4MPEG streaming to [%s]", str);
		}
		if(n==-1)
		{
			veejay_msg(VEEJAY_MSG_INFO, "YUV4MPEG stream not started");
		}
	}	
}

void vj_event_output_y4m_stop(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	if(v->stream_enabled==1)
	{
		vj_yuv_stream_stop_write(v->output_stream);
		v->stream_enabled = 0;
		veejay_msg(VEEJAY_MSG_INFO , "Stopped YUV4MPEG streaming to %s", v->stream_outname);
	}
}

void vj_event_enable_audio(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	if(v->editlist->has_audio)
	{
		if(v->audio != AUDIO_PLAY)
		{
			if( vj_perform_audio_start(v) )
			{
				v->audio = AUDIO_PLAY;
			}
			else 
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Cannot start Jack ");
			}
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Already playing audio");
		}
	}
	else 
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Video has no audio");
	}
}

void vj_event_disable_audio(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t *)ptr;
	if(v->editlist->has_audio)
	{
		if(v->audio == AUDIO_PLAY)
		{
			vj_perform_audio_stop(v);
			v->audio = NO_AUDIO;
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Not playing audio");
		}
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Video has no audio");
	}
}


void vj_event_output_raw_start(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	char str[1024];
	int *args = NULL;
	P_A( args,str,format,ap);
	if(v->output_raw_enabled==0)
	{
		strncpy(v->stream_outname, str,strlen(str));
		if( vj_raw_stream_start_write(v->render_raw,str)==0)
		{
			v->output_raw_enabled = 1;
			veejay_msg(VEEJAY_MSG_INFO, "Started raw stream to [%s]", str);
		}
		else 
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Cannot start writing raw stream to %s",str);
		}
	}	
	else 
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Already writing to raw stream to [%s]",v->stream_outname);
	}
}

void vj_event_output_raw_stop(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	if(v->output_raw_enabled)
	{
		vj_raw_stream_stop_rw(v->output_raw);
		v->output_raw_enabled = 0;
		veejay_msg(VEEJAY_MSG_INFO , "Stopped RAW stream to %s", v->stream_outname);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "RAW stream never started writing");
	}
}



void vj_event_effect_inc(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int real_id;
	
	if(!CLIP_PLAYING(v) && !TAG_PLAYING(v))
	{
		p_invalid_mode();
		return;
	}
	v->uc->key_effect += 1;
	if(v->uc->key_effect >= MAX_EFFECTS) v->uc->key_effect = 1;

	real_id = vj_effect_get_real_id(v->uc->key_effect);
	veejay_msg(VEEJAY_MSG_INFO, "Selected %s Effect %s (%d)", (vj_effect_get_extra_frame(real_id)==1 ? "Video" : "Image"),vj_effect_get_description(real_id),v->uc->key_effect);
}

void vj_event_effect_dec(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int real_id;
	if(!CLIP_PLAYING(v) && !TAG_PLAYING(v))
	{
		p_invalid_mode();
		return;
	}

	v->uc->key_effect -= 1;
	if(v->uc->key_effect <= 0) v->uc->key_effect = MAX_EFFECTS-1;
	
	real_id = vj_effect_get_real_id(v->uc->key_effect);
	veejay_msg(VEEJAY_MSG_INFO, "Selected %s Effect %s",
		(vj_effect_get_extra_frame(real_id) == 1 ? "Video" : "Image"), 
		vj_effect_get_description(real_id));	
}
void vj_event_effect_add(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	if(CLIP_PLAYING(v)) 
	{	
		int c = clip_get_selected_entry(v->uc->clip_id);
		if ( clip_chain_add( v->uc->clip_id, c, 
				       vj_effect_get_real_id(v->uc->key_effect)) != 1)
		{
			int real_id = vj_effect_get_real_id(v->uc->key_effect);
			veejay_msg(VEEJAY_MSG_INFO,"Effect %d %s on Entry %d",
				real_id,
				vj_effect_get_description(real_id),
				c
			);
			if(v->no_bezerk && vj_effect_get_extra_frame(real_id) ) veejay_set_clip(v,v->uc->clip_id);
			v->uc->chain_changed = 1;
		}
	}
	if(TAG_PLAYING(v))
	{
		int c = vj_tag_get_selected_entry(v->uc->clip_id);
		if ( vj_tag_set_effect( v->uc->clip_id, c,
				vj_effect_get_real_id( v->uc->key_effect) ) != -1) 
		{
			int real_id = vj_effect_get_real_id(v->uc->key_effect);
			veejay_msg(VEEJAY_MSG_INFO,"Effect %d %s on Entry %d",
			  	real_id,
				vj_effect_get_description(real_id),
				c
			);
			if(v->no_bezerk && vj_effect_get_extra_frame(real_id)) veejay_set_clip(v,v->uc->clip_id);
			v->uc->chain_changed = 1;
		}
	}

}

void vj_event_misc_start_rec_auto(void *ptr, const char format[], va_list ap)
{
 
}
void vj_event_misc_start_rec(void *ptr, const char format[], va_list ap)
{

}
void vj_event_misc_stop_rec(void *ptr, const char format[], va_list ap)
{

}

void vj_event_select_id(void *ptr, const char format[], va_list ap)
{
	veejay_t *v=  (veejay_t*)ptr;
	int args[2];
	char *str = NULL;
	P_A(args,str, format, ap);
	if(!TAG_PLAYING(v))
	{ 
		int clip_id = (v->uc->clip_key*12)-12 + args[0];
		if(clip_exists(clip_id))
		{
			veejay_change_playback_mode( v, VJ_PLAYBACK_MODE_CLIP, clip_id);
			vj_event_print_clip_info(v,clip_id);
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR,"Selected clip %d does not exist",clip_id);
		}
	}	
	else
	{
		int clip_id = (v->uc->clip_key*12)-12 + args[0];
		if(vj_tag_exists(clip_id ))
		{
			veejay_change_playback_mode(v, VJ_PLAYBACK_MODE_TAG ,clip_id);

		}
		else
		{
			veejay_msg(VEEJAY_MSG_INFO,"Selected stream %d does not exist",clip_id);
		}
	}

}

void vj_event_select_bank(void *ptr, const char format[], va_list ap) 
{
	veejay_t *v =(veejay_t*) ptr;
	int args[1];

	char *str = NULL; P_A(args,str,format,ap);
	if(args[0] >= 1 && args[0] <= 9)
	{
		veejay_msg(VEEJAY_MSG_INFO,"Selected bank %d (active clip range is now %d-%d)",args[0],
			(12 * args[0]) - 12 , (12 * args[0]));
		v->uc->clip_key = args[0];
	}
}

void vj_event_print_tag_info(veejay_t *v, int id) 
{
	int i, y, j, value;
	char description[100];
	char source[150];

	vj_tag_get_description(id,description);
	vj_tag_get_source_name(id, source);
	veejay_msg(VEEJAY_MSG_INFO,
		"Stream [%d]/[%d] [%s] %s ",
		id, vj_tag_size()-1, description,
		(vj_tag_get_active(id) == 1 ? "is active" : "is not active"));

	veejay_msg(VEEJAY_MSG_INFO,  "|-----------------------------------|");	
	for (i = 0; i < CLIP_MAX_EFFECTS; i++)
	{
		y = vj_tag_get_effect_any(id, i);
		if (y != -1) 
		{
			veejay_msg(VEEJAY_MSG_INFO, "%02d  [%d] [%s] %s (%s)",
				i,
				y,
				vj_tag_get_chain_status(id,i) ? "on" : "off", vj_effect_get_description(y),
				(vj_effect_get_subformat(y) == 1 ? "4:4:4" : "4:2:0")
			);

			for (j = 0; j < vj_effect_get_num_params(y); j++)
			{
				value = vj_tag_get_effect_arg(id, i, j);
				if (j == 0)
				{
		    			veejay_msg(VEEJAY_MSG_PRINT, "          [%04d]", value);
				}
				else
				{
		    			veejay_msg(VEEJAY_MSG_PRINT, " [%04d]",value);
				}
				
	    		}
	    		veejay_msg(VEEJAY_MSG_PRINT, "\n");

			if (vj_effect_get_extra_frame(y) == 1)
			{
				int source = vj_tag_get_chain_source(id, i);
				veejay_msg(VEEJAY_MSG_INFO, "     V %s [%d]",(source == VJ_TAG_TYPE_NONE ? "Clip" : "Stream"),
			    		clip_get_chain_channel(id,i)
					);
				//veejay_msg(VEEJAY_MSG_INFO, "     A: %s",   clip_get_chain_audio(id, i) ? "yes" : "no");
	    		}

	    		veejay_msg(VEEJAY_MSG_PRINT, "\n");
		}
    	}
	v->real_fps = -1;

}

void vj_event_create_effect_bundle(veejay_t * v, char *buf, int key_id )
{
	char blob[50 * CLIP_MAX_EFFECTS];
	char prefix[20];
	int i ,y,j;
	int num_cmd = 0;
	int id = v->uc->clip_id;
	int event_id = 0;
	int bunlen=0;
	bzero( prefix, 20);
	bzero( blob, (50 * CLIP_MAX_EFFECTS));
   
	if(!CLIP_PLAYING(v) && !TAG_PLAYING(v)) return;
	
 	for (i = 0; i < CLIP_MAX_EFFECTS; i++)
	{
		y = (CLIP_PLAYING(v) ? clip_get_effect_any(id, i) : vj_tag_get_effect_any(id,i) );
		if (y != -1)
		{
			num_cmd++;
		}
	}
	if(num_cmd < 0) return;

	for (i=0; i < CLIP_MAX_EFFECTS; i++)
	{
		y = (CLIP_PLAYING(v) ? clip_get_effect_any(id, i) : vj_tag_get_effect_any(id,i) );
		if( y != -1)
		{
			//int entry = i;
			int effect_id = y;
			if(effect_id != -1)
			{
				char bundle[200];
				int np = vj_effect_get_num_params(y);
				bzero(bundle,200);
				sprintf(bundle, "%03d:0 %d %d", NET_CHAIN_ENTRY_SET_PRESET,i, effect_id );
		    		for (j = 0; j < np; j++)
				{
					char svalue[10];
					int value = (CLIP_PLAYING(v) ? clip_get_effect_arg(id, i, j) : vj_tag_get_effect_arg(id,i,j));
					if(value != -1)
					{
						if(j == (np-1))
							sprintf(svalue, " %d;", value);
						else 
							sprintf(svalue, " %d", value);
						strncat( bundle, svalue, strlen(svalue));
					}
				}
				strncpy( blob+bunlen, bundle,strlen(bundle));
				bunlen += strlen(bundle);
			}
		}
 	}
	sprintf(prefix, "BUN:%03d{", num_cmd);
	sprintf(buf, "%s%s}",prefix,blob);
	event_id = vj_event_suggest_bundle_id();

	if(event_id > 0) 
	{
		vj_msg_bundle *m = vj_event_bundle_new( buf, event_id);
		if(m)
		{
			vj_event_bundle_store(m);
			sdl_parameter[key_id] = event_id;
			if( register_new_bundle_as_key( key_id, 3 ))
			{
				veejay_msg(VEEJAY_MSG_INFO, "Created Effect Template : press SHIFT - Key %d to trigger Bundle %d ", key_id,event_id);
			}
		}
		else
		{
			veejay_msg(VEEJAY_MSG_ERROR , "Unable to create new event %d", event_id);
		}
	}	
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR , "Invalid bundle contents ");
	}
}


void vj_event_print_clip_info(veejay_t *v, int id) 
{
	video_playback_setup *s = v->settings;
	int y, i, j;
	long value;
	char timecode[15];
	char curtime[15];
	MPEG_timecode_t tc;
	int start = clip_get_startFrame( id );
	int end = clip_get_endFrame( id );
	int len = end - start;

	mpeg_timecode(&tc, len,
 		mpeg_framerate_code(mpeg_conform_framerate(v->editlist->video_fps)),v->editlist->video_fps);
	sprintf(timecode, "%2d:%2.2d:%2.2d:%2.2d", tc.h, tc.m, tc.s, tc.f);

	mpeg_timecode(&tc, (end - s->current_frame_num),
		mpeg_framerate_code(mpeg_conform_framerate
		   (v->editlist->video_fps)),
	  	v->editlist->video_fps);

	sprintf(curtime, "%2d:%2.2d:%2.2d:%2.2d", tc.h, tc.m, tc.s, tc.f);

	veejay_msg(VEEJAY_MSG_INFO, 
		"Clip [%4d]/[%4d]\t[duration: %s | %8d ]",
		id,clip_size()-1,timecode,len);
	veejay_msg(VEEJAY_MSG_INFO, "                \t[timecode: %s | %8ld ]",
		curtime,(long)v->settings->current_frame_num);

    	veejay_msg(VEEJAY_MSG_INFO, 
		"|Start  |  End  | Speed | Looptype |");
	veejay_msg(VEEJAY_MSG_INFO, 
		"|%6d |%6d |%6d | %s |",
		start, end, clip_get_speed(id),
		(clip_get_looptype(id) ==
		2 ? "PingPong" : (clip_get_looptype(id)==1 ? "Normal  " : "None    ")  )
		);
	veejay_msg(VEEJAY_MSG_INFO,  "|-----------------------------------|");
    	for (i = 0; i < CLIP_MAX_EFFECTS; i++)
	{
		y = clip_get_effect_any(id, i);
		if (y != -1)
		{
			veejay_msg(VEEJAY_MSG_INFO, "%02d  [%d] [%s] %s (%s)",
				i,
				y,
				clip_get_chain_status(id,i) ? "on" : "off", vj_effect_get_description(y),
				(vj_effect_get_subformat(y) == 1 ? "4:4:4" : "4:2:0")
			);

	    		for (j = 0; j < vj_effect_get_num_params(y); j++)
			{
				value = clip_get_effect_arg(id, i, j);
				if (j == 0)
				{
		    			veejay_msg(VEEJAY_MSG_PRINT, "        [%04ld]", value);
				}
				else
				{
		    			veejay_msg(VEEJAY_MSG_PRINT, " [%04ld] ",value);
				}
			}
	    		veejay_msg(VEEJAY_MSG_PRINT,"\n");

	    		if (vj_effect_get_extra_frame(y) == 1)
			{
				int c = clip_get_selected_entry(id);
				int source = clip_get_chain_source(id, i);
						 
				veejay_msg(VEEJAY_MSG_INFO, "     V %s [%d]",(source == VJ_TAG_TYPE_NONE ? "Clip" : "Stream"),
			    		clip_get_chain_channel(id,i)
					);
				//veejay_msg(VEEJAY_MSG_INFO, "     A: %s",   clip_get_chain_audio(id, i) ? "yes" : "no");
	    		}

	    		veejay_msg(VEEJAY_MSG_PRINT, "\n");
		}
    	}
	v->real_fps = -1;
	veejay_msg(VEEJAY_MSG_PRINT, "\n");
}

void vj_event_print_plain_info(void *ptr, int x)
{
	veejay_t *v = (veejay_t*) ptr;
	v->real_fps = -1;	
}

void vj_event_print_info(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[1];
	char *str = NULL; P_A(args,str,format,ap);
	if(args[0]==0)
	{
		args[0] = v->uc->clip_id;
	}

	vj_event_print_plain_info(v,args[0]);

	if( CLIP_PLAYING(v) && clip_exists(args[0])  )
	{
		vj_event_print_clip_info( v, args[0] );
	}
	if( TAG_PLAYING(v) && vj_tag_exists(args[0]) )
	{
		vj_event_print_tag_info(v, args[0]) ;
	}
}


#define FORMAT_MSG(dst,str) sprintf(dst,"%03d%s",strlen(str),str)

#define APPEND_MSG(dst,str) strncat(dst,str,strlen(str))


#define SEND_MSG(v,str)\
{\
vj_server_send(v->uc->current_link, str, strlen(str));\
}


/*
#define SEND_MSG(v,str) veejay_msg(VEEJAY_MSG_INFO, "[%d][%s]",strlen(str),str)\


*/


void vj_event_init_gui_screen(void *ptr, const char format[], va_list ap)
{
	int args[2];
	char str[100];
	veejay_t *v = (veejay_t*) ptr;
	P_A(args,str,format,ap);

	if(v->gui_screen==1)
	{
	  veejay_msg(VEEJAY_MSG_ERROR,"Use this at initialization only");
	  return;
	}
	else
	{
		char msg[100];
		long id = 0;
		id =  atol( str );
		if(id <= 0)
		{
		 veejay_msg(VEEJAY_MSG_ERROR, "Invalid Window ID '%s' given",str);
		 return;
		}
		veejay_msg(VEEJAY_MSG_INFO, "Using Window ID %ld",id);
		sprintf(msg,"SDL_WINDOWID=%ld",id);
		putenv(msg);

		v->sdl_gui = (vj_sdl*) vj_sdl_allocate(
			v->editlist->video_width,v->editlist->video_height);

		if( !vj_sdl_init(v->sdl_gui,
			v->editlist->video_width,v->editlist->video_height,"veejay",1))
		{
			veejay_msg(VEEJAY_MSG_ERROR,
				"initializing SDL screen");
			if(v->sdl_gui) vj_sdl_free(v->sdl_gui);
			if(v->sdl_gui) free(v->sdl_gui);
			return;
		}	
		v->gui_screen = 1;
		windowID = id;
		if(args[0]==1)
		{
			char send_msg[30];
			veejay_msg(VEEJAY_MSG_INFO, "Initialized SDL screen. Informing GUI");
			sprintf(msg, "%ld %ld", v->editlist->video_width, v->editlist->video_height); 
			FORMAT_MSG(send_msg,msg);
			SEND_MSG(v,send_msg);
		}
	}
}

void	vj_event_send_tag_list			(	void *ptr,	const char format[],	va_list ap	)
{
	int args[1];
	int start_from_tag = 1;
	veejay_t *v = (veejay_t*)ptr;
	char *str = NULL; 
	P_A(args,str,format,ap);
	
	if(args[0]>0) start_from_tag = args[0];

	if( ((vj_tag_size()-1) <= 0) || (args[0] > vj_tag_size()-1))
	{
		/* there are no tags */
		veejay_msg(VEEJAY_MSG_ERROR, "No Streams (%d) or asking for non existing (%d)",
			vj_tag_size()-1,args[0]);
		SEND_MSG(v, "00000");
	}
	else
	{
		int i;
		int n = vj_tag_size()-1;
		if (n >= 1 )
		{
			char line[300];
			bzero( _print_buf, n * 300 );

			for(i=start_from_tag; i <= n; i++)
			{
				/* tag_id | source_type | source_name */
				if(vj_tag_exists(i))
				{	
					char source_name[255];
					char cmd[300];
					bzero(source_name,200);bzero(cmd,255);
					vj_tag_get_description( i, source_name );
					source_name[strlen(source_name)] = '\0';
					sprintf(line,"%-5d%03d%s", i, strlen(source_name),source_name);
					sprintf(cmd, "%03d%s",strlen(line),line);
					APPEND_MSG(_print_buf,cmd); 
				}
	
			}
			bzero( _s_print_buf, strlen(_print_buf) + 6 );
			sprintf(_s_print_buf, "%05d%s",strlen(_print_buf),_print_buf);
			SEND_MSG(v,_s_print_buf);
		}
		else
		{
			char stag_list[5];
			sprintf(stag_list,"%05d",0);
			SEND_MSG(v,stag_list);
		}
	}
}



void	vj_event_send_clip_list		(	void *ptr,	const char format[],	va_list ap	)
{
	veejay_t *v = (veejay_t*)ptr;
	int args[1];
	int start_from_clip = 1;
	char *str = NULL; P_A(args,str,format,ap);
	char cmd[300];
	if(args[0]>0) start_from_clip = args[0];

	if( ((clip_size()-1) <= 0) || (args[0] > clip_size()-1))
	{
		/* there are no tags */
		veejay_msg(VEEJAY_MSG_ERROR, "No Clips (%d) or asking for non existing (%d)",
			clip_size()-1,args[0]);
		SEND_MSG(v, "00000");
	}
	else
	{
		int i;
		int n = clip_size()-1;
		char line[308];
		bzero(_print_buf, SEND_BUF);
		for(i=start_from_clip; i <= n; i++)
		{
			/* clip_id | description */
			if(clip_exists(i))
			{	
				MPEG_timecode_t tc;
				char description[255];
				char timecode[20];
				int len = clip_get_endFrame(i) - clip_get_startFrame(i);
				bzero(cmd, 300);
				mpeg_timecode(&tc,len,mpeg_framerate_code(mpeg_conform_framerate(v->editlist->video_fps)),
					v->editlist->video_fps);

				sprintf(timecode,"%2.2d:%2.2d:%2.2d:%2.2d",tc.h,tc.m,tc.s,tc.f);

				clip_get_description( i, description );
				
				sprintf(cmd,"%-5d%s%03d%s",
					i,	
					timecode,
					strlen(description),
					description
				);
				FORMAT_MSG(line,cmd);
				APPEND_MSG(_print_buf,line); 
			}

		}
		sprintf(_s_print_buf, "%05d%s", strlen(_print_buf),_print_buf);
		SEND_MSG(v, _s_print_buf);
	}
}

void	vj_event_send_chain_entry		( 	void *ptr,	const char format[],	va_list ap	)
{
	char fline[255];
	char line[255];
	int args[2];
	char *str = NULL;
	veejay_t *v = (veejay_t*)ptr;
	P_A(args,str,format,ap);

	bzero(fline,255);

	if(args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}

	if(PLAIN_PLAYING(v))
	{
		
		sprintf(line, "%d %d %d %d %d %d %d %d %d %d %d %d",
			0,0,0,0,0,0,0,0,0,0,0,0);
		FORMAT_MSG(fline,line);	
		SEND_MSG(v,fline);
		return;	
	}
	if(CLIP_PLAYING(v))
	{
	int effect_id = clip_get_effect_any(args[0], args[1]);
	if(args[1]==-1) args[1] = clip_get_selected_entry(args[0]);
		if(args[1]>=CLIP_MAX_EFFECTS)
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Invalid chain entry %d was given",args[1]);
			return;
		}
		if(effect_id > 0)
		{
			int is_video = vj_effect_get_extra_frame(effect_id);
			int params[CLIP_MAX_PARAMETERS];
			int p;
			int video_on = clip_get_chain_status(args[0],args[1]);
			int audio_on = 0;
			//int audio_on = clip_get_chain_audio(args[0],args[1]);
			int num_params = vj_effect_get_num_params(effect_id);
			for(p = 0 ; p < num_params; p++)
			{
				params[p] = clip_get_effect_arg(args[0],args[1],p);
			}
			for(p = num_params; p < CLIP_MAX_PARAMETERS; p++)
			{
				params[p] = 0;
			}

			sprintf(line, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
				effect_id,
				is_video,
				num_params,
				params[0],
				params[1],	
				params[2],	
				params[3],
				params[4],		
				params[5],
				params[6],
				params[7],
				params[8],
				video_on,
				audio_on,
				clip_get_chain_source(args[0],args[1]),
				clip_get_chain_channel(args[0],args[1]) 
			);				
			
		}
		else
		{
			sprintf(line, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d",
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0 ,0);

		}

		FORMAT_MSG(fline,line);
		SEND_MSG(v, fline);
		return;
	}
	if(TAG_PLAYING(v))
	{

		int effect_id;
		if(args[1] == -1) args[1] = vj_tag_get_selected_entry(args[0]);
 		effect_id = vj_tag_get_effect_any(args[0], args[1]);

		if(effect_id > 0)
		{
			int is_video = vj_effect_get_extra_frame(effect_id);
			int params[CLIP_MAX_PARAMETERS];
			int p;
			int num_params = vj_effect_get_num_params(effect_id);
			int video_on = vj_tag_get_chain_status(args[0], args[1]);
			for(p = 0 ; p < num_params; p++)
			{
				params[p] = vj_tag_get_effect_arg(args[0],args[1],p);
			}
			for(p = num_params; p < CLIP_MAX_PARAMETERS;p++)
			{
				params[p] = 0;
			}

			sprintf(line, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
				effect_id,
				is_video,
				num_params,
				params[0],
				params[1],	
				params[2],	
				params[3],
				params[4],		
				params[5],
				params[6],
				params[7],
				params[8],
				video_on,	
				0,
				vj_tag_get_chain_source(args[0],args[1]),
				vj_tag_get_chain_channel(args[0],args[1])
			);				
			
		}
		else
		{
			sprintf(line, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0,0,0 ,0);

		}

		FORMAT_MSG(fline,line);
		SEND_MSG(v, fline);
		return;
	}
	
}

void	vj_event_send_chain_list		( 	void *ptr,	const char format[],	va_list ap	)
{
	int i;
	char line[18];
	char chain_list[CLIP_MAX_EFFECTS*18];
	char send_buf[CLIP_MAX_EFFECTS*18];
	int args[1];
	char *str = NULL;
	veejay_t *v = (veejay_t*)ptr;
	P_A(args,str,format,ap);
	bzero(send_buf,18*CLIP_MAX_EFFECTS);
	bzero(chain_list, 18*CLIP_MAX_EFFECTS);
	if(args[0] == 0) 
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if(CLIP_PLAYING(v))
	{
		for(i=0; i < CLIP_MAX_EFFECTS; i++)
		{
			int effect_id = clip_get_effect_any(args[0], i);
			if(effect_id > 0)
			{
				int is_video = vj_effect_get_extra_frame(effect_id);
				int using_effect = clip_get_chain_status(args[0], i);
				int using_audio = 0;
				//int using_audio = clip_get_chain_audio(args[0],i);
				sprintf(line,"%02d%03d%1d%1d%1d",
					i,
					effect_id,
					is_video,
					(using_effect <= 0  ? 0 : 1 ),
					(using_audio  <= 0  ? 0 : 1 )
				);
						
				APPEND_MSG(chain_list,line);
			}
		}
	}
	if(TAG_PLAYING(v))
	{
		for(i=0; i < CLIP_MAX_EFFECTS; i++) 
		{
			int effect_id = vj_tag_get_effect_any(args[0], i);
			if(effect_id > 0)
			{
				int is_video = vj_effect_get_extra_frame(effect_id);
				int using_effect = vj_tag_get_chain_status(args[0],i);
				sprintf(line, "%02d%03d%1d%1d%1d",
					i,
					effect_id,
					is_video,
					(using_effect <= 0  ? 0 : 1 ),
					0
				);
				APPEND_MSG(chain_list, line);
				
			}
		}
	}
	if(strlen(chain_list) == 0)
		{
			/* empty chain */	
			sprintf(chain_list, "%s","empty");
		}
	sprintf(send_buf, "%03d%s",strlen(chain_list), chain_list);
	SEND_MSG(v, send_buf);

	
}
void	vj_event_send_clip_history_list	(	void *ptr,	const char format[],	va_list ap	)
{
	int args[2];
	char *s = NULL;
	veejay_t *v = (veejay_t*) ptr;
	P_A(args,s,format,ap);

	if(args[0] == 0)
	{
		args[0] = v->uc->clip_id;
	}
	if(args[0] == -1) args[0] = clip_size()-1;

	if(clip_exists(args[0])) {
		clip_info *clip = clip_get(args[0]);
		int start = clip->first_frame[0];
		int end = clip->last_frame[0];
		char line[20];
		char history_list[20 * CLIP_MAX_RENDER];
		int i;
		bzero(history_list,(20 * CLIP_MAX_RENDER));
		for(i=0; i < CLIP_MAX_RENDER; i++)
		{
			bzero(line,20);
			if( clip->first_frame[i] != start &&
			    clip->last_frame[i] != end)
			{
				sprintf( line, "%02d%08ld%08ld",i, clip->first_frame[i],clip->last_frame[i]);
			}
			else
			{
				sprintf( line, "%02d%08d%08d", i, 0,0);
			}
			strncat(history_list, line, 18);
		}
		SEND_MSG(v, history_list);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Clip %d does not exists",args[0]);
	}
}
void 	vj_event_send_video_information		( 	void *ptr,	const char format[],	va_list ap	)
{
	/* send video properties */
	char info_msg[100];

	veejay_t *v = (veejay_t*)ptr;
	bzero(info_msg,100);
	sprintf(info_msg, "%04ld %04ld %01ld %01ld %02.3g %1d %04d %06ld %02d %03ld %08ld",
		v->editlist->video_width,
		v->editlist->video_height,
		v->editlist->video_inter,
		v->editlist->video_norm,
		v->editlist->video_fps,  
		v->editlist->has_audio,
		v->editlist->audio_bits,
		v->editlist->audio_rate,
		v->editlist->audio_chans,
		v->editlist->num_video_files,
		v->editlist->video_frames
		);	
	SEND_MSG(v,info_msg);
	
}
void 	vj_event_send_editlist			(	void *ptr,	const char format[],	va_list ap	)
{
	veejay_t *v = (veejay_t*) ptr;
	char timecode[20];
	char filename[255];
	MPEG_timecode_t tc;
	int n = v->editlist->num_video_files;
	char *editlist_msg = (char*)malloc(sizeof(char) * n * 300);
	char line[300];
	int i;
	if(!editlist_msg)
	{
		veejay_msg(VEEJAY_MSG_ERROR, vj_event_err(VJ_ERROR_MEM));
		return;
	}

	bzero(editlist_msg, (sizeof(char)*n*300));
	for(i=0; i < n; i++)
	{
		char cmd[300];
		int nframes = lav_video_frames(v->editlist->lav_fd[i]);

		sprintf(filename,"%3d%s", strlen( v->editlist->video_file_list[i] ), v->editlist->video_file_list[i] );	
		mpeg_timecode(&tc,nframes,mpeg_framerate_code(mpeg_conform_framerate(v->editlist->video_fps)),v->editlist->video_fps);
		sprintf(timecode, "%2.2d:%2.2d:%2.2d:%2.2d",tc.h,tc.m,tc.s,tc.f);
		sprintf(cmd, "%02d %s %s", i, filename, timecode);
		FORMAT_MSG(line,cmd);
		APPEND_MSG(editlist_msg,line);
	}
	SEND_MSG(v,editlist_msg);

	if(editlist_msg) free(editlist_msg);

}
void	vj_event_send_devices			(	void *ptr,	const char format[],	va_list ap	)
{
	char str[255];
	struct dirent **namelist;
	int n_dev = 0;
	int n;
	char device_list[512];
	char useable_devices[2];
	int *args = NULL;
	veejay_t *v = (veejay_t*)ptr;
	P_A(args,str,format,ap);
	bzero(device_list,512);

	n = scandir(str,&namelist,0,alphasort);
	if( n<= 0)
	{
		veejay_msg(VEEJAY_MSG_ERROR, "No device information in [%s]",str);
		SEND_MSG(v,"0000");
		return;
	}

		
	while(n--)
	{
		if( strncmp(namelist[n]->d_name, "video", 4)==0)
		{
			FILE *fd;
			char filename[300];
			sprintf(filename,"%s%s",str,namelist[n]->d_name);
			fd = fopen( filename, "r");
			if(fd)
			{
				char buf[4096];
				char result[255];
				char line[300];
				char *name,*arg=NULL;
				bzero(line,300);
				while(fgets(buf,4096,fd))
				{
					buf[strlen(buf)-1] = 0;
					name = strtok(buf, ":");
					arg = strtok(NULL, ":");
				}
				if( strcmp(arg, " 0x0") == 0)
				{
					switch( vj_vloopback_get_status( namelist[n]->d_name ))
					{
						case VLOOPBACK_FREE_PIPE:
							sprintf(result, "F%s", namelist[n]->d_name );
							n_dev ++;
							FORMAT_MSG( line,result );
							APPEND_MSG( device_list, line );	
							break;
						case VLOOPBACK_AVAIL_OUTPUT:
							sprintf(result, "W%s", namelist[n]->d_name);
							n_dev ++;
							FORMAT_MSG( line,result);
							APPEND_MSG( device_list, line );
							break;
					}
				}
				else
				{
					sprintf(result, "H%s", namelist[n]->d_name);
					FORMAT_MSG(line, result );
					APPEND_MSG( device_list, line );
					n_dev++;
				}
				fclose(fd);
			}
		}
	}
	sprintf(useable_devices,"%02d", n_dev);

	APPEND_MSG( device_list, useable_devices );
	SEND_MSG(v,device_list);

}
void	vj_event_send_effect_list		(	void *ptr,	const char format[],	va_list ap	)
{
	int i;
	veejay_t *v = (veejay_t*)ptr;
	char line[300];   
	char fline[300];
	bzero( _print_buf, SEND_BUF);
	bzero( _s_print_buf,SEND_BUF);

	for(i=1; i < MAX_EFFECTS; i++)
	{
		int effect_id = vj_effect_get_real_id(i);
		bzero(line, 300);
		if(effect_id > 0 && vj_effect_get_summary(i,line)==1)
		{
			sprintf(fline, "%03d%s",strlen(line),line);
			strcat( _print_buf, fline );

		}
		else
		{
			fprintf(stderr, "no matching effect for %d\n",i);	
			fprintf(stderr, "get summary returns with %d",
				vj_effect_get_summary(i,line));
		}
	}	
	sprintf( _s_print_buf, "%05d%s",strlen(_print_buf), _print_buf);

	SEND_MSG(v,_s_print_buf);
}



int vj_event_load_bundles(char *bundle_file)
{
	FILE *fd;
	char *event_name, *event_msg;
	char buf[65535];
	int event_id=0;
	if(!bundle_file) return -1;
	fd = fopen(bundle_file, "r");
	bzero(buf,65535);
	if(!fd) return -1;
	while(fgets(buf,4096,fd))
	{
		buf[strlen(buf)-1] = 0;
		event_name = strtok(buf, "|");
		event_msg = strtok(NULL, "|");
		if(event_msg!=NULL && event_name!=NULL) {
			//veejay_msg(VEEJAY_MSG_INFO, "Event: %s , Msg [%s]",event_name,event_msg);
			event_id = atoi( event_name );
			if(event_id && event_msg)
			{
				vj_msg_bundle *m = vj_event_bundle_new( event_msg, event_id );
				if(m != NULL) 
				{
					if( vj_event_bundle_store(m) == 0) 
					{
						veejay_msg(VEEJAY_MSG_INFO, "(VIMS) Registered a bundle as event %03d",event_id);
					}
				}
			}
		}
	}
	fclose(fd);
	return 1;
}

void vj_event_do_bundled_msg(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*) ptr;
	int args[1];
	char s[1024];	
	vj_msg_bundle *m;
	P_A( args, s , format, ap);
	//veejay_msg(VEEJAY_MSG_INFO, "Parsing message bundle as event");
	m = vj_event_bundle_get(args[0]);
	if(m)
	{
		vj_event_parse_bundle( v, m->bundle );
	}	
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Requested event %d does not exist. ",args[0]);
	}
}

void vj_event_attach_key_to_bundle(void *ptr, const char format[], va_list ap)
{
	int args[3];
	char s[1024];
	vj_msg_bundle *m;
	P_A(args,s,format,ap);
	// args[0] = custom event id, args[1] = sdl key , args[2] = sdl modifier
	m = vj_event_bundle_get(args[0]);
	if(m) 
	{
		if ( args[1] >= 0 && args[1] <= MAX_SDL_KEY )
		{
			sdl_parameter[(args[1])] = args[0];
			if(register_new_bundle_as_key( args[1], args[2] ))
			{
				veejay_msg(VEEJAY_MSG_INFO,"Attached SDL key %d to bundle id %d with modifier type %d",
					args[1],args[0],args[2]);
			}
			else 
			{
				veejay_msg(VEEJAY_MSG_INFO, "Invalid Key %d or Modifier %d. Range is %d-%d, %d-%d", args[1],args[2],0,MAX_SDL_KEY, 0,3);
			}
		}
		else {
			veejay_msg(VEEJAY_MSG_ERROR,"Invalid Key %d ",args[1]);
		}
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Bundle %d does not exist.\n",args[0]);
	}
}

void vj_event_bundled_msg_del(void *ptr, const char format[], va_list ap)
{
	
	int args[1];	
	char *s = NULL;
	P_A(args,s,format,ap);
	if ( vj_event_bundle_del( args[0] ) == 0)
	{
		veejay_msg(VEEJAY_MSG_INFO,"Bundle %d deleted from event system",args[0]);
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Bundle is %d is not known",args[0]);
	}
}




void vj_event_bundled_msg_add(void *ptr, const char format[], va_list ap)
{
	
	int args[1];
	char s[1024];
	P_A(args,s,format,ap);
	if(args[0] > 5000 && args[0] < 6000)
	{
		if(!vj_event_bundle_exists(args[0]))
		{
			vj_msg_bundle *m;
			veejay_strrep( s, '_' , ' ');
			m = vj_event_bundle_new( s, args[0] );
			if(m != NULL ) 
			{
				if( vj_event_bundle_store(m) == 0) 
				{
					veejay_msg(VEEJAY_MSG_INFO, "(VIMS) Registered Bundle %d in VIMS",args[0]);
				}
			}
			else 
			{
				veejay_msg(VEEJAY_MSG_ERROR, "Unable to add bundle %d to VIMS", args[0]);
			}
		}
		else 
		{
			veejay_msg(VEEJAY_MSG_ERROR, "Bundle %d already exists", args[0]);
		}
	}
	else
	{
		veejay_msg(VEEJAY_MSG_ERROR, "Events %d-%d is reserved for VIMS", 5001,5999);
	}
}

void vj_event_screenshot(void *ptr, const char format[], va_list ap)
{
	veejay_t *v = (veejay_t*)ptr;
	v->uc->hackme = 1;
}

