

#ifndef _MYPLUGIN_H_
#define _MYPLUGIN_H_

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

#define MY_PLUGIN_TOOL_POSITION    -1          // Request default positioning of toolbar tool



class MyMarkerType
{
public:

	MyMarkerType(void);
    ~MyMarkerType(void);
	
	PlugIn_Waypoint *pluginWaitPoint;
	
	double serverLat;
	double serverLon;
	bool serverDeep;
	int serverId;
	
	wxString serverTitle;
	
	wxString getMarkerTitle(void);
};


class my_plugin_pi : public opencpn_plugin_112
{
public:
      my_plugin_pi(void *ppimgr);
      ~my_plugin_pi(void);

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
	  
	  void sendRequest(double lat,double lon);
	  void sendRequestPlus(int id);
	  
	
	  
	  bool isPlugInActive;
	  PlugIn_Waypoint *m_ActiveMarker;
	  MyMarkerType *m_ActiveMyMarker;
	  
	  bool PointInLLBox( PlugIn_ViewPort *vp, double x, double y );
	  
    void initLoginDialog(wxWindow* parent);
	
	void cleanMarkerList(void);
	void showMarkerList(void);
private:
	bool _initDb(void);
};




#endif
