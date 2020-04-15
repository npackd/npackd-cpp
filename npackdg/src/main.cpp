// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWidgets headers)
#ifndef WX_PRECOMP
    #include "wx/wx.h"
    #include "wx/aui/aui.h"
    #include "wx/grid.h"
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
    // any class wishing to process wxWidgets events must use this macro
    wxDECLARE_EVENT_TABLE();
    wxGrid *createGrid();
};

// IDs for the controls and the menu commands
enum
{
    // menu items
    Minimal_Quit = wxID_EXIT,

    // it is important for the id corresponding to the "About" command to have
    // this standard value as otherwise it won't be handled properly under Mac
    // (where it is special and put into the "Apple" menu)
    Minimal_About = wxID_ABOUT
};

// the event tables connect the wxWidgets events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
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
    // set the frame icon
    SetIcon(wxICON(IDI_ICON1));

    // create a menu bar
    wxMenu *fileMenu = new wxMenu;

    // the "About" item should be in the help menu
    wxMenu *helpMenu = new wxMenu;
    helpMenu->Append(Minimal_About, "About\tF1", "About the program");

    fileMenu->Append(Minimal_Quit, "&Exit", "Exits the application");

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

    // Create the wxAuiNotebook widget
    wxAuiNotebook* auiNotebook = new wxAuiNotebook(content, NotebookID,
            wxDefaultPosition, wxSize(150, 200));

    // Add 2 pages to the wxNotebook widget
    auiNotebook->AddPage(createGrid(), L"Tab 1");
    wxTextCtrl* textCtrl2 = new wxTextCtrl(auiNotebook, wxID_ANY, L"Tab 2 Contents");
    auiNotebook->AddPage(textCtrl2, L"Tab 2");

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

void MyFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
    // true is to force the frame to close
    Close(true);
}

void MyFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
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
}
