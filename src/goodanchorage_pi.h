/***************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  GoodAnchorage PlugIn
 *
 ***************************************************************************
 *   Copyright (C) 2015 by GoodAnchorage.com                               *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 **************************************************************************/

#ifndef _GOODANCHORAGEPI_H_
#define _GOODANCHORAGEPI_H_

#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
  #include <wx/glcanvas.h>
#endif //precompiled headers

#include "version.h"

#define     MY_API_VERSION_MAJOR    1
#define     MY_API_VERSION_MINOR    12

#include "ocpn_plugin.h"


//----------------------------------------------------------------------------------------------------------
//    The PlugIn Class Definition
//----------------------------------------------------------------------------------------------------------

#define GOODANCHORAGE_TOOL_POSITION    -1          // Request default positioning of toolbar tool



class GAMarker
{
public:

	GAMarker(void);
    ~GAMarker(void);
	
	PlugIn_Waypoint *pluginWaypoint;
	
	double serverLat;
	double serverLon;
	bool serverDeep;
	int serverId;
	
	wxString serverTitle;
	wxString serverPath;
	
	wxString getMarkerTitle(void);
};

class CustomDialog;

class goodanchorage_pi : public opencpn_plugin_112
{
public:
      goodanchorage_pi(void *ppimgr);
      ~goodanchorage_pi(void);

//    The required PlugIn Methods
      int Init(void);
      bool DeInit(void);

      int GetAPIVersionMajor();
      int GetAPIVersionMinor();
      int GetPlugInVersionMajor();
      int GetPlugInVersionMinor();
      wxBitmap *GetPlugInBitmap();
      wxString GetCommonName();
      wxString GetShortDescription();
      wxString GetLongDescription();

//    The override PlugIn Methods
      bool MouseEventHook( wxMouseEvent &event);
      bool RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp);
      void SetCursorLatLon(double lat, double lon);
      void OnContextMenuItemCallback(int id);
      void SetPluginMessage(wxString &message_id, wxString &message_body);
      bool RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp);
      void SendTimelineMessage(wxDateTime time);
      void SetDefaults(void);
      int GetToolbarToolCount(void);
      void ShowPreferencesDialog( wxWindow* parent );
      void OnToolbarToolCallback(int id);
	  
	  
	  wxFileConfig     *m_pconfig;
      wxWindow         *m_parent_window;
	  int              m_leftclick_tool_id;
	  PlugIn_ViewPort  m_vp;
	  
	  bool sendRequest(double lat,double lon);
	  bool sendRequestPlus(int id, GAMarker *marker, wxString &details);
	  
	  PlugIn_Waypoint *m_ActiveMarker;
	  GAMarker *m_ActiveGAMarker;
	  
	  bool PointInLLBox( PlugIn_ViewPort *vp, double x, double y );
	  
    void initLoginDialog(wxWindow* parent);
	
	void cleanMarkerList(void);
	void showMarkerList(void);
	void setServerAuthHeaders(wxHTTP &httpObj);

	CustomDialog *loginDialog;

        enum GoodAnchorageState { DISABLED, OFFLINE, BUSY, ONLINE } m_state;
        void SetState(enum GoodAnchorageState state);

private:

	wxString m_PluginDir;

	bool _initPluginDir(void);
	bool _initDb(void);
	bool _initAuthFile(void);
	void _storeMarkerDb(GAMarker);
	void _storeMarkerJsonDb(int, wxString);
	wxString _parseMarkerJson(wxString, GAMarker *);
	void _loadMarkersDb();
	bool _loadMarkerDetailsDb(int, GAMarker *marker, wxString &details);
};

const int LOGIN_BUTTON_ID = 101;

class CustomDialog : public wxDialog
{
public:

	int              m_leftclick_tool_id;
	
	wxTextCtrl *loginTextCtrl;
	wxTextCtrl *passwordTextCtrl;

	CustomDialog(const wxString& title, goodanchorage_pi &pi, wxWindow* parent);
	void onQuit(wxCommandEvent & event);
	void onLogin(wxCommandEvent & event);
	
	void setM_leftclick_tool_id(int m_leftclick_tool_id);
	bool sendRequestAuth(wxString login, wxString password);

private:
        goodanchorage_pi &m_goodanchorage_pi;
};

#ifdef _MSC_VER
    #pragma warning(disable : 4190)
#endif
extern "C" DECL_EXP  wxString getErrorText(int errorID,int codeID);


#endif
