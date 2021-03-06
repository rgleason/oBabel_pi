/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  obabel Plugin
 * Author:   David Register, Mike Rossiter
 *
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.             *
 ***************************************************************************
 */


#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
  #include <wx/glcanvas.h>
#endif //precompiled headers

#include <wx/fileconf.h>
#include <wx/stdpaths.h>

#include "obabel_pi.h"

// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr)
{
    return new obabel_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p)
{
    delete p;
}

//---------------------------------------------------------------------------------------------------------
//
//    obabel PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------

#include "icons.h"


//---------------------------------------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//---------------------------------------------------------------------------------------------------------

obabel_pi::obabel_pi(void *ppimgr)
      :opencpn_plugin_17(ppimgr)
{
      // Create the PlugIn icons
      initialize_images();
      m_bShowobabel = false;
	  
}

obabel_pi::~obabel_pi(void)
{
      delete _img_babel_pi;
      delete _img_babel;
}

int obabel_pi::Init(void)
{
        
	  AddLocaleCatalog( _T("opencpn-obabel_pi") );

      // Set some default private member parameters
      m_obabel_dialog_x = 0;
      m_obabel_dialog_y = 0;
      m_obabel_dialog_sx = 200;
      m_obabel_dialog_sy = 400;
      m_pobabelDialog = NULL;
      m_pobabelOverlayFactory = NULL;

      ::wxDisplaySize(&m_display_width, &m_display_height);

      //    Get a pointer to the opencpn configuration object
      m_pconfig = GetOCPNConfigObject();
	 
      //    And load the configuration items
      LoadConfig();

      // Get a pointer to the opencpn display canvas, to use as a parent for the obabel dialog
      m_parent_window = GetOCPNCanvasWindow();

      //    This PlugIn needs a toolbar icon, so request its insertion if enabled locally
    //  if(m_bobabelShowIcon)
          m_leftclick_tool_id = InsertPlugInTool(_T(""), _img_babel, _img_babel, wxITEM_CHECK,
                                                 _("obabel"), _T(""), NULL,
                                                 obabel_TOOL_POSITION, 0, this);

      return (WANTS_OVERLAY_CALLBACK |
              WANTS_OPENGL_OVERLAY_CALLBACK |
              WANTS_CURSOR_LATLON       |
              WANTS_TOOLBAR_CALLBACK    |
              INSTALLS_TOOLBAR_TOOL     |
              WANTS_CONFIG             
              //WANTS_PLUGIN_MESSAGING
            );
}

bool obabel_pi::DeInit(void)
{
    if(m_pobabelDialog) { 



		m_pobabelDialog->Close();
        delete m_pobabelDialog;
        m_pobabelDialog = NULL;
    }

	
    
	delete m_pobabelOverlayFactory;
    m_pobabelOverlayFactory = NULL;

    return true;
}

int obabel_pi::GetAPIVersionMajor()
{
      return MY_API_VERSION_MAJOR;
}

int obabel_pi::GetAPIVersionMinor()
{
      return MY_API_VERSION_MINOR;
}

int obabel_pi::GetPlugInVersionMajor()
{
      return PLUGIN_VERSION_MAJOR;
}

int obabel_pi::GetPlugInVersionMinor()
{
      return PLUGIN_VERSION_MINOR;
}

wxBitmap *obabel_pi::GetPlugInBitmap()
{
      return _img_babel_pi;
}

wxString obabel_pi::GetCommonName()
{
      return _T("obabel");
}


wxString obabel_pi::GetShortDescription()
{
      return _("obabel PlugIn for OpenCPN");
}


wxString obabel_pi::GetLongDescription()
{
      return _("obabel PlugIn for OpenCPN \nFor uploading KML/GPX routes and waypoints to Chartplotters.\n\n\
			   ");
}

int obabel_pi::GetToolbarToolCount(void)
{
      return 1;
}

void obabel_pi::OnToolbarToolCallback(int id)
{
    if(!m_pobabelDialog)
    {
		        		
		m_pobabelDialog = new obabelUIDialog(m_parent_window, this);
        wxPoint p = wxPoint(m_obabel_dialog_x, m_obabel_dialog_y);
        m_pobabelDialog->Move(0,0);        // workaround for gtk autocentre dialog behavior
        m_pobabelDialog->Move(p);

        // Create the drawing factory
        m_pobabelOverlayFactory = new obabelOverlayFactory( *m_pobabelDialog );
        m_pobabelOverlayFactory->SetParentSize( m_display_width, m_display_height);		
        
    }

      // Qualify the obabel dialog position
            bool b_reset_pos = false;

#ifdef __WXMSW__
        //  Support MultiMonitor setups which an allow negative window positions.
        //  If the requested window does not intersect any installed monitor,
        //  then default to simple primary monitor positioning.
            RECT frame_title_rect;
            frame_title_rect.left =   m_obabel_dialog_x;
            frame_title_rect.top =    m_obabel_dialog_y;
            frame_title_rect.right =  m_obabel_dialog_x + m_obabel_dialog_sx;
            frame_title_rect.bottom = m_obabel_dialog_y + 30;


            if(NULL == MonitorFromRect(&frame_title_rect, MONITOR_DEFAULTTONULL))
                  b_reset_pos = true;
#else
       //    Make sure drag bar (title bar) of window on Client Area of screen, with a little slop...
            wxRect window_title_rect;                    // conservative estimate
            window_title_rect.x = m_obabel_dialog_x;
            window_title_rect.y = m_obabel_dialog_y;
            window_title_rect.width = m_obabel_dialog_sx;
            window_title_rect.height = 30;

            wxRect ClientRect = wxGetClientDisplayRect();
            ClientRect.Deflate(60, 60);      // Prevent the new window from being too close to the edge
            if(!ClientRect.Intersects(window_title_rect))
                  b_reset_pos = true;

#endif

            if(b_reset_pos)
            {
                  m_obabel_dialog_x = 20;
                  m_obabel_dialog_y = 170;
                  m_obabel_dialog_sx = 300;
                  m_obabel_dialog_sy = 540;
            }

      //Toggle obabel overlay display
      m_bShowobabel = !m_bShowobabel;

      //    Toggle dialog?
      if(m_bShowobabel) {
          m_pobabelDialog->Show();
      } 
	  else {
         m_pobabelDialog->Hide();         
      }

      // Toggle is handled by the toolbar but we must keep plugin manager b_toggle updated
      // to actual status to ensure correct status upon toolbar rebuild
      SetToolbarItemState( m_leftclick_tool_id, m_bShowobabel );
      RequestRefresh(m_parent_window); // refresh main window
}

void obabel_pi::OnobabelDialogClose()
{
     
	m_bShowobabel = false;	
    SetToolbarItemState( m_leftclick_tool_id, m_bShowobabel );
    m_pobabelDialog->Hide();
    SaveConfig();

    RequestRefresh(m_parent_window); // refresh main window

}

bool obabel_pi::RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp)
{
    if(!m_pobabelDialog ||
       !m_pobabelDialog->IsShown() ||
       !m_pobabelOverlayFactory)
        return false;

    m_pobabelDialog->SetViewPort( vp );
    m_pobabelOverlayFactory->RenderobabelOverlay ( dc, vp );
    return true;
}

bool obabel_pi::RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp)
{
    if(!m_pobabelDialog ||
       !m_pobabelDialog->IsShown() ||
       !m_pobabelOverlayFactory)
        return false;

    m_pobabelDialog->SetViewPort( vp );
    m_pobabelOverlayFactory->RenderGLobabelOverlay ( pcontext, vp );
    return true;
}
void obabel_pi::SetCursorLatLon(double lat, double lon)
{
    if(m_pobabelDialog)
        m_pobabelDialog->SetCursorLatLon(lat, lon);
}

bool obabel_pi::LoadConfig(void)
{
    wxFileConfig *pConf = (wxFileConfig *)m_pconfig;

    if(!pConf)
        return false;

    pConf->SetPath ( _T( "/PlugIns/obabel" ) );

	pConf->Read ( _T ( "obabelformat" ),&m_GetFormat,wxEmptyString);
	pConf->Read ( _T ( "obabeldevice" ),&m_GetDevice,wxEmptyString);
	pConf->Read ( _T ( "obabelexe" ),&m_GetExe,wxEmptyString);

    m_obabel_dialog_sx = pConf->Read ( _T ( "obabelDialogSizeX" ), 300L );
    m_obabel_dialog_sy = pConf->Read ( _T ( "obabelDialogSizeY" ), 540L );
    m_obabel_dialog_x =  pConf->Read ( _T ( "obabelDialogPosX" ), 20L );
    m_obabel_dialog_y =  pConf->Read ( _T ( "obabelDialogPosY" ), 170L );


	
    return true;
}

bool obabel_pi::SaveConfig(void)
{
    wxFileConfig *pConf = (wxFileConfig *)m_pconfig;

    if(!pConf)
        return false;

    pConf->SetPath ( _T( "/PlugIns/obabel" ) );
    pConf->Write ( _T ( "obabelformat" ), m_GetFormat );
    pConf->Write ( _T ( "obabeldevice" ), m_GetDevice);
	pConf->Write ( _T ( "obabelexe" ), m_GetExe );

    pConf->Write ( _T ( "obabelDialogSizeX" ),  m_obabel_dialog_sx );
    pConf->Write ( _T ( "obabelDialogSizeY" ),  m_obabel_dialog_sy );
    pConf->Write ( _T ( "obabelDialogPosX" ),   m_obabel_dialog_x );
    pConf->Write ( _T ( "obabelDialogPosY" ),   m_obabel_dialog_y );

	
    return true;
}

void obabel_pi::SetColorScheme(PI_ColorScheme cs)
{
    DimeWindow(m_pobabelDialog);
}

