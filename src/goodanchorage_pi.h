

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

#define GOODANCHORAGE_TOOL_POSITION    -1          // Request default positioning of toolbar tool



class MyMarkerType
{
public:

	MyMarkerType(void);
    ~MyMarkerType(void);
	
	PlugIn_Waypoint *pluginWaypoint;
	
	double serverLat;
	double serverLon;
	bool serverDeep;
	int serverId;
	
	wxString serverTitle;
	wxString serverPath;
	
	wxString getMarkerTitle(void);
};

const int LOGIN_BUTTON_ID = 101;

class CustomDialog : public wxDialog
{
public:

	int              m_leftclick_tool_id;
	
	wxTextCtrl *loginTextCtrl;
	wxTextCtrl *passwordTextCtrl;

	CustomDialog(const wxString& title,wxWindow* parent);
	void onQuit(wxCommandEvent & event);
	void onLogin(wxCommandEvent & event);
	
	void setM_leftclick_tool_id(int m_leftclick_tool_id);
	bool sendRequestAuth(wxString login, wxString password);
};

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
	  wxString sendRequestPlus(int id);
	  
	
	  
	  bool isPlugInActive;
	  PlugIn_Waypoint *m_ActiveMarker;
	  MyMarkerType *m_ActiveMyMarker;
	  
	  bool PointInLLBox( PlugIn_ViewPort *vp, double x, double y );
	  
    void initLoginDialog(wxWindow* parent);
	
	void cleanMarkerList(void);
	void showMarkerList(void);
	void setServerAuthHeaders(wxHTTP &httpObj);

	CustomDialog *loginDialog;
private:
	bool m_isIconPressed;
	wxString m_PluginDir;

	bool _initPluginDir(void);
	bool _initDb(void);
	bool _initAuthFile(void);
	void _storeMarkerDb(MyMarkerType);
	void _storeMarkerJsonDb(int, wxString);
	wxString _parseMarkerJson(wxString);
	void _loadMarkersDb();
	wxString _loadMarkerDetailsDb(int);
};

extern "C" DECL_EXP  wxString getErrorText(int errorID,int codeID);




#endif
