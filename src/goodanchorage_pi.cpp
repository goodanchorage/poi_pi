
#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
  #ifdef ocpnUSE_GL
      #include <wx/glcanvas.h>
  #endif
#endif //precompiled headers

#include <wx/fileconf.h>
#include <wx/stdpaths.h>

////////////////////////////////////////////////
#include <wx/sstream.h>
#include <wx/protocol/http.h>
#include <wx/jsonreader.h>
#include <wx/textfile.h>
#include <wx/wfstream.h>
#include <wx/txtstrm.h>
#include <wx/utils.h> 
#include <wx/hyperlink.h> 
//////////////////////////////////////////

#include "goodanchorage_pi.h"
#include <map>
#include <string>
#include <vector>

// DB related
#include <wx/dir.h>
#include <wx/filename.h>
#include <sqlite/sqlite3.h>

#include "icons.h"

#include <wx/listimpl.cpp>
WX_DEFINE_LIST (Plugin_HyperlinkList);

std::vector<GAMarker> markersList;	// TODO: convert to wxlist
sqlite3 *gaDb;
wxTextFile *gaAuthFile;
bool isOnline;

//---------------------------------------------------------------------------------------------------------
//
//    GoodAnchorage PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------


// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr) {
    return new goodanchorage_pi(ppimgr);
}


extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p) {
    delete p;
}


wxWindow        *m_parent_window;


//---------------------------------------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//---------------------------------------------------------------------------------------------------------

goodanchorage_pi::goodanchorage_pi(void *ppimgr)
    :opencpn_plugin_112(ppimgr)
{
      // Create the PlugIn icons
      initialize_images();
	  isPlugInActive = false;
}


goodanchorage_pi::~goodanchorage_pi(void)
{
	  delete _img_ga_anchor;
      delete _img_ga_toolbar;
	  delete _img_ga_toolbar_on;
}


int goodanchorage_pi::Init(void)
{
	isPlugInActive = false;
	m_isIconPressed = false;
	m_ActiveMarker = NULL;
	m_ActiveMyMarker = NULL;
	// Get a pointer to the opencpn display canvas
    m_parent_window = GetOCPNCanvasWindow();
	initLoginDialog(m_parent_window);

	AddLocaleCatalog( _T("opencpn-goodanchorage_pi") );

	// Get a pointer to the opencpn configuration object
	m_pconfig = GetOCPNConfigObject();

	// Get a pointer to the opencpn display canvas, to use as a parent for the GoodAnchorage dialog
	m_parent_window = GetOCPNCanvasWindow();
	  
	AddCustomWaypointIcon(_img_ga_anchor, _T("_img_ga_anchor"), _T("Good Anchorage"));

	// This PlugIn needs a toolbar icon, so request its insertion if enabled locally
	m_leftclick_tool_id = InsertPlugInTool(_T(""), _img_ga_toolbar, _img_ga_toolbar, wxITEM_CHECK,
						_("GoodAnchorage"), _T(""), NULL, GOODANCHORAGE_TOOL_POSITION, 0, this);
	if (!_initPluginDir()) {
		wxMessageBox(_T("Error locating plugin data directory.\nGoodAnchorage plugin will not run properly."),
					_T("GoodAnchorage Plugin"), wxICON_ERROR);
	} else if (!_initDb()) {
		wxMessageBox(_T("Error opening local data store.\nGoodAnchorage plugin will run in ONLINE mode only."),
					_T("GoodAnchorage Plugin"), wxICON_ERROR);
	}
	if (!_initAuthFile()) {
		wxMessageBox(_T("Error creating authentication file.\nGoodAnchorage plugin will not run properly."),
					_T("GoodAnchorage Plugin"), wxICON_ERROR);
	}

	return (WANTS_OVERLAY_CALLBACK |
			WANTS_OPENGL_OVERLAY_CALLBACK |
			WANTS_CURSOR_LATLON       |
			WANTS_TOOLBAR_CALLBACK    |
			INSTALLS_TOOLBAR_TOOL     |
			WANTS_CONFIG              |
			//WANTS_PREFERENCES         |
			WANTS_PLUGIN_MESSAGING    |
			WANTS_MOUSE_EVENTS
		);
}


bool goodanchorage_pi::_initPluginDir(void) {
	wxString ga_dir = *GetpPrivateApplicationDataLocation();
	ga_dir.Append(wxFileName::GetPathSeparator());
	ga_dir.Append(_T("plugins"));
	ga_dir.Append(wxFileName::GetPathSeparator());
	ga_dir.Append(_T("goodanchorage"));
	//wxMessageBox(_T("Looking for GA data directory in ") + ga_dir);
	if (!wxDir::Exists(ga_dir) && !wxFileName::Mkdir(ga_dir, 0755, wxPATH_MKDIR_FULL)) {
		wxLogMessage(_T("Failed to create GA data directory in ") + ga_dir);
		return false;
	}

	m_PluginDir = ga_dir;
	return true;
}


bool goodanchorage_pi::_initDb(void) {
	if (m_PluginDir.IsNull()) return false;

	wxString db_file = m_PluginDir + wxFileName::GetPathSeparator() + _T("ga_data.db");
	wxLogMessage(db_file);

	int rc = sqlite3_open_v2(db_file.utf8_str(), &gaDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
	if (rc) {
		wxString errmsg;
		errmsg.Printf(wxT("goodanchorage_pi: sqlite3_open failed: %d"), rc);
		wxLogMessage(errmsg);
		return false;
	} else {
		wxLogMessage(_T("goodanchorage_pi: sqlite3_open success"));

		char *errMsg;
		wxString _errMsg;
		char const *sql = "SELECT id FROM anchor_point LIMIT 1";
		rc = sqlite3_exec(gaDb, sql, NULL, NULL, &errMsg);
		if (rc != SQLITE_OK && errMsg != NULL) {
			_errMsg = wxString::FromUTF8(errMsg);
			sqlite3_free(errMsg);
			if (_errMsg.Left(13) == _T("no such table")) {
				sql =	"CREATE TABLE anchor_point (id integer primary key not null, "
				"lat real not null, lon real not null, is_deep int not null, title text not null, "
				"path text, json text, updated integer);"
				"CREATE INDEX ix_anchor_point_updated ON anchor_point(updated);";
				// json: results of anchor_point call
				// updated: seconds from 1970 -- wxGetUTCTime()
				rc = sqlite3_exec(gaDb, sql, NULL, NULL, &errMsg);
				if (rc != SQLITE_OK) {
					if (errMsg != NULL) {
						_errMsg = wxString::FromUTF8(errMsg);
						sqlite3_free(errMsg);
						wxMessageBox(_T("Failed to create local storage: ") + _errMsg);
					}
					return false;
				}
			}
		}
	}

	return true;
}



bool goodanchorage_pi::_initAuthFile(void) {
	if (m_PluginDir.IsNull()) { return false; }
	wxString fname = m_PluginDir + wxFileName::GetPathSeparator() + _T("ga_auth.txt");

	gaAuthFile = new wxTextFile( fname );
	if( ! gaAuthFile->Exists() ) {
		if (!gaAuthFile->Create()) return false;
	}

	return true;
}


bool goodanchorage_pi::DeInit(void)
{
    cleanMarkerList();
	sqlite3_close(gaDb);
	gaAuthFile->Close();
	RemovePlugInTool(m_leftclick_tool_id);
    return true;
}


int goodanchorage_pi::GetAPIVersionMajor()
{
      return MY_API_VERSION_MAJOR;
}


int goodanchorage_pi::GetAPIVersionMinor()
{
      return MY_API_VERSION_MINOR;
}


int goodanchorage_pi::GetPlugInVersionMajor()
{
      return PLUGIN_VERSION_MAJOR;
}


int goodanchorage_pi::GetPlugInVersionMinor()
{
      return PLUGIN_VERSION_MINOR;
}


wxBitmap *goodanchorage_pi::GetPlugInBitmap()
{
      return _img_ga_toolbar;
}


wxString goodanchorage_pi::GetCommonName()
{
      return _T("GoodAnchorage");
}


wxString goodanchorage_pi::GetShortDescription()
{
      return _("GoodAnchorage PlugIn for OpenCPN");
}

wxString goodanchorage_pi::GetLongDescription()
{
      return _("GoodAnchorage PlugIn for OpenCPN\n\n\
Provides access to crowdsourced anchorages around the world.\n\n\
- Initial data download requires a working network connection and\n\
  a free GoodAnchorage.com account.\n\
- Afterwards, locally stored data can be used whenever offline.\n\
- To view a list of anchorages for an area activate toolbar icon\n\
  and DOUBLE-CLICK on the map.\n\
- To view anchorage details RIGHT-CLICK on a marker and select Properties.\n\n\
Your feedback is welcome! http://GoodAnchorage.com/contact");
}


void goodanchorage_pi::SetDefaults(void)
{
}


int goodanchorage_pi::GetToolbarToolCount(void)
{
      return 1;
}


bool goodanchorage_pi::MouseEventHook( wxMouseEvent &event )
{
	if(!isPlugInActive)
		return false;

	// load multiple markers -- coordinates only
	if( event.LeftDClick() && m_ActiveMarker == NULL )
	{
		double plat,plon;

		wxBeginBusyCursor();
        GetCanvasLLPix( &m_vp, event.GetPosition(), &plat, &plon );
		//wxMessageBox( wxString::Format(wxT("%f"),plat) + _T(" ") + wxString::Format(wxT("%f"),plon));
		if (isOnline) {
			if (!sendRequest(plat,plon)) {
				isOnline = false;
				wxMessageBox(_T("Switching to OFFLINE mode.\nClick on the map to load locally stored data."));
			}
		} else {
			_loadMarkersDb();
			showMarkerList();
		}
		wxEndBusyCursor();
		
		return true;	// stop propagation of the event -- don't zoom/move the map.
	}
	// load marker details before showing the dialog
	else if (event.RightDown () && m_ActiveMarker != NULL )
	{
		wxBeginBusyCursor();
		wxString details;
		if (isOnline) {
			details = sendRequestPlus(m_ActiveMyMarker->serverId);
			if (details.IsNull()) {
				details = wxEmptyString;
				isOnline = false;
				wxMessageBox(_T("Switching to OFFLINE mode."));
			}
		} else {
			details = _loadMarkerDetailsDb(m_ActiveMyMarker->serverId);
			if (details.IsNull()) {
				details = _T("No locally stored data available for this anchorage.");
			}
		}
		// TODO: Crashes somewhere here. Null pointer?
		// Looks like either m_ActiveMyMarker is not found fast enough or 
		// network request takes too long to load between the clicks.
		// Works much better with right-click than double-click.
		if (m_ActiveMyMarker) {
			m_ActiveMyMarker->pluginWaypoint->m_MarkDescription = details;
			UpdateSingleWaypoint(m_ActiveMyMarker->pluginWaypoint);
		}
		wxEndBusyCursor();
			
		return false;	// allow event propagation -- we want to see marker dialog
	}
	else
		return false;
}


void goodanchorage_pi::ShowPreferencesDialog( wxWindow* parent )
{
    
}


void goodanchorage_pi::OnToolbarToolCallback(int id)
{
	// a hack to overcome unappealing icon shifting done by OCPN by default
	m_isIconPressed = !m_isIconPressed;
	wxBitmap *toolbar_icon = m_isIconPressed ?  _img_ga_toolbar_on : _img_ga_toolbar;
	SetToolbarToolBitmaps(m_leftclick_tool_id, toolbar_icon, toolbar_icon);

	if(isPlugInActive)
	{
		cleanMarkerList();
		isPlugInActive = false;
		return;
	}
	else
	{
		// check network first
		wxHTTP get;
		get.SetHeader(_T("Content-type"), _T("text/html; charset=utf-8"));
		get.SetTimeout(10);
		while (!get.Connect(_T("api.goodanchorage.com")))
			wxSleep(5);
	 
		isOnline = wxApp::IsMainLoopRunning(); // should return true
		get.Close();

		if (isOnline && gaAuthFile) {
			// login box if no auth info in the auth file
			gaAuthFile->Open();
			if( gaAuthFile->Eof() )
			{	
				gaAuthFile->Close();
				loginDialog->ShowModal();
			}
		}

		isPlugInActive = true;
	}
	
    RequestRefresh(m_parent_window); // refresh main window
}


bool goodanchorage_pi::RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp)
{
    m_vp = *vp;
    return true;
}


bool goodanchorage_pi::RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp)
{
    m_vp = *vp;
    return true;
}


void goodanchorage_pi::SetCursorLatLon(double lat, double lon)
{
	if(!isPlugInActive) return;
	
	wxPoint cur;
    GetCanvasPixLL( &m_vp, &cur, lat, lon );

	// TODO: review -- this may not be fast enough. Maybe because of the loop,
	// or perhaps because of frequency of calls to SetCursorLatLon. In either case,
	// the active marker takes a while to be found.
	bool changeMarkerFlag = false;
	for (std::vector<GAMarker>::iterator it = markersList.begin() ; it != markersList.end(); ++it)
	{
		PlugIn_Waypoint *marker = it->pluginWaypoint;
		
		if(  PointInLLBox( &m_vp,  marker->m_lon, marker->m_lat ) ) // !!!!!!!!!!!!!!! x == lon  and y = lat !!!!!!!!
		{
			//wxMessageBox(wxString::Format(wxT("%f"),marker->m_lat) + _T(" ") +wxString::Format(wxT("%f"),marker->m_lon));
			wxPoint pl;
            GetCanvasPixLL( &m_vp, &pl, marker->m_lat, marker->m_lon );
			// TODO: there is likely to be a problem here. Two markers nearby or overlapping
			// prevent one of them from being selected
            if (pl.x > cur.x - 10 && pl.x < cur.x + 10 && pl.y > cur.y - 10 && pl.y < cur.y + 10)
            {
				if(m_ActiveMyMarker != NULL)
				{
					m_ActiveMyMarker->pluginWaypoint->m_MarkName = _T("");
					UpdateSingleWaypoint(m_ActiveMyMarker->pluginWaypoint);
				}
				
                m_ActiveMarker = marker;
				m_ActiveMyMarker = &(*it);
                changeMarkerFlag = true;
				
				m_ActiveMyMarker->pluginWaypoint->m_MarkName = m_ActiveMyMarker->getMarkerTitle();
				UpdateSingleWaypoint(m_ActiveMyMarker->pluginWaypoint);
				
                break;
            }
		}
	}
	
	if(	!changeMarkerFlag	)
	{
		if(m_ActiveMyMarker != NULL)
		{
			m_ActiveMyMarker->pluginWaypoint->m_MarkName = _T("");
			UpdateSingleWaypoint(m_ActiveMyMarker->pluginWaypoint);
		}
	
		m_ActiveMarker = NULL;
		m_ActiveMyMarker = NULL;
	}
}


bool goodanchorage_pi::PointInLLBox( PlugIn_ViewPort *vp, double x, double y )
{
    double Marge = 0.;
    double m_minx = vp->lon_min;
    double m_maxx = vp->lon_max;
    double m_miny = vp->lat_min;
    double m_maxy = vp->lat_max;

    //   Box is centered in East lon, crossing IDL
    if(m_maxx > 180.)
    {
        if( x < m_maxx - 360.)
            x +=  360.;

        if (  x >= (m_minx - Marge) && x <= (m_maxx + Marge) &&
            y >= (m_miny - Marge) && y <= (m_maxy + Marge) )
            return TRUE;
        return FALSE;
    }

    //    Box is centered in Wlon, crossing IDL
    else if(m_minx < -180.)
    {
        if(x > m_minx + 360.)
            x -= 360.;

        if (  x >= (m_minx - Marge) && x <= (m_maxx + Marge) &&
            y >= (m_miny - Marge) && y <= (m_maxy + Marge) )
            return TRUE;
        return FALSE;
    }

    else
    {
        if (  x >= (m_minx - Marge) && x <= (m_maxx + Marge) &&
            y >= (m_miny - Marge) && y <= (m_maxy + Marge) )
            return TRUE;
        return FALSE;
    }
	/*
	else
    {
        if (  x >= (m_minx - Marge) && x <= (m_maxx + Marge) &&
            y >= (m_miny - Marge) && y <= (m_maxy + Marge) )
            return TRUE;
        return FALSE;
    }
	*/
}


void goodanchorage_pi::OnContextMenuItemCallback(int id)
{
   
}


void goodanchorage_pi::SetPluginMessage(wxString &message_id, wxString &message_body)
{
    
}


void goodanchorage_pi::SendTimelineMessage(wxDateTime time)
{
   
}


bool goodanchorage_pi::sendRequest(double lat,double lon){
	bool isLoaded;

	cleanMarkerList();

	wxHTTP get;
	get.SetHeader(_T("Content-type"), _T("text/html; charset=utf-8"));
	get.SetTimeout(10); // 10 seconds of timeout instead of 10 minutes ...
	setServerAuthHeaders(get);
	 
	// this will wait until the user connects to the internet. It is important in case of dialup (or ADSL) connections
	while (!get.Connect(_T("api.goodanchorage.com")))  // only the server, no pages here yet ...
		wxSleep(5);
	 
	wxApp::IsMainLoopRunning(); // should return true
	 
	 //goodanchorage.com/api/v1/anchor_list/
	// use _T("/") for index.html, index.php, default.asp, etc.
	wxInputStream *httpStream = get.GetInputStream( _T("/api/v1/anchor_list/") + wxString::Format(wxT("%f"),lat) + _T("/") + wxString::Format(wxT("%f.json"),lon) );
	 
	// wxLogVerbose( wxString(_T(" GetInputStream: ")) << get.GetResponse() << _T("-") << ((resStream)? _T("OK ") : _T("FAILURE ")) << get.GetError() );
	 
	if (get.GetError() == wxPROTO_NOERR)
	{
		wxString res;
		wxStringOutputStream out_stream(&res);
		httpStream->Read(out_stream);

		wxJSONValue  root;
		wxJSONReader reader;

        // now read the JSON text and store it in the 'root' structure
        // check for errors before retreiving values...
		int numErrors = reader.Parse( res, &root );
		if ( numErrors > 0 || !root.IsArray() )  {
			wxMessageBox(_T("!root.IsArray() ") + res);
			return false;
		}

		for ( int i = 0; i < root.Size(); i++ )  {
			wxJSONValue lat_lon = root[i][_T("lat_lon")];	 
			if ( !lat_lon.IsArray() ) {
				wxMessageBox(res);	
				continue;
			}

			bool is_deep =  root[i][_T("is_deep")].AsBool();
			int id = root[i][_T("id")].AsInt();
			wxString title = root[i][_T("title")].AsString();
			wxString path = root[i][_T("path")].AsString();

			double lat_i = lat_lon[0].AsDouble();
			double lon_i = lat_lon[1].AsDouble();

			GAMarker newMarker;
			newMarker.serverDeep = is_deep;
			newMarker.serverId = id;
			newMarker.serverTitle = root[i][_T("title")].AsString();
			newMarker.serverPath = root[i][_T("path")].AsString();
			newMarker.serverLat = lat_i;
			newMarker.serverLon = lon_i;
				
			PlugIn_Waypoint *bufWayPoint = new PlugIn_Waypoint( lat_i, lon_i,
                _T("_img_ga_anchor"), _T("") ,
                    GetNewGUID()  );
			//bufWayPoint->m_MarkName = newMarker.getMarkerTitle();	// too messy, don't load here
			if (!newMarker.serverPath.IsNull()) {
				Plugin_Hyperlink *plink = new Plugin_Hyperlink;
				plink->DescrText = _T("Satellite & Ground Photos, 72 Hours Forecast, Reviews");
				plink->Link = _T("http://www.goodanchorage.com") + newMarker.serverPath;
				plink->Type = wxEmptyString;
				bufWayPoint->m_HyperlinkList = new Plugin_HyperlinkList;
				bufWayPoint->m_HyperlinkList->Insert(plink);
			}
			bufWayPoint->m_MarkDescription = 
							wxT("Right click on the marker again to load details.\n")
							wxT("Slow network may delay data loading.");
			newMarker.pluginWaypoint = bufWayPoint;
				
			markersList.push_back(newMarker);
			_storeMarkerDb(newMarker);
		}

		showMarkerList();
		isLoaded = true;
	}
	else
	{
		wxMessageBox( _T("Unable to connect: ") +
			getErrorText(get.GetError(),get.GetResponse()) +
			_T("\n") +
			wxString::Format(wxT("(Error %d, "),get.GetError())+
			wxString::Format(wxT("HTTP %d)"),get.GetResponse()));

		// TODO: catch 401/403, clear auth data nad prompt to login
		// otherwise, stale auth data sits in the file and prevents access
		isLoaded = false;
	}
	 
	wxDELETE(httpStream);
	get.Close();

	return isLoaded;
}


void goodanchorage_pi::_storeMarkerDb(GAMarker marker) {
	// insert or update while keeping existing values: json and updated
	char const *sql = "INSERT OR REPLACE INTO anchor_point (id, lat, lon, is_deep, title, path, json, updated) "
			"VALUES (?, ?, ?, ?, ?, ?, "
			"(SELECT json FROM anchor_point WHERE id = ?), "
			"(SELECT updated FROM anchor_point WHERE id = ?));";
	sqlite3_stmt *stmt;
	if ( sqlite3_prepare(gaDb, sql, -1, &stmt, 0) != SQLITE_OK) {
		wxMessageBox(_T("Failed prepare to store Anchorage in local storage"));
		sqlite3_finalize(stmt);
		return;
	}

	if (sqlite3_bind_int(stmt, 1, marker.serverId) != SQLITE_OK) {
		wxMessageBox(_T("Failed to bind ID in local storage"));
		sqlite3_finalize(stmt);
		return;
	}
	if (sqlite3_bind_double(stmt, 2, marker.serverLat) != SQLITE_OK) {
		wxMessageBox(_T("Failed to bind Latitude in local storage"));
		sqlite3_finalize(stmt);
		return;
	}
	if (sqlite3_bind_double(stmt, 3, marker.serverLon) != SQLITE_OK) {
		wxMessageBox(_T("Failed to bind Longitude in local storage"));
		sqlite3_finalize(stmt);
		return;
	}
	if (sqlite3_bind_int(stmt, 4, marker.serverDeep) != SQLITE_OK) {
		wxMessageBox(_T("Failed to bind IsDeep in local storage"));
		sqlite3_finalize(stmt);
		return;
	}
	if (sqlite3_bind_text(stmt, 5, strdup(marker.serverTitle.utf8_str()), -1, SQLITE_STATIC) != SQLITE_OK) {
		wxMessageBox(_T("Failed to bind Title in local storage"));
		sqlite3_finalize(stmt);
		return;
	}
	if (sqlite3_bind_text(stmt, 6, strdup(marker.serverPath.utf8_str()), -1, SQLITE_STATIC) != SQLITE_OK) {
		wxMessageBox(_T("Failed to bind Path in local storage"));
		sqlite3_finalize(stmt);
		return;
	}
	if (sqlite3_bind_int(stmt, 7, marker.serverId) != SQLITE_OK) {
		wxMessageBox(_T("Failed to bind ID in local storage"));
		sqlite3_finalize(stmt);
		return;
	}
	if (sqlite3_bind_int(stmt, 8, marker.serverId) != SQLITE_OK) {
		wxMessageBox(_T("Failed to bind ID in local storage"));
		sqlite3_finalize(stmt);
		return;
	}
	if (sqlite3_step(stmt) != SQLITE_DONE) {
		wxMessageBox(_T("Failed to save Anchorage in local storage"));
		sqlite3_finalize(stmt);
		return;
	}
	sqlite3_finalize(stmt);
	return;
}


void goodanchorage_pi::_loadMarkersDb() {
	// load them all -- don't worry about lat/lon for this version
	char const *sql = "SELECT id, lat, lon, is_deep, title, path FROM anchor_point;";
	sqlite3_stmt *stmt;
	int rc = sqlite3_prepare_v2(gaDb, sql, -1, &stmt, 0);
	if( rc != SQLITE_OK ){
      wxMessageBox(wxString::Format(wxT("Failed to fetch data: %s"), sqlite3_errmsg(gaDb)));
	}

    while (sqlite3_step(stmt) == SQLITE_ROW) {
		GAMarker newMarker;
		newMarker.serverId = sqlite3_column_int(stmt, 0);
		newMarker.serverLat = sqlite3_column_double(stmt, 1);
		newMarker.serverLon = sqlite3_column_double(stmt, 2);
		newMarker.serverDeep = sqlite3_column_int(stmt, 3) != 0;
		newMarker.serverTitle = wxString(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)), wxConvUTF8);
		newMarker.serverPath = wxString(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)), wxConvUTF8);
		PlugIn_Waypoint *bufWayPoint = new PlugIn_Waypoint( newMarker.serverLat, 
				newMarker.serverLon, _T("_img_ga_anchor"), _T(""), GetNewGUID()  );
		if (!newMarker.serverPath.IsNull()) {
			Plugin_Hyperlink *plink = new Plugin_Hyperlink;
			plink->DescrText = _T("Satellite & Ground Photos, 72 Hours Forecast, Reviews");
			plink->Link = _T("http://www.goodanchorage.com") + newMarker.serverPath;
			plink->Type = wxEmptyString;
			bufWayPoint->m_HyperlinkList = new Plugin_HyperlinkList;
			bufWayPoint->m_HyperlinkList->Insert(plink);
		}

		newMarker.pluginWaypoint = bufWayPoint;
		markersList.push_back(newMarker);
    }
	sqlite3_finalize(stmt);

	return;
}


wxString goodanchorage_pi::sendRequestPlus(int id){
	wxString details;
	wxHTTP get;
	get.SetHeader(_T("Content-type"), _T("text/html; charset=utf-8"));
	get.SetTimeout(10); // 10 seconds of timeout instead of 10 minutes ...
	setServerAuthHeaders(get);
	 
	// this will wait until the user connects to the internet. It is important in case of dialup (or ADSL) connections
	while (!get.Connect(_T("api.goodanchorage.com")))  // only the server, no pages here yet ...
		wxSleep(5);
	 
	wxApp::IsMainLoopRunning(); // should return true
	 
	//goodanchorage.com/api/v1/anchor_list/
	// use _T("/") for index.html, index.php, default.asp, etc.
	wxInputStream *httpStream = get.GetInputStream( _T("/api/v1/anchor_point/") + wxString::Format(wxT("%d.json"),id)  );
	 
	// wxLogVerbose( wxString(_T(" GetInputStream: ")) << get.GetResponse() << _T("-") << ((resStream)? _T("OK ") : _T("FAILURE ")) << get.GetError() );
	 
	if (get.GetError() == wxPROTO_NOERR)
	{
		wxString json;
		wxStringOutputStream out_stream(&json);
		httpStream->Read(out_stream);

		details = _parseMarkerJson(json);
		_storeMarkerJsonDb(id, json);
		//wxMessageBox(details);
	}
	else
	{
		wxMessageBox( _T("Unable to connect: ") +
			getErrorText(get.GetError(),get.GetResponse()) +
			_T("\n") +
			wxString::Format(wxT("(Error %d, "),get.GetError())+
			wxString::Format(wxT("HTTP %d)"),get.GetResponse()));
	}
	 
	wxDELETE(httpStream);
	get.Close();

	return details;
}


wxString goodanchorage_pi::_parseMarkerJson(wxString res) {
	wxString forPrint;
	wxJSONValue  root;
	wxJSONReader reader;
	int id;

	// now read the JSON text and store it in the 'root' structure
	// check for errors before retreiving values...
	int numErrors = reader.Parse( res, &root );
	if ( numErrors > 0  )  {
		wxMessageBox(_T("!root.IsArray() ") + res);
		return wxEmptyString;
	}
			
	if( root[_T("id")].IsValid() && !root[_T("id")].IsNull() ) {
		id = root[_T("id")].AsInt();
		(void)id;
	}
				
	if( root[_T("title")].IsValid() && !root[_T("title")].IsNull() )
	{
		wxString title = root[_T("title")].AsString(); //Anchorage Name
		forPrint += _T("Anchorage Name: ") + title + _T("\n");
	}

	if( root[_T("title_alt")].IsValid() && !root[_T("title_alt")].IsNull() )
	{
		wxString title_alt = root[_T("title_alt")].AsString() ; //Alternative Name
		forPrint += _T("Alternative Name: ") + title_alt + _T("\n");
	}

	if( root[_T("lat_lon")].IsValid() && !root[_T("lat_lon")].IsNull() )
	{
		wxJSONValue lat_lon = root[_T("lat_lon")];//Anchorage Position Single lat/lon pair
					 
		if ( !lat_lon.IsArray() ) 
		{					
			wxMessageBox(_T("!lat_lon.IsArray()) ") + res);						
			return wxEmptyString;
		}

		double lat_i = lat_lon[0].AsDouble();
		double lon_i = lat_lon[1].AsDouble();
				
		forPrint += _T("Anchorage Position: ") 
					+ wxString::Format(wxT(" %.3f  "),lat_i)
					+ wxString::Format(wxT(" %.3f"),lon_i)
					+ _T("\n");
	}
			
	if( root[_T("wp_lat_lon")].IsValid() && !root[_T("wp_lat_lon")].IsNull() )
	{
		wxJSONValue wp_lat_lon  = root[_T("wp_lat_lon")]; //Safe Waypoint(s) Multiple lat/lon pairs
				
		if ( !wp_lat_lon.IsArray() ) 
		{					
			wxMessageBox(_T("!wp_lat_lon.IsArray()) ") + res);						
			return wxEmptyString;
		}
		forPrint += _T("Safe Waypoint(s): ") ;
				
		for(int i=0; i < wp_lat_lon.Size();i++)
		{
			double lat_wp_i = wp_lat_lon[i][0].AsDouble();
			double lon_wp_i = wp_lat_lon[i][0].AsDouble();

			forPrint += wxString::Format(wxT(" %.3f"),lat_wp_i)
				+ wxString::Format(wxT(" %.3f"),lon_wp_i)
				+ _T(" ; ");
		}
				
		forPrint += _T("\n"); 
	}

	if( root[_T("depth_m")].IsValid() && !root[_T("depth_m")].IsNull() )
	{
		wxJSONValue depth_m_val = root[_T("depth_m")];
		double depth_m;
		if (depth_m_val.IsDouble()) {
			depth_m = depth_m_val.AsDouble(); //Depth (m)
		} else {
			depth_m = depth_m_val.AsInt();
		}
				

		forPrint += _T("Depth (m): ") 
			+ wxString::Format(wxT(" %.1f"),depth_m)
			+ _T("\n");
	}

	if( root[_T("depth_ft")].IsValid() && !root[_T("depth_ft")].IsNull() )
	{
		wxJSONValue depth_ft_val = root[_T("depth_ft")];
		double depth_ft;
		if (depth_ft_val.IsDouble()) {
			depth_ft = depth_ft_val.AsDouble(); //Depth (ft)
		} else {
			depth_ft = depth_ft_val.AsInt();
		}
				
		forPrint += _T("Depth (ft): ") 
			+ wxString::Format(wxT(" %.0f"),depth_ft)
			+ _T("\n");
	}

				
	if( root[_T("is_deep")].IsValid() && !root[_T("is_deep")].IsNull() ) {
		bool is_deep = root[_T("is_deep")].AsBool() ; //Don't show
		(void)is_deep;
	}

	if( root[_T("bottom")].IsValid() && !root[_T("bottom")].IsNull() )
	{			
		wxJSONValue bottom  = root[_T("bottom")]; //Bottom Composition
				
		forPrint += _T("Bottom Composition: ") ;
		for(int i=0; i < bottom.Size();i++)
		{
			forPrint += bottom[i].AsString() + _T(" ");
		}
		forPrint += _T("\n"); 
	}
			
	if( root[_T("prot_wind")].IsValid() && !root[_T("prot_wind")].IsNull() )
	{
		wxJSONValue prot_wind  = root[_T("prot_wind")]; //Protection from Wind
				
		forPrint += _T("Protection from Wind: ") ;
		for(int i=0; i < prot_wind.Size();i++)
		{
			forPrint += prot_wind[i].AsString() + _T(" ");
		}
		forPrint += _T("\n"); 
	}

	if( root[_T("prot_swell")].IsValid() && !root[_T("prot_swell")].IsNull() )
	{
		wxJSONValue prot_swell  = root[_T("prot_swell")]; //Protection from Swell
				
		forPrint += _T("Protection from Swell: ") ;
		for(int i=0; i < prot_swell.Size();i++)
		{
			forPrint += prot_swell[i].AsString() + _T(" ");
		}
		forPrint += _T("\n"); 
	}

	if( root[_T("navigation")].IsValid() && !root[_T("navigation")].IsNull() )
	{
		wxJSONValue navigation  = root[_T("navigation")]; //Navigation Assets
				
		forPrint += _T("Navigation Assets: ") ;
		for(int i=0; i < navigation.Size();i++)
		{
			forPrint += navigation[i].AsString() + _T(" ");
		}
		forPrint += _T("\n"); 
	}

	if( root[_T("shore_access")].IsValid() && !root[_T("shore_access")].IsNull() )
	{
		wxJSONValue shore_access  = root[_T("shore_access")]; //Shore Access
				
		forPrint += _T("Shore Access: ") ;
		for(int i=0; i < shore_access.Size();i++)
		{
			forPrint += shore_access[i].AsString() + _T(" ");
		}
		forPrint += _T("\n"); 
	}

	if( root[_T("closeby")].IsValid() && !root[_T("closeby")].IsNull() )
	{
		wxString closeby  = root[_T("closeby")].AsString(); //Close-by
				
		forPrint += _T("Close-by: ") 
			+ closeby
			+ _T("\n");
	}

	if( root[_T("nearby_town")].IsValid() && !root[_T("nearby_town")].IsNull() )
	{
		wxString nearby_town = root[_T("nearby_town")].AsString(); // Nearest Town(s)
				
		forPrint += _T("Nearest Town(s): ") 
			+ nearby_town
			+ _T("\n");
	}

	if( root[_T("dangers")].IsValid() && !root[_T("dangers")].IsNull() )
	{
		wxJSONValue dangers  = root[_T("dangers")]; //Dangers
				
		forPrint += _T("Dangers: ") ;
		for(int i=0; i < dangers.Size();i++)
		{
			forPrint += dangers[i].AsString() + _T(" ");
		}
		forPrint += _T("\n"); 
	}

	if( root[_T("country")].IsValid() && !root[_T("country")].IsNull() )
	{
		wxString country  = root[_T("country")].AsString(); //Country
				
		forPrint += _T("Country: ") 
			+ country
			+ _T("\n");
	}

	if( root[_T("date_anchored")].IsValid() && !root[_T("date_anchored")].IsNull() )
	{
		wxString date_anchored  = root[_T("date_anchored")].AsString(); //Date Anchored
				
		forPrint += _T("Date Anchored: ") 
			+ date_anchored
			+ _T("\n");
	}

	if( root[_T("comments")].IsValid() && !root[_T("comments")].IsNull() )
	{
		wxString comments  = root[_T("comments")].AsString(); //Comments
				
		forPrint += _T("Comments: ") 
			+ comments
			+ _T("\n");
	}
	//wxMessageBox(forPrint);

	return forPrint;
}


void goodanchorage_pi::setServerAuthHeaders(wxHTTP &httpObj)
{
	if( !gaAuthFile->Exists() || !gaAuthFile->Open() || gaAuthFile->GetLineCount() < 3 ) return;

	httpObj.SetHeader(_T("Content-type"), _T("text/html; charset=utf-8"));

	wxString sessid = gaAuthFile->GetFirstLine();
	wxString session_name = gaAuthFile->GetNextLine();
	wxString token = gaAuthFile->GetNextLine();
		
	httpObj.SetHeader(_T("Cookie"),  session_name + _T("=") + sessid );
	httpObj.SetHeader(_T("X-CSRF-Token"),  token );
}


bool CustomDialog::sendRequestAuth(wxString login, wxString password)
{
	if( !gaAuthFile->Exists() || !gaAuthFile->Open()) return false;

	wxHTTP http;
    http.SetHeader(_T("Content-type"), _T("application/json")); 
    http.SetPostBuffer(_T("{ \"username\":\"") + login + _T("\",\"password\":\"")+ password +_T("\" }")); 
    http.Connect(_T("api.goodanchorage.com"));
    wxInputStream *httpStream = http.GetInputStream(_T("/api/v1/user/login.json"));
	
	gaAuthFile->Clear();
	
    if (http.GetError() == wxPROTO_NOERR)
    {
        wxString res;
        wxStringOutputStream out_stream(&res);
        httpStream->Read(out_stream);
		// wxMessageBox(res);

		wxJSONValue  root;
		wxJSONReader reader;

        // now read the JSON text and store it in the 'root' structure
        // check for errors before retreiving values...
		int numErrors = reader.Parse( res, &root );
		if ( numErrors > 0  )  {
			wxMessageBox(_T("reader  ERR0R:") + res);
			return false;
		}
		
		if( root[_T("sessid")].IsValid() && !root[_T("sessid")].IsNull() )
		{
			wxString sessid = root[_T("sessid")].AsString();	

			gaAuthFile->AddLine( sessid );
		}

		if( root[_T("session_name")].IsValid() && !root[_T("session_name")].IsNull() )
		{
			wxString session_name = root[_T("session_name")].AsString();

			gaAuthFile->AddLine( session_name );				
		}

		if( root[_T("token")].IsValid() && !root[_T("token")].IsNull() )
		{
			wxString token = root[_T("token")].AsString();	

			gaAuthFile->AddLine( token );
		}  
    }
    else
    {
		wxMessageBox( _T("Unable to connect: ") +
			getErrorText(http.GetError(),http.GetResponse()) +
			_T("\n") +
			wxString::Format(wxT("(Error %d, "),http.GetError())+
			wxString::Format(wxT("HTTP %d)"),http.GetResponse()));
		isOnline = false;
    }

	gaAuthFile->Write();
	gaAuthFile->Close();

    wxDELETE(httpStream);
    http.Close();

	return true;
}


wxString getErrorText(int errorID,int codeID)
{
	if(errorID == 3 && codeID == 0)
	{	
		return _T("Connection to server failed.");
	}
	else
	if(errorID == 6 && codeID == 401 )
	{	
		return _T("Unrecognized username or password.");
	}
	else
	if(errorID == 6 && codeID == 403 )
	{	
		return _T("Access to data is forbidden.");
	}
	else
	{
		return _T("Unknown error.");
	}	
}


void goodanchorage_pi::_storeMarkerJsonDb(int id, wxString json) {
	// insert or update while keeping existing values: json and updated
	char const *sql = "UPDATE anchor_point SET json = ?, updated = ? WHERE id = ?;";
	sqlite3_stmt *stmt;
	if ( sqlite3_prepare(gaDb, sql, -1, &stmt, 0) != SQLITE_OK) {
		wxMessageBox(_T("Failed prepare to update Anchorage in local storage"));
		sqlite3_finalize(stmt);
		return;
	}
	if (sqlite3_bind_text(stmt, 1, strdup(json.utf8_str()), json.length(), SQLITE_STATIC) != SQLITE_OK) {
		wxMessageBox(_T("Failed to bind JSON in local storage"));
		sqlite3_finalize(stmt);
		return;
	}
	if (sqlite3_bind_int(stmt, 2, wxGetUTCTime()) != SQLITE_OK) {
		wxMessageBox(_T("Failed to bind Updated in local storage"));
		sqlite3_finalize(stmt);
		return;
	}
	if (sqlite3_bind_int(stmt, 3, id) != SQLITE_OK) {
		wxMessageBox(_T("Failed to bind ID in local storage"));
		sqlite3_finalize(stmt);
		return;
	}
	
	if (sqlite3_step(stmt) != SQLITE_DONE) {
		wxMessageBox(_T("Failed to update Anchorage in local storage"));
		sqlite3_finalize(stmt);
		return;
	}

	sqlite3_finalize(stmt);
	return;
}


wxString goodanchorage_pi::_loadMarkerDetailsDb(int id) {
	wxString json, details;
	int updated;	// TODO: convert to UTC date and add to the extracted marker data
	char const *sql = "SELECT json, updated FROM anchor_point WHERE id = ?;";
	sqlite3_stmt *stmt;
	int rc = sqlite3_prepare_v2(gaDb, sql, -1, &stmt, 0);
	if( rc != SQLITE_OK ){
      wxMessageBox(wxString::Format(wxT("Failed to fetch Anchorage details: %s"), sqlite3_errmsg(gaDb)));
	  sqlite3_finalize(stmt);
	  return wxEmptyString;
	}

	if (sqlite3_bind_int(stmt, 1, id) != SQLITE_OK) {
		wxMessageBox(_T("Failed to bind ID while loading locally stored anchorage"));
		sqlite3_finalize(stmt);
		return wxEmptyString;
	}

    if (sqlite3_step(stmt) == SQLITE_ROW) {
		json = wxString::FromUTF8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
		if (!json.IsNull()) details = _parseMarkerJson(json);
		else details = _T("No details found in local storage for this anchorage.");
		updated = sqlite3_column_int(stmt, 0);
		(void)updated;
    }
	sqlite3_finalize(stmt);

	return details;
}


void goodanchorage_pi::cleanMarkerList(void)
{
	m_ActiveMarker = NULL;
	m_ActiveMyMarker = NULL;

	for(unsigned int i = 0; i < markersList.size(); i++)
	{
		DeleteSingleWaypoint(  markersList[i].pluginWaypoint->m_GUID );
	}
	
	markersList.clear();
}


void goodanchorage_pi::showMarkerList(void)
{
	// TODO: when charts are present markers are hidden unless there is a zoom in/out
	// Need to add a layer?
	for(unsigned int i = 0; i < markersList.size(); i++)
	{
		AddSingleWaypoint(  markersList[i].pluginWaypoint,  true);
	}
}
	

GAMarker::GAMarker(void){}
GAMarker::~GAMarker(void){}

wxString GAMarker::getMarkerTitle(void)
{
	wxString result = this->serverTitle 
	
	+wxString::Format(wxT(" (%.3f"),this->serverLat)
	+wxString::Format(wxT(", %.3f / "),this->serverLon) ;
	
	if(this->serverDeep)
	{
		result += _T("deep)");
	}
	else
	{
		result += _T("shallow)");
	}
	
	return result;
}

void goodanchorage_pi::initLoginDialog(wxWindow* parent)
{

	loginDialog = new CustomDialog(wxT("GoodAnchorage Access"),parent);	
}


CustomDialog::CustomDialog(const wxString & title,wxWindow* parent)
       : wxDialog(parent, -1, title, wxDefaultPosition, wxSize(280, 200))
{
	
	wxFlexGridSizer *fgs = new wxFlexGridSizer(2, 2, 9, 25);
	wxFlexGridSizer *fgsSingUp = new wxFlexGridSizer(1, 1, 9, 0);
	
	wxBoxSizer *verticalBox = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *buttonsHorithontalBox = new wxBoxSizer(wxHORIZONTAL);
  
	wxBoxSizer *loginHorithontalBox = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer *passwordHorithontalBox = new wxBoxSizer(wxHORIZONTAL);

	wxStaticText *loginTitle = new wxStaticText(this, -1, wxT("Login:"));
	wxStaticText *passwordTitle = new wxStaticText(this, -1, wxT("Password:"));
  
	loginTextCtrl = new wxTextCtrl(this, -1);
	passwordTextCtrl = new wxTextCtrl(this, -1, _T(""), wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
  
	loginHorithontalBox->Add(loginTitle,1);
	loginHorithontalBox->Add(loginTextCtrl,1, wxLEFT, 5);
  
	passwordHorithontalBox->Add(passwordTitle,-1);
	passwordHorithontalBox->Add(passwordTextCtrl,-1);
  
	wxButton *okButton = new wxButton(this, LOGIN_BUTTON_ID, wxT("Ok"), 
		wxDefaultPosition, wxSize(70, 30));
	  
	Connect(LOGIN_BUTTON_ID, wxEVT_COMMAND_BUTTON_CLICKED, 
		wxCommandEventHandler(CustomDialog::onLogin));

	wxButton *closeButton = new wxButton(this, wxID_EXIT, wxT("Cancel"), 
		wxDefaultPosition, wxSize(70, 30));
	  
	Connect(wxID_EXIT, wxEVT_COMMAND_BUTTON_CLICKED, 
		wxCommandEventHandler(CustomDialog::onQuit));
	
	wxColour blue(wxT("#2A2AF7"));
	wxColour backColour(wxT("#FFFFFF")); 
	wxHyperlinkCtrl *singUpButton = new wxHyperlinkCtrl(this, -1, wxT("Join GoodAnchorage.com"),wxT("http://www.goodanchorage.com/user/register"), 
	wxDefaultPosition, wxSize(230, 30) );
	
	singUpButton->SetForegroundColour( blue );
	
	fgsSingUp->Add(singUpButton);
	 
	fgs->Add(loginTitle);
	fgs->Add(loginTextCtrl, 1, wxEXPAND);
	fgs->Add(passwordTitle);
	fgs->Add(passwordTextCtrl, 1, wxEXPAND);
	fgs->AddGrowableRow(2, 1);
	fgs->AddGrowableCol(1, 1);
	
	buttonsHorithontalBox->Add(okButton, 1);
	buttonsHorithontalBox->Add(closeButton, 1, wxLEFT, 5);

	//verticalBox->Add(loginHorithontalBox, 1);
	//verticalBox->Add(passwordHorithontalBox, 1);
	verticalBox->Add(fgs, 1, wxALL | wxEXPAND, 15);
	verticalBox->Add(buttonsHorithontalBox, 0, wxALIGN_CENTER | wxTOP | wxBOTTOM, 10);
	verticalBox->Add(fgsSingUp, 0, wxALIGN_CENTER, 0);
 
	SetSizer(verticalBox);
}


void CustomDialog::onQuit(wxCommandEvent & WXUNUSED(event))
{
	//SetToolbarItemState( m_leftclick_tool_id, false );
    Close(true);
}


void CustomDialog::onLogin(wxCommandEvent & WXUNUSED(event))
{
	//SetToolbarItemState( m_leftclick_tool_id, true );
	wxString login = loginTextCtrl->GetValue();
	wxString password = passwordTextCtrl->GetValue();
	wxBeginBusyCursor();
	if (!sendRequestAuth(login,password)) {
		// TODO: show login box again
		// If canceled -- switch to offline
	}
	wxEndBusyCursor();
    Close(true);
}

void CustomDialog::setM_leftclick_tool_id(int m_leftclick_tool_id)
{
	this->m_leftclick_tool_id = m_leftclick_tool_id;
}
