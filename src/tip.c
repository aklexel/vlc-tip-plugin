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

static void ChangeInput     ( intf_thread_t *, input_thread_t * );
static void ChangeVout      ( intf_thread_t *, vout_thread_t * );
static void SetVarByValueIndex( input_thread_t *, char const *, int64_t );

const char* TRANSLATE_AUDIOTRACK_NUM = "tip-translate_audiotrack";
const char* TRANSLATE_SUBTRACK_NUM = "tip-translate-subtrack";
const char* REPEAT_SUBTRACK_NUM = "tip-repeat-subtrack";
const char* TIME_TO_TRANSLATE = "tip-time-to-translate";

#define ChangeSubTrack( p_input, ... ) \
    SetVarByValueIndex( p_input, "spu-es",  __VA_ARGS__ );
#define ChangeAudioTrack( p_input, ... ) \
    SetVarByValueIndex( p_input, "audio-es",  __VA_ARGS__ );
#define DisplayMessage( vout, ... ) \
    vout_OSDText( vout, VOUT_SPU_CHANNEL_OSD, SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_LEFT,  __VA_ARGS__ );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_text_domain( DOMAIN )
    set_shortname( N_("TIP") )
    set_description( N_("TIP (translate it, please)") )
    set_help( N_("<sup><b>v1.1.0</b></sup><br/>The plugin adds new keyboard shortcuts to VLC:<ul><li>&nbsp;&lt;<b>/</b>&gt; for translation the specified period of video</li><li>&nbsp;&lt;<b>Shift</b> + <b>/</b>&gt; for repeat the last translated part of video</li></ul>") )
    set_capability( "interface", 0 )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )

    add_integer( TIME_TO_TRANSLATE, 5, N_("Period of time for translation/repeat (in seconds)"), N_("The period of time for play again with a translated audio track (in seconds)"), false )

    set_section( N_("Translation settings"), NULL )
    add_integer( TRANSLATE_AUDIOTRACK_NUM, 1, N_("Audio track"), N_("The audio track number for translation (set -1 to disable audio track switch)"), false )
    add_integer( TRANSLATE_SUBTRACK_NUM, 1, N_("Subtitle track"), N_("The subtitle track number for translation (set -1 to disable subtitle switch)"), false )

    set_section( N_("Repeat settings"), NULL )
    add_integer( REPEAT_SUBTRACK_NUM, 2, N_("Subtitle track"), N_("The subtitle track number for repeat (set -1 to disable subtitle switch)"), false )

    set_callbacks( Open, Close )
vlc_module_end ()
 
/*****************************************************************************
 * intf_sys_t: internal state for an instance of the module
 *****************************************************************************/
struct intf_sys_t
{
    vlc_mutex_t      lock;
    vlc_timer_t      timer;
    
    vlc_value_t origin_audiotrack;
    vlc_value_t origin_subtrack;
    bool is_origin;
    
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
    p_sys->origin_audiotrack.i_int = -1;
    p_sys->origin_subtrack.i_int = -1;
    p_sys->is_origin = true;
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
    msg_Dbg( libvlc, "key pressed: 0x%" PRIx64, newval.i_int );
    if ( newval.i_int != KEY_SLASH && newval.i_int != KEY_SHIFT_QUESTION )
        return VLC_SUCCESS;

    VLC_UNUSED( psz_var );
    VLC_UNUSED( oldval );
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    intf_sys_t *p_sys = p_intf->p_sys;
    
    vlc_mutex_lock( &p_sys->lock );
    input_thread_t *p_input = p_sys->p_input ? vlc_object_hold( p_sys->p_input )
                                             : NULL;
    vout_thread_t *p_vout = p_sys->p_vout ?    vlc_object_hold( p_sys->p_vout )
                                             : NULL;
    vlc_mutex_unlock( &p_sys->lock );

    if ( p_input && p_vout && var_GetBool( p_input, "can-seek" ) )
    {
        int64_t i_time = var_InheritInteger( p_input, TIME_TO_TRANSLATE );
        int64_t i_num;
        char *tip = NULL;

        if ( p_sys->is_origin )
        {
            var_Get( p_input, "audio-es", &p_sys->origin_audiotrack );
            var_Get( p_input, "spu-es", &p_sys->origin_subtrack );
            p_sys->is_origin = false;
        }

        switch ( newval.i_int )
        {
        /* translate the past few seconds */
        case KEY_SLASH:
            i_num = var_InheritInteger( p_input, TRANSLATE_SUBTRACK_NUM );
            ChangeSubTrack( p_input, i_num );

            i_num = var_InheritInteger( p_input, TRANSLATE_AUDIOTRACK_NUM );
            ChangeAudioTrack( p_input, i_num );

            p_sys->i_end_time = var_GetInteger( p_input, "time" );
            p_sys->i_start_time = p_sys->i_end_time - i_time * CLOCK_FREQ;
            tip = "TIP: translate";
            break;
        /* repeat the last translated period */
        case KEY_SHIFT_QUESTION:
            i_num = var_InheritInteger( p_input, REPEAT_SUBTRACK_NUM );
            ChangeSubTrack( p_input, i_num );

            var_Set( p_input, "audio-es", p_sys->origin_audiotrack );
            tip = "TIP: repeat";
            break;
        }

        msg_Dbg( libvlc, "move backward %" PRId64 " sec", i_time );
        var_SetInteger( p_input, "time", p_sys->i_start_time );
        DisplayMessage( p_vout, p_sys->i_end_time - p_sys->i_start_time, tip );
        vlc_timer_schedule( p_sys->timer, false, i_time * CLOCK_FREQ, 0 );
    }
    
    if ( p_input != NULL )
        vlc_object_release( p_input );
    if ( p_vout != NULL )
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
    p_intf->p_sys->i_start_time = 0;
    p_intf->p_sys->is_origin = true;

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

    if ( newval.i_int == INPUT_EVENT_VOUT )
        ChangeVout( p_intf, input_GetVout( p_input ) );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * TimerEvent: callback for processing timer events
 *****************************************************************************/
static void TimerEvent( void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    intf_sys_t *p_sys = p_intf->p_sys;

    msg_Dbg( p_intf, "timer event" );
    var_Set( p_sys->p_input, "spu-es", p_sys->origin_subtrack );
    var_Set( p_sys->p_input, "audio-es", p_sys->origin_audiotrack );
    p_sys->is_origin = true;
}

static void SetVarByValueIndex( input_thread_t* p_input, char const *psz_name,
                                int64_t i_index )
{
    msg_Dbg( p_input, "change %s to #%" PRId64, psz_name, i_index );
    if ( i_index < 0 )
        return;

    vlc_value_t list;
    var_Change( p_input, psz_name, VLC_VAR_GETCHOICES, &list, NULL );
    int i_count = list.p_list->i_count;

    if ( i_index < i_count )
        var_Set( p_input, psz_name, list.p_list->p_values[i_index] );
    else
        msg_Err( p_input, "%s value should be < %d", psz_name, i_count );

    var_FreeList( &list, NULL );
}

static void ChangeInput( intf_thread_t *p_intf, input_thread_t *p_input )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    input_thread_t *p_old_input = p_sys->p_input;
    vout_thread_t *p_old_vout = NULL;
    if ( p_old_input != NULL )
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
    if ( p_old_input != NULL )
    {
        if ( p_old_vout != NULL )
            vlc_object_release( p_old_vout );
        vlc_object_release( p_old_input );
    }

    /* Register input events */
    if ( p_input != NULL )
        var_AddCallback( p_input, "intf-event", InputEvent, p_intf );
}

static void ChangeVout( intf_thread_t *p_intf, vout_thread_t *p_vout )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    vlc_mutex_lock( &p_sys->lock );
    vout_thread_t *p_old_vout = p_sys->p_vout;
    p_sys->p_vout = p_vout;
    vlc_mutex_unlock( &p_sys->lock );

    if ( p_old_vout != NULL )
    {
        vlc_object_release( p_old_vout );
    }
}
