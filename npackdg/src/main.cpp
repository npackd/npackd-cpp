#include <vector>

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWidgets headers)
#ifndef WX_PRECOMP
    #include "wx/wx.h"
    #include "wx/aui/aui.h"
    #include "wx/grid.h"
    #include "wx/intl.h"
#endif

const wxWindowID NotebookID = wxID_HIGHEST + 1;

// Define a new application type, each program should derive a class from wxApp
class MyApp : public wxApp {
public:
    // override base class virtuals
    // ----------------------------

    // this one is called on application startup and is a good place for the app
    // initialization (doing it here and not in the ctor allows to have an error
    // return: if OnInit() returns false, the application terminates)
    virtual bool OnInit() override;
};

/**
 * @brief a GUI action
 */
class Action {
public:
    /** wxWidgets event ID */
    int id;

    /** title and accelerator separated by \t */
    wxString title;

    /** hint */
    wxString hint;

    Action(int id, const wxString& title, const wxString& hint) : id(id), title(title), hint(hint) {
    }
};

// Define a new frame type: this is going to be our main frame
class MyFrame : public wxFrame
{
public:
    // ctor(s)
    MyFrame(const wxString& title);

    // event handlers (these functions should _not_ be virtual)
    void OnQuit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);

private:
    wxAuiNotebook* auiNotebook;

    std::vector<Action> actions;

    // any class wishing to process wxWidgets events must use this macro
    wxDECLARE_EVENT_TABLE();

    wxGrid *createGrid();

    void addTextTab(const wxString &title, const wxString &text);


};

// IDs for the controls and the menu commands
enum
{
    // menu items
    Minimal_Quit = wxID_EXIT,

    // it is important for the id corresponding to the "About" command to have
    // this standard value as otherwise it won't be handled properly under Mac
    // (where it is special and put into the "Apple" menu)
    Minimal_About = wxID_ABOUT,

    /** install packages */
    ID_Install = wxID_HIGHEST + 1,

    /** uninstall packages */
    ID_Uninstall,

    /** update packages */
    ID_Update,
};

// the event tables connect the wxWidgets events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU(ID_Install,  MyFrame::OnQuit)
    EVT_MENU(ID_Uninstall,  MyFrame::OnQuit)
    EVT_MENU(ID_Update,  MyFrame::OnQuit)
    EVT_MENU(Minimal_Quit,  MyFrame::OnQuit)
    EVT_MENU(Minimal_About, MyFrame::OnAbout)
wxEND_EVENT_TABLE()

// Create a new application object: this macro will allow wxWidgets to create
// the application object during program execution (it's better than using a
// static object for many reasons) and also implements the accessor function
// wxGetApp() which will return the reference of the right type (i.e. MyApp and
// not wxApp)
wxIMPLEMENT_APP(MyApp);

// 'Main program' equivalent: the program execution "starts" here
bool MyApp::OnInit()
{
    // call the base class initialization method, currently it only parses a
    // few common command-line options but it could be do more in the future
    if ( !wxApp::OnInit() )
        return false;

    // todo qInstallMessageHandler(eventLogMessageHandler);

        HMODULE m = LoadLibrary(L"exchndl.dll");
/* todo
        QLoggingCategory::setFilterRules("npackd=true\nnpackd.debug=false");

    #if NPACKD_ADMIN != 1
        WPMUtils::hasAdminPrivileges();
    #endif

        QString packageName;
    #if !defined(__x86_64__) && !defined(_WIN64)
        packageName = "com.googlecode.windows-package-manager.Npackd";
    #else
        packageName = "com.googlecode.windows-package-manager.Npackd64";
    #endif
        InstalledPackages::packageName = packageName;

        qRegisterMetaType<Version>("Version");
        qRegisterMetaType<int64_t>("int64_t");
        qRegisterMetaType<QMessageBox::Icon>("QMessageBox::Icon");

    #if !defined(__x86_64__) && !defined(_WIN64)
        if (WPMUtils::is64BitWindows()) {
            QMessageBox::critical(0, "Error",
                    QObject::tr("The 32 bit version of Npackd requires a 32 bit operating system.") + "\r\n" +
                    QObject::tr("Please download the 64 bit version from https://github.com/tim-lebedkov/npackd/wiki/Downloads"));
            return 1;
        }
    #endif


        // July, 25 2018:
        // "bmp", "cur", "gif", "ico", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "xbm", "xpm"
        // December, 25 2018:
        // "bmp", "cur", "gif", "icns", "ico", "jp2", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm"
        // July, 27 2019:
        // "bmp", "cur", "gif", "icns", "ico", "jpeg", "jpg", "pbm", "pgm", "png", "ppm", "tga", "tif", "tiff", "wbmp", "webp", "xbm", "xpm"
        qCDebug(npackd) << QImageReader::supportedImageFormats();

        // July, 25 2018: "windowsvista", "Windows", "Fusion"
        qCDebug(npackd) << QStyleFactory::keys();

        CLProcessor clp;

        int errorCode;
        clp.process(argc, argv, &errorCode);

        //WPMUtils::timer.dump();

        FreeLibrary(m);

        return errorCode;
    }

     */

    // create the main application window
    MyFrame *frame = new MyFrame("Minimal wxWidgets App");

    // and show it (the frames, unlike simple controls, are not shown when
    // created initially)
    frame->Show(true);

    // success: wxApp::OnRun() will be called which will enter the main message
    // loop and the application will run. If we returned false here, the
    // application would exit immediately.
    return true;
}

// frame constructor
MyFrame::MyFrame(const wxString& title)
       : wxFrame(NULL, wxID_ANY, title)
{
    wxImage::AddHandler(new wxPNGHandler);

    // set the frame icon
    SetIcon(wxICON(IDI_ICON1));

    actions.push_back(Action(Minimal_Quit, _("&Exit"), _("Exits the application")));
    actions.push_back(Action(ID_Install, _("&Install\tCTRL+I"),
            _("Installs the selected version of a package or the newest is none is selected")));
    actions.push_back(Action(ID_Uninstall, _("U&ninstall\tCTRL+N"),
            _("Uninstalls the currently selected version of a package or the newest if none is selected")));

    // create a menu bar
    wxMenu *fileMenu = new wxMenu;

    fileMenu->Append(ID_Install, "Install", "Exits the application");
    fileMenu->Append(ID_Uninstall, "Uninstall", "Exits the application");
    fileMenu->Append(ID_Update, "Update", "Exits the application");
    fileMenu->Append(Minimal_Quit, "&Exit", "Exits the application");

    // the "About" item should be in the help menu
    wxMenu *helpMenu = new wxMenu;
    helpMenu->Append(Minimal_About, "About\tF1", "About the program");

    // now append the freshly created menu to the menu bar...
    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, "&File");
    menuBar->Append(helpMenu, "&Help");

    // ... and attach this menu bar to the frame
    SetMenuBar(menuBar);

    // create a status bar just for fun (by default with 1 pane only)
    CreateStatusBar(1);

    // Create a top-level panel to hold all the contents of the frame
    wxPanel* content = new wxPanel(this, wxID_ANY);

    wxToolBar *toolbar = CreateToolBar(wxTB_FLAT | wxTB_DOCKABLE | wxTB_HORIZONTAL | wxTB_TEXT);

    toolbar->AddTool(ID_Install, _("Install"),
            wxBitmap("INSTALL_PNG", wxBITMAP_TYPE_PNG_RESOURCE));
    toolbar->AddTool(ID_Uninstall, _("Uninstall"),
            wxBitmap("UNINSTALL_PNG", wxBITMAP_TYPE_PNG_RESOURCE));
    toolbar->AddTool(ID_Update, _("Update"),
            wxBitmap("UPDATE_PNG", wxBITMAP_TYPE_PNG_RESOURCE));

    toolbar->Realize();

    // Create the wxAuiNotebook widget
    auiNotebook = new wxAuiNotebook(content, NotebookID,
            wxDefaultPosition, wxSize(150, 200),
            wxAUI_NB_TOP | wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE |
            wxAUI_NB_SCROLL_BUTTONS | wxAUI_NB_CLOSE_ON_ALL_TABS |
            wxAUI_NB_MIDDLE_CLICK_CLOSE);

    // Add 2 pages to the wxNotebook widget
    auiNotebook->AddPage(createGrid(), _("Packages"));
    addTextTab(_("Jobs"), "Contents");

    // Set up the sizer for the panel
    wxBoxSizer* panelSizer = new wxBoxSizer(wxHORIZONTAL);
    panelSizer->Add(auiNotebook, 1, wxEXPAND);
    content->SetSizer(panelSizer);

    // Set up the sizer for the frame and resize the frame
    // according to its contents
    wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);
    topSizer->SetMinSize(400, 200);
    topSizer->Add(content, 1, wxEXPAND);
    SetSizerAndFit(topSizer);
}

void MyFrame::addTextTab(const wxString& title, const wxString& text)
{
    wxTextCtrl* textCtrl2 = new wxTextCtrl(auiNotebook, wxID_ANY, text, wxDefaultPosition, wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxTE_AUTO_URL);
    auiNotebook->AddPage(textCtrl2, title, true);
}

wxGrid* MyFrame::createGrid()
{
    // Create a wxGrid object
    wxGrid* grid = new wxGrid( this,
                        -1,
                        wxPoint( 0, 0 ),
                        wxSize( 400, 300 ) );
    // Then we call CreateGrid to set the dimensions of the grid
    // (100 rows and 10 columns in this example)
    grid->CreateGrid( 100, 10 );
    // We can set the sizes of individual rows and columns
    // in pixels
    grid->SetRowSize( 0, 60 );
    grid->SetColSize( 0, 120 );
    // And set grid cell contents as strings
    grid->SetCellValue( 0, 0, "wxGrid is good" );
    // We can specify that some cells are read->only
    grid->SetCellValue( 0, 3, "This is read->only" );
    grid->SetReadOnly( 0, 3 );
    // Colours can be specified for grid cell contents
    grid->SetCellValue(3, 3, "green on grey");
    grid->SetCellTextColour(3, 3, *wxGREEN);
    grid->SetCellBackgroundColour(3, 3, *wxLIGHT_GREY);
    // We can specify the some cells will store numeric
    // values rather than strings. Here we set grid column 5
    // to hold floating point values displayed with width of 6
    // and precision of 2
    grid->SetColFormatFloat(5, 6, 2);
    grid->SetCellValue(0, 6, "3.1415");
    return grid;
}

void MyFrame::OnQuit(wxCommandEvent& /* event */)
{
    // true is to force the frame to close
    Close(true);
}

void MyFrame::OnAbout(wxCommandEvent& /* event */)
{
    /*
    wxMessageBox(wxString::Format
                 (
                    "Welcome to %s!\n"
                    "\n"
                    "This is the minimal wxWidgets sample\n"
                    "running under %s.",
                    wxVERSION_STRING,
                    wxGetOsDescription()
                 ),
                 "About wxWidgets minimal sample",
                 wxOK | wxICON_INFORMATION,
                 this);
                 */
    addTextTab(_("About"), _("<html><body>Npackd %1 - "
                   "software package manager for Windows (R)"
                   "<ul>"
                   "<li><a href='https://www.npackd.org/'>Home page (https://www.npackd.org)</a></li>"
                   "<li><a href='https://github.com/tim-lebedkov/npackd/wiki/ChangeLog'>Changelog</a></li>"
                   "<li><a href='https://github.com/tim-lebedkov/npackd/wiki'>Documentation</a></li>"
                   "<li>Author: <a href='https://github.com/tim-lebedkov'>Tim Lebedkov</a></li>"
                   "</ul>"
                   "Contributors:"
                   "<ul>"
                   "<li><a href='https://github.com/OgreTransporter'>OgreTransporter</a>: Visual C++ support, CMake integration, group policy configuration, non-admin installations</li>"
                   "</ul>"
                   "</body></html>"));
                    // TODO                   arg(NPACKD_VERSION)); + HTML
}

