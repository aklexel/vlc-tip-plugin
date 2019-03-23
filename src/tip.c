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
 
#include <stdlib.h>
/* VLC core API headers */
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
/* Internationalization */
#define DOMAIN  "vlc-tip-plugin"
#define _(str)  dgettext(DOMAIN, str)
#define N_(str) (str)
 
/*****************************************************************************
 * Forward declarations
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );
 
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_text_domain( DOMAIN )
    set_shortname( N_("TIP") )
    set_description( N_("Translate it, please") )
    set_capability( "interface", 0 )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    add_string( "tip-param", "1", "Parameter", "Some parameter value.", false )
    set_callbacks( Open, Close )
vlc_module_end ()
 
/*****************************************************************************
 * intf_sys_t: internal state for an instance of the module
 *****************************************************************************/
struct intf_sys_t
{
    char *param;
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
 
    /* Read settings */
    char *param = var_InheritString( p_intf, "tip-param" );
    if ( param == NULL )
    {
        msg_Err( p_intf, "param is not set" );
    }
    p_sys->param = param;
 
    msg_Info( p_intf, "TIP module is loaded with param=%s", param );
    
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;
    
    msg_Info( p_intf, "The module (param=%s) is unloaded", p_sys->param );

    /* Free internal state */
    free( p_sys->param );
    free( p_sys );
}
