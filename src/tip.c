/*****************************************************************************
 * tip.c: Translate it please VLC module
 *****************************************************************************
 * Copyright (C) 2019 Aleksey Koltakov
 *
 * Authors: Aleksey Koltakov <aleksey.koltakov@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
/* Internationalization */
#define DOMAIN  "vlc-tip-plugin"
#define _(str)  dgettext(DOMAIN, str)
#define N_(str) (str)

#define KEY_COMMA           0x0000002c
#define KEY_DOT             0x0000002e
#define KEY_SLASH           0x0000002f
#define KEY_SHIFT_QUESTION  0x0200003f
 
#include <stdlib.h>
/* VLC core API headers */
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_input.h>

/*****************************************************************************
 * Forward declarations
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

static int  KeyboardEvent   ( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static int  PlaylistEvent   ( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static int  InputEvent      ( vlc_object_t *, char const *,
                              vlc_value_t, vlc_value_t, void * );
static void TimerEvent      ( void * );

static void ChangeAudioTrack( input_thread_t *, int64_t );
static void ChangeInput     ( intf_thread_t *, input_thread_t * );
static void ChangeVout      ( intf_thread_t *, vout_thread_t * );

const char* MAIN_AUDIOTRACK_NUM = "tip-main_audiotrack";
const char* TRANSLATED_AUDIOTRACK_NUM = "tip-translated_audiotrack";
const char* TIME_TO_TRANSLATE = "tip-time-to-translate";

#define DisplayMessage( vout, ... ) \
    vout_OSDText( vout, VOUT_SPU_CHANNEL_OSD, SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_LEFT,  __VA_ARGS__ );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_text_domain( DOMAIN )
    set_shortname( N_("TIP") )
    set_description( N_("TIP (translate it, please)") )
    set_capability( "interface", 0 )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    add_integer( MAIN_AUDIOTRACK_NUM, 1, N_("Main audio track"), N_("The main audio track number"), false )
    add_integer( TRANSLATED_AUDIOTRACK_NUM, 2, N_("Auxiliary audio track (for translation)"), N_("The auxiliary audio track number (for translation)"), false )
    add_integer( TIME_TO_TRANSLATE, 5, N_("Period of time for translation (in seconds)"), N_("The period of time for play again with a translated audio track (in seconds)"), false )
    set_callbacks( Open, Close )
vlc_module_end ()
 
/*****************************************************************************
 * intf_sys_t: internal state for an instance of the module
 *****************************************************************************/
struct intf_sys_t
{
    vlc_mutex_t      lock;
    vlc_timer_t      timer;
    
    int64_t          i_start_time;
    int64_t          i_end_time;
    
    vout_thread_t   *p_vout;
    input_thread_t  *p_input;
};

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    
    /* Allocate internal state */
    intf_sys_t *p_sys = malloc( sizeof( intf_sys_t ) );
    if ( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;
    
    p_intf->p_sys = p_sys;
    p_sys->i_start_time = -1;
    p_sys->i_end_time = -1;
    p_sys->p_vout = NULL;
    p_sys->p_input = NULL;
    vlc_mutex_init( &p_sys->lock );
    if ( vlc_timer_create( &p_sys->timer, TimerEvent, p_intf ) )
    {
        vlc_mutex_destroy( &p_sys->lock );
        free( p_sys );
        return VLC_EGENERIC;
    }

    var_AddCallback( p_intf->obj.libvlc, "key-pressed", KeyboardEvent, p_intf );
    var_AddCallback( pl_Get(p_intf), "input-current", PlaylistEvent, p_intf );
 
    msg_Info( p_intf, "the module is loaded" );
    
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;
    
    var_DelCallback( pl_Get(p_intf), "input-current", PlaylistEvent, p_intf );
    var_DelCallback( p_intf->obj.libvlc, "key-pressed", KeyboardEvent, p_intf );

    ChangeInput( p_intf, NULL );

    /* Free internal state */
    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys );
    
    msg_Info( p_intf, "the module is unloaded" );
}

/*****************************************************************************
 * KeyboardEvent: callback for processing keyboard events
 *****************************************************************************/
static int KeyboardEvent( vlc_object_t *libvlc, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED( psz_var );
    VLC_UNUSED( oldval );
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    intf_sys_t *p_sys = p_intf->p_sys;
    
    msg_Dbg( libvlc, "key pressed: 0x%" PRIx64, newval.i_int );
    
    vlc_mutex_lock( &p_intf->p_sys->lock );
    input_thread_t *p_input = p_sys->p_input ? vlc_object_hold( p_sys->p_input )
                                             : NULL;
    vout_thread_t *p_vout = p_sys->p_vout ?    vlc_object_hold( p_sys->p_vout )
                                             : NULL;
    vlc_mutex_unlock( &p_intf->p_sys->lock );
    
    switch ( newval.i_int )
    {
        /* translate the last few seconds */
        case KEY_SLASH:
        {
            if( p_input && var_GetBool( p_input, "can-seek" ) )
            {
                /* change a language */
                int64_t i_num = var_InheritInteger( p_input, TRANSLATED_AUDIOTRACK_NUM );
                ChangeAudioTrack( p_input, i_num );
                /* move backward */
                int64_t i_time = var_InheritInteger( p_input, TIME_TO_TRANSLATE );
                msg_Dbg( libvlc, "move backward %" PRId64 " sec", i_time );
            
                p_sys->i_end_time = var_GetInteger( p_input, "time" );
                p_sys->i_start_time = p_sys->i_end_time - i_time * CLOCK_FREQ;
                var_SetInteger( p_input, "time", p_sys->i_start_time );
                DisplayMessage(p_vout, p_sys->i_end_time - p_sys->i_start_time, "TIP: translate");
                vlc_timer_schedule( p_sys->timer, false, i_time * CLOCK_FREQ, 0 );
            }
            break;
        }
        /* repeat the last translated period */    
        case KEY_SHIFT_QUESTION:
        {
            if ( p_sys->i_start_time >= 0 && p_input && var_GetBool(p_input, "can-seek") )
            {
                int64_t i_num = var_InheritInteger( p_input, MAIN_AUDIOTRACK_NUM );
                ChangeAudioTrack( p_input, i_num );
                var_SetInteger( p_input, "time", p_sys->i_start_time );
                DisplayMessage( p_vout, p_sys->i_end_time - p_sys->i_start_time, "TIP: repeat" );
                break;
            }
        }
    }
    
    if( p_input != NULL )
        vlc_object_release( p_input );
    if( p_vout != NULL )
        vlc_object_release( p_vout );
    
    return VLC_SUCCESS;
}

/*****************************************************************************
 * PlaylistEvent: callback for processing playlist events
 *****************************************************************************/
static int PlaylistEvent( vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED( p_this ); 
    VLC_UNUSED( psz_var ); 
    VLC_UNUSED( oldval );
    intf_thread_t *p_intf = p_data;

    ChangeInput( p_intf, newval.p_address );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * InputEvent: callback for processing input events
 *****************************************************************************/
static int InputEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    intf_thread_t *p_intf = p_data;
    VLC_UNUSED( psz_var );
    VLC_UNUSED( oldval );

    if( newval.i_int == INPUT_EVENT_VOUT )
        ChangeVout( p_intf, input_GetVout( p_input ) );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * TimerEvent: callback for processing timer events
 *****************************************************************************/
static void TimerEvent(void *p_data) {
    intf_thread_t *p_intf = (intf_thread_t *) p_data;
    intf_sys_t *p_sys = p_intf->p_sys;

    msg_Dbg( p_intf, "timer event" );
    int64_t i_num = var_InheritInteger( p_sys->p_input, MAIN_AUDIOTRACK_NUM );
    ChangeAudioTrack( p_sys->p_input, i_num );
}

static void ChangeAudioTrack( input_thread_t* p_input, int64_t i_num ) {
    msg_Dbg( p_input, "change an audio track to #%" PRId64, i_num );
    vlc_value_t audio_tracks;
    var_Change( p_input, "audio-es", VLC_VAR_GETCHOICES, &audio_tracks, NULL );

    int i_count = audio_tracks.p_list->i_count;
    if (i_num > 0 && i_num < i_count) {
        var_Set( p_input, "audio-es", audio_tracks.p_list->p_values[i_num] );
    } else
        msg_Err( p_input, "wrong audio track number, the value should be an integer from 1 to %d", i_count - 1 );

    var_FreeList( &audio_tracks, NULL );
}

static void ChangeInput( intf_thread_t *p_intf, input_thread_t *p_input )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    input_thread_t *p_old_input = p_sys->p_input;
    vout_thread_t *p_old_vout = NULL;
    if( p_old_input != NULL )
    {
        /* First, remove callbacks from previous input. 
         * It's safe to access it unlocked, since it's written from this thread */
        var_DelCallback( p_old_input, "intf-event", InputEvent, p_intf );
        p_old_vout = p_sys->p_vout;
    }

    /* Replace input and vout locked */
    vlc_mutex_lock( &p_sys->lock );
    p_sys->p_input = p_input ? vlc_object_hold( p_input ) : NULL;
    p_sys->p_vout = NULL;
    vlc_mutex_unlock( &p_sys->lock );

    /* Release old input and vout objects unlocked */
    if( p_old_input != NULL )
    {
        if( p_old_vout != NULL )
            vlc_object_release( p_old_vout );
        vlc_object_release( p_old_input );
    }

    /* Register input events */
    if( p_input != NULL )
        var_AddCallback( p_input, "intf-event", InputEvent, p_intf );
}

static void ChangeVout( intf_thread_t *p_intf, vout_thread_t *p_vout )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    vout_thread_t *p_old_vout = p_sys->p_vout;
    p_sys->p_vout = p_vout;
    vlc_mutex_unlock( &p_sys->lock );

    if( p_old_vout != NULL )
    {
        vlc_object_release( p_old_vout );
    }
}
