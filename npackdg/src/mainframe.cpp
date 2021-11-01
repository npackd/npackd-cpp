#include <algorithm>
#include <windows.h>
#include <windowsx.h>

#include "mainframe.h"
#include "ui_mainframe.h"

#include <QListWidgetItem>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QAction>

#include "dbrepository.h"
#include "mainwindow.h"
#include "package.h"
#include "windowsregistry.h"
#include "abstractrepository.h"
#include "uiutils.h"
#include "wpmutils.h"
#include "gui.h"

/** ID of the edit filter control. 8 is the first valid Id. */
#define ID_EDIT_FILTER 8

LRESULT CALLBACK packagesPanelSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR dwRefData)
{
    MainFrame* mf = reinterpret_cast<MainFrame*>(dwRefData);
    return mf->windowProc(hWnd, uMsg, wParam, lParam);
}

MainFrame::MainFrame(QWidget *parent) :
    QObject(parent), Selection(), obsoleteBrush(QColor(255, 0xc7, 0xc7)),
    maxStars(-1)
{
    this->categoryCombosEvents = true;

    /* todo

    t->horizontalHeader()->setSectionHidden(6, true);
    t->horizontalHeader()->setSectionHidden(7, true);
    t->horizontalHeader()->setSectionHidden(8, true);
    t->horizontalHeader()->setSectionHidden(9, true);

            */
}

int MainFrame::getCategoryFilter(int level) const
{
    int r = -1;

    if (level == 0) {
        int sel = ComboBox_GetCurSel(comboBoxCategory0);
        if (sel <= 0)
            r = -1;
        else
            r = this->categories0.at(sel - 1).at(0).toInt();
    } else if (level == 1) {
        int sel = ComboBox_GetCurSel(comboBoxCategory1);
        if (sel <= 0)
            r = -1;
        else
            r = this->categories1.at(sel - 1).at(0).toInt();
    } else
        r = -1;

    return r;
}

int MainFrame::getStatusFilter() const
{
    int r;
    if (Button_GetCheck(buttonInstalled) == BST_CHECKED)
        r = 1;
    else if (Button_GetCheck(buttonUpdateable) == BST_CHECKED)
        r = 2;
    else
        r = 0;

    return r;
}

LRESULT CALLBACK MainFrame::windowProc(HWND hWnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam)
{
    MainWindow* mw = MainWindow::getInstance();

    switch(uMsg)
    {
        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code) {
                case LVN_GETDISPINFO:
                {
                    // provide the data for the packages table
                    NMLVDISPINFO* pnmv = (NMLVDISPINFO*) lParam;
                    if (pnmv->item.iItem < 0 || // typo fixed 11am
                            pnmv->item.iItem >= static_cast<int>(
                            getRowCount())) {
                        return E_FAIL;         // requesting invalid item
                    }

                    LPWSTR pszResult;
                    if (pnmv->item.mask & LVIF_TEXT) {
                        pnmv->item.pszText = WPMUtils::toLPWSTR(
                            getCellText(pnmv->item.iItem, pnmv->item.iSubItem));
                    }
                    if (pnmv->item.mask & LVIF_IMAGE) {
                        pnmv->item.iImage = -1;
                    }
                    if (pnmv->item.mask & LVIF_STATE) {
                        pnmv->item.state = 0;
                    }
                    break;
                }
                case LVN_ITEMCHANGED:{
                    tableWidget_selectionChanged();
                }
            }
            break;
        }
        case WM_SIZE:
        {
            packagesPanelLayout();
            break;
        }
        case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);

            if (wmId == IDM_INSTALL || wmId == IDM_UNINSTALL ||
                wmId == IDM_UPDATE || wmId == IDM_SHOW_DETAILS ||
                wmId == IDM_SHOW_CHANGELOG || wmId == IDM_RUN ||
                wmId == IDM_OPEN_FOLDER || wmId == IDM_OPEN_WEB_SITE ||
                wmId == IDM_TEST_DOWNLOAD_SITE || wmId == IDM_EXPORT)
            {
                mw->windowProc(mw->window, uMsg, wParam, lParam);
            } else if (wmId == ID_EDIT_FILTER && HIWORD(wParam) == EN_CHANGE) {
                mw->fillList();
            } else if (HIWORD(wParam) == BN_CLICKED) {
                if ((HWND) lParam == buttonAll ||
                    (HWND) lParam == buttonInstalled ||
                    (HWND) lParam == buttonUpdateable) {
                    fillList();
                    selectSomething();
                }
            }

            break;
        }
        case WM_CONTEXTMENU:
        {
            int xPos = GET_X_LPARAM(lParam);
            int yPos = GET_Y_LPARAM(lParam);

            contextMenu(xPos, yPos);
            break;
        }
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void MainFrame::packagesPanelLayout()
{
    RECT r;
    GetClientRect(packagesPanel, &r);

    r.left = 200;
    r.top = 10;
    r.bottom -= 10;
    r.right -= 10;
    MoveWindow(table, r.left, r.top,
        r.right - r.left, r.bottom - r.top, FALSE);
}

HWND MainFrame::createPackagesPanel(HWND parent)
{
    HWND result = t_gui_create_panel(parent);
    SetWindowSubclass(result, &packagesPanelSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));

    SendMessage(result, WM_SETFONT, (LPARAM)defaultFont, TRUE);

    int y = 10;
    HWND label = t_gui_create_label(result, L"S&earch:");
    SIZE sz = t_gui_get_preferred_size(label);
    MoveWindow(label, 10, y, sz.cx, sz.cy, FALSE);
    y += sz.cy + 5;

    filterLineEdit = t_gui_create_edit(result, ID_EDIT_FILTER);
    sz = t_gui_get_preferred_size(filterLineEdit);
    MoveWindow(filterLineEdit, 10, y, 180, sz.cy, FALSE);
    y += sz.cy + 5;

    buttonAll = t_gui_create_radio_button(result, L"&All");
    sz = t_gui_get_preferred_size(buttonAll);
    MoveWindow(buttonAll, 10, y, sz.cx, sz.cy, FALSE);
    Button_SetCheck(buttonAll, BST_CHECKED);
    y += sz.cy + 5;

    buttonInstalled = t_gui_create_radio_button(result, L"&Installed");
    sz = t_gui_get_preferred_size(buttonInstalled);
    MoveWindow(buttonInstalled, 10, y, sz.cx, sz.cy, FALSE);
    y += sz.cy + 5;

    buttonUpdateable = t_gui_create_radio_button(result, L"&Updateable");
    sz = t_gui_get_preferred_size(buttonUpdateable);
    MoveWindow(buttonUpdateable, 10, y, sz.cx, sz.cy, FALSE);
    y += sz.cy + 5;

    label = t_gui_create_label(result, L"Category:");
    sz = t_gui_get_preferred_size(label);
    MoveWindow(label, 10, y, sz.cx, sz.cy, FALSE);
    y += sz.cy + 5;

    comboBoxCategory0 = t_gui_create_combobox(result);
    sz = t_gui_get_preferred_size(comboBoxCategory0);
    MoveWindow(comboBoxCategory0, 10, y, 180, sz.cy, FALSE);
    y += sz.cy + 5;

    label = t_gui_create_label(result, L"Sub-category:");
    sz = t_gui_get_preferred_size(label);
    MoveWindow(label, 10, y, sz.cx, sz.cy, FALSE);
    y += sz.cy + 5;

    comboBoxCategory1 = t_gui_create_combobox(result);
    sz = t_gui_get_preferred_size(comboBoxCategory1);
    MoveWindow(comboBoxCategory1, 10, y, 180, sz.cy, FALSE);
    y += sz.cy + 5;

    labelDuration = t_gui_create_label(result, L"");
    sz = t_gui_get_preferred_size(labelDuration);
    MoveWindow(labelDuration, 10, y, 180, sz.cy, FALSE);
    SetWindowTextW(labelDuration, L"0ms");
    //y += sz.cy + 5;

    table = createTable(result);

    this->packagesPanel = result;

    SetWindowLongPtr(result, GWLP_USERDATA, (LONG_PTR) this);

    return result;
}

HWND MainFrame::createTable(HWND parent)
{
    HWND table = CreateWindow(WC_LISTVIEW, NULL,
                  WS_VISIBLE | WS_CHILD | WS_TABSTOP |
                  LVS_NOSORTHEADER | LVS_OWNERDATA |
                  LVS_REPORT,
                  200, 25, 200, 200,
                  parent,
                  0,
                  hInst,
                  NULL);

    ListView_SetExtendedListViewStyle(table,
        LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES |
        LVS_EX_HEADERDRAGDROP | LVS_EX_LABELTIP);

    HIMAGELIST images = t_gui_create_image_list(
        UIUtils::ICON_SIZE, UIUtils::ICON_SIZE, NULL, 0);
    ListView_SetImageList(table, images, LVSIL_SMALL);

    LVCOLUMN col = {};
    col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    col.fmt = LVCFMT_LEFT;
    col.cx = 190;
    col.pszText = const_cast<LPWSTR>(L"Title");
    ListView_InsertColumn(table, 0, &col);

    col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    col.fmt = LVCFMT_LEFT;
    col.cx = 300;
    col.pszText = const_cast<LPWSTR>(L"Description");
    ListView_InsertColumn(table, 1, &col);

    col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    col.fmt = LVCFMT_LEFT;
    col.cx = 100;
    col.pszText = const_cast<LPWSTR>(L"Available");
    ListView_InsertColumn(table, 2, &col);

    col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    col.fmt = LVCFMT_LEFT;
    col.cx = 100;
    col.pszText = const_cast<LPWSTR>(L"Installed");
    ListView_InsertColumn(table, 3, &col);

    col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    col.fmt = LVCFMT_LEFT;
    col.cx = 100;
    col.pszText = const_cast<LPWSTR>(L"License");
    ListView_InsertColumn(table, 4, &col);

    col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    col.fmt = LVCFMT_RIGHT;
    col.cx = 100;
    col.pszText = const_cast<LPWSTR>(L"Download size");
    ListView_InsertColumn(table, 5, &col);

    col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    col.fmt = LVCFMT_LEFT;
    col.cx = 100;
    col.pszText = const_cast<LPWSTR>(L"Category");
    ListView_InsertColumn(table, 6, &col);

    col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    col.fmt = LVCFMT_LEFT;
    col.cx = 100;
    col.pszText = const_cast<LPWSTR>(L"Tags");
    ListView_InsertColumn(table, 7, &col);

    col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
    col.fmt = LVCFMT_RIGHT;
    col.cx = 20;
    col.pszText = const_cast<LPWSTR>(L"Stars");
    ListView_InsertColumn(table, 8, &col);

    return table;
}


MainFrame::~MainFrame()
{
    qDeleteAll(this->selectedPackages);
}

void MainFrame::saveColumns() const
{
    WindowsRegistry wr;
    QString err = wr.open(HKEY_CURRENT_USER, "", false);

    WindowsRegistry wrr;
    if (err.isEmpty()) {
        wrr = wr.createSubKey(
                "Software\\Npackd\\Npackd", &err,
                KEY_ALL_ACCESS);
    }

    if (err.isEmpty()) {
        // TODO wrr.setBytes("MainTableState", t->horizontalHeader()->saveState());
    }
}

void MainFrame::loadColumns() const
{
    WindowsRegistry wr;
    if (wr.open(HKEY_CURRENT_USER,
            "Software\\Npackd\\Npackd", false,
            KEY_READ).isEmpty()) {
        QString err;
        QByteArray ba = wr.getBytes("MainTableState", &err);
        if (err.isEmpty()) {
            // TODO QTableView* t = this->ui->tableWidget;
            // TODO t->horizontalHeader()->restoreState(ba);
        }
    } else {
        QString err = wr.open(HKEY_CURRENT_USER,
                "Software\\Npackd\\Npackd\\TableColumns", false,
                KEY_READ);

        std::vector<QString> v;
        if (err.isEmpty()) {
            v = wr.loadStringList(&err);
        }

        if (err.isEmpty()) {
        /* todo
            QTableView* t = this->ui->tableWidget;
            for (int i = 0; i < std::min<int>(t->model()->columnCount(),
                    v.size()); i++) {
                bool ok;
                int w = v.at(i).toInt(&ok);
                if (!ok)
                    break;

                t->setColumnWidth(i, w);
            }
            */
        }
    }
}

void MainFrame::setCategories(int level, const std::vector<std::vector<QString>> &cats)
{
    this->categoryCombosEvents = false;

    std::vector<QString> labels;
    int count = 0;
    for (auto& c: cats) {
        QString n;
        n = c.at(2);
        if (n.isEmpty()) {
            n = QObject::tr("Uncategorized");
        }
        labels.push_back(n + " (" + c.at(1) + ")");
        count += c.at(1).toInt();
    }

    if (level == 0) {
        ComboBox_ResetContent(this->comboBoxCategory0);

        QString s = QObject::tr("All") + " (" + QString::number(count) + ")";
        ComboBox_AddString(this->comboBoxCategory0, WPMUtils::toLPWSTR(s));

        for (auto& label: labels) {
            ComboBox_AddString(this->comboBoxCategory0, WPMUtils::toLPWSTR(label));
        }

        EnableWindow(this->comboBoxCategory0, true);
        this->categories0 = cats;

        this->categories1.clear();
        EnableWindow(this->comboBoxCategory1, false);
    } else if (level == 1) {
        ComboBox_ResetContent(this->comboBoxCategory1);

        if (labels.size() > 0) {
            QString s = QObject::tr("All") + " (" + QString::number(count) + ")";
            ComboBox_AddString(this->comboBoxCategory1, WPMUtils::toLPWSTR(s));

            for (auto& label: labels) {
                ComboBox_AddString(this->comboBoxCategory1, WPMUtils::toLPWSTR(label));
            }

            EnableWindow(this->comboBoxCategory1, true);
        } else {
            EnableWindow(this->comboBoxCategory1, false);
        }
        this->categories1 = cats;
    }

    this->categoryCombosEvents = true;
}

void MainFrame::setDuration(int d)
{
    QString s = QObject::tr("Found in %1 ms").arg(d);
    Static_SetText(this->labelDuration, WPMUtils::toLPWSTR(s));
}

void MainFrame::setCategoryFilter(int level, int v)
{
    this->categoryCombosEvents = false;

    if (level == 0) {
        int newCurrentIndex = ComboBox_GetCurSel(this->comboBoxCategory0);
        if (v == -1)
            newCurrentIndex = 0;
        else {
            for (int i = 0; i < static_cast<int>(this->categories0.size()); i++) {
                int c = this->categories0.at(i).at(0).toInt();
                if (c == v) {
                    newCurrentIndex = i + 1;
                    break;
                }
            }
        }
        if (ComboBox_GetCurSel(this->comboBoxCategory0) != newCurrentIndex) {
            ComboBox_SetCurSel(this->comboBoxCategory0, newCurrentIndex);
            ComboBox_ResetContent(this->comboBoxCategory1);
        }
    } else if (level == 1) {
        int newCurrentIndex = ComboBox_GetCurSel(this->comboBoxCategory1);
        if (v == -1)
            newCurrentIndex = 0;
        else {
            for (int i = 0; i < static_cast<int>(this->categories1.size()); i++) {
                int c = this->categories1.at(i).at(0).toInt();
                if (c == v) {
                    newCurrentIndex = i + 1;
                    break;
                }
            }
        }
        if (ComboBox_GetCurSel(this->comboBoxCategory1) != newCurrentIndex) {
            ComboBox_SetCurSel(this->comboBoxCategory1, newCurrentIndex);
        }
    }

    this->categoryCombosEvents = true;
}

void MainFrame::chooseColumns()
{
    /* todo
    QDialog* d = new QDialog(this);
    d->setWindowTitle(QObject::tr("Choose columns"));
    QVBoxLayout* layout = new QVBoxLayout();
    d->setLayout(layout);
    QListWidget* list = new QListWidget(d);
    d->layout()->addWidget(list);
    QDialogButtonBox* dbb = new QDialogButtonBox(QDialogButtonBox::Ok |
            QDialogButtonBox::Cancel);
    d->layout()->addWidget(dbb);
    connect(dbb, SIGNAL(accepted()), d, SLOT(accept()));
    connect(dbb, SIGNAL(rejected()), d, SLOT(reject()));

    QTableView* t = this->ui->tableWidget;
    QAbstractItemModel* m = t->model();
    QHeaderView* hv = t->horizontalHeader();

    for (int i = 0; i < m->columnCount(); i++) {
        QListWidgetItem* item = new QListWidgetItem(m->headerData(i,
                Qt::Horizontal, Qt::DisplayRole).toString(), list);
        item->setCheckState(!hv->isSectionHidden(i) ?
                Qt::Checked : Qt::Unchecked);
    }

    if (d->exec()) {
        for (int i = 0; i < m->columnCount(); i++) {
            QListWidgetItem* item = list->item(i);
            hv->setSectionHidden(i, item->checkState() == Qt::Unchecked);
        }
    }
    d->deleteLater();
    */
}

int MainFrame::getRowCount() const
{
    return packages.size();
}

void MainFrame::setStatusFilter(int status)
{
    if (status == 1)
        Button_SetCheck(buttonInstalled, BST_CHECKED);
    else if (status == 2)
        Button_SetCheck(buttonUpdateable, BST_CHECKED);
    else
        Button_SetCheck(buttonAll, BST_CHECKED);
}

std::vector<void*> MainFrame::getSelected(const QString& type) const
{
    std::vector<void*> res;
    if (type == "Package") {
        std::vector<Package*> ps = this->getSelectedPackagesInTable();
        for (auto p: ps) {
            res.push_back(p);
        }
    }
    return res;
}

std::vector<Package*> MainFrame::getSelectedPackagesInTable() const
{
    return this->selectedPackages;
}

void MainFrame::setSelectedPackages(const std::vector<QString> &packageNames)
{
    /* TODO
    QTableView* t = this->mainFrame->getTableWidget();
    t->clearSelection();
    QAbstractItemModel* m = t->model();
    for (int i = 0; i < m->rowCount(); i++) {
        const QVariant v = m->data(m->index(i, 1), Qt::UserRole);
        QString name = v.toString();
        if (packageNames.count(name) > 0) {
            QModelIndex topLeft = t->model()->index(i, 0);

            t->selectionModel()->select(topLeft, QItemSelectionModel::Rows |
                    QItemSelectionModel::Select);
        }
    }
    */
}

void MainFrame::on_tableWidget_doubleClicked(QModelIndex /*index*/)
{
    MainWindow* mw = MainWindow::getInstance();
    QAction *a = mw->findChild<QAction *>("actionShow_Details");
    if (a)
        a->trigger();
}

void MainFrame::on_lineEditText_textChanged(QString )
{
    fillList();
    selectSomething();
}

void MainFrame::tableWidget_selectionChanged()
{
    qDeleteAll(this->selectedPackages);
    this->selectedPackages.clear();

    DBRepository* r = DBRepository::getDefault();

    // Get the first selected item
    int iPos = ListView_GetNextItem(table, -1, LVNI_SELECTED);
    while (iPos != -1) {
        Package* p = r->findPackage_(packages.at(iPos));
        this->selectedPackages.push_back(p);

        // Get the next selected item
        iPos = ListView_GetNextItem(table, iPos, LVNI_SELECTED);
    }

    MainWindow::getInstance()->updateActions();
}

void MainFrame::fillList()
{
    MainWindow* mw = MainWindow::getInstance();
    mw->fillListInBackground();
}

void MainFrame::selectSomething()
{
    if (ListView_GetSelectedCount(table) == 0 && packages.size() > 0) {
        ListView_SetItemState(table, 0, LVIS_SELECTED, LVIS_SELECTED);
    }
}

void MainFrame::on_comboBoxCategory0_currentIndexChanged(int /*index*/)
{
    if (categoryCombosEvents) {
        fillList();
        selectSomething();
    }
}

void MainFrame::on_comboBoxCategory1_currentIndexChanged(int /*index*/)
{
    if (categoryCombosEvents) {
        fillList();
        selectSomething();
    }
}

MainFrame::Info* MainFrame::createInfo(
        Package* p) const
{
    Info* r = new Info();

    DBRepository* rep = DBRepository::getDefault();

    // error is ignored here
    QString err;
    std::vector<PackageVersion*> pvs = rep->getPackageVersions_(p->name, &err);

    PackageVersion* newestInstallable = nullptr;
    PackageVersion* newestInstalled = nullptr;
    for (auto pv: pvs) {
        if (pv->installed()) {
            if (!r->installed.isEmpty())
                r->installed.append(", ");
            r->installed.append(pv->version.getVersionString());
            if (!newestInstalled ||
                    newestInstalled->version.compare(pv->version) < 0)
                newestInstalled = pv;
        }

        if (pv->download.isValid()) {
            if (!newestInstallable ||
                    newestInstallable->version.compare(pv->version) < 0)
                newestInstallable = pv;
        }
    }

    if (newestInstallable) {
        r->avail = newestInstallable->version.getVersionString();
        r->newestDownloadURL = newestInstallable->download.toString(
                QUrl::FullyEncoded);
    }

    r->up2date = !(newestInstalled && newestInstallable &&
            newestInstallable->version.compare(
            newestInstalled->version) > 0);

    qDeleteAll(pvs);
    pvs.clear();

    QString s = p->description;
    if (s.length() > 200) {
        s = s.left(200) + "...";
    }
    r->shortenDescription = s;

    r->title = p->title;

    // the error message is ignored
    License* lic = rep->findLicense_(p->license, &err);
    if (lic) {
        r->licenseTitle = lic->title;
        delete lic;
    }

    r->icon = p->getIcon();

    if (p->categories.size() > 0) {
        r->category = p->categories.at(0);
    } else {
        r->category.clear();
    }

    r->tags = WPMUtils::join(p->tags, QStringLiteral(", "));

    r->stars = p->stars;

    return r;
}

QString MainFrame::getCellText(int row, int column) const
{
    QString p = this->packages.at(row);

    QString r;
    DBRepository* rep = DBRepository::getDefault();
    Info* cached = this->cache.object(p);
    bool insertIntoCache = false;
    if (!cached) {
        Package* pk = rep->findPackage_(p);
        cached = createInfo(pk);
        delete pk;
        insertIntoCache = true;
    }

    switch (column) {
    case 0:
        r = cached->title;
        break;
    case 1: {
        r = cached->shortenDescription;
        break;
    }
    case 2: {
        r = cached->avail;
        break;
    }
    case 3: {
        r = cached->installed;
        break;
    }
    case 4: {
        r = cached->licenseTitle;
        break;
    }
    case 5: {
        MainWindow* mw = MainWindow::getInstance();
        if (cached->newestDownloadURL.isEmpty()) {
            r = "";
        } else {
            int64_t sz = mw->fileLoader.downloadSizeOrQueue(
                        cached->newestDownloadURL);
            if (sz >= 0)
                r = QString::number(
                            static_cast<double>(sz) /
                            (1024.0 * 1024.0), 'f', 1) +
                        " MiB";
            else if (sz == -1)
                r = QObject::tr("computing");
            else
                r = "";
        }
        break;
    }
    case 6: {
        r = cached->category;
        break;
    }
    case 7: {
        r = cached->tags;
        break;
    }
    case 8: {
        if (cached->stars > 0)
            r = QString::number(cached->stars);
        break;
    }
    }

/* TODO        } else if (role == Qt::UserRole) {
        switch (index.column()) {
            case 0:
                r = QVariant::fromValue(cached->icon);
                break;
            default:
                r = p;
        }
} else if (role == Qt::DecorationRole) {
        switch (index.column()) {
            case 0: {
                MainWindow* mw = MainWindow::getInstance();
                if (!cached->icon.isEmpty()) {
                    r = QVariant::fromValue(mw->downloadIcon(cached->icon));
                } else {
                    r = QVariant::fromValue(MainWindow::genericAppIcon);
                }
                break;
            }
        }
    } else if (role == Qt::BackgroundRole) {
        switch (index.column()) {
            case 4: {
                if (!cached->up2date)
                    r = QVariant::fromValue(obsoleteBrush);
                break;
            }
            case 9: {
                if (cached->stars > 0) {
                    if (maxStars < 0) {
                        QString err;
                        maxStars = rep->getMaxStars(&err);
                    }

                    int f = static_cast<int>(100.0 *
                        (1.0 + log(maxStars / cached->stars)));
                    r = QVariant::fromValue(QBrush(QColor(
                            0xd4, 0xed, 0xda).lighter(f)));
                }
                break;
            }
        }*/

    if (insertIntoCache)
        this->cache.insert(p, cached);

    return r;
}

void MainFrame::setPackages(const std::vector<QString>& packages)
{
    this->packages = packages;

    ListView_SetItemCountEx(table, this->packages.size(),
        LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);

    InvalidateRect(table, NULL, false);
}

void MainFrame::iconUpdated(const QString &/*url*/)
{
    InvalidateRect(table, NULL, false);
}

void MainFrame::downloadSizeUpdated(const QString &/*url*/)
{
    InvalidateRect(table, NULL, false);
}

void MainFrame::installedStatusChanged(const QString& package,
        const Version& /*version*/)
{
    this->cache.remove(package);

    auto it = find(this->packages.begin(), this->packages.end(), package);
    if (it != this->packages.end()) {
        int index = it - this->packages.begin();
        ListView_RedrawItems(table, index, index);
    }
}

void MainFrame::clearCache()
{
    this->cache.clear();

    InvalidateRect(table, NULL, false);
}

void MainFrame::contextMenu(int x, int y)
{
    // TODO: icons loaded twice: here and in the main menu
    HMENU m = CreatePopupMenu();
    t_gui_menu_append_item(m, IDM_INSTALL, QObject::tr("&Install"),
                           t_gui_load_png_resource(L"install16_png"));
    t_gui_menu_append_item(m, IDM_UNINSTALL, QObject::tr("U&ninstall"),
                           t_gui_load_png_resource(L"uninstall16_png"));
    t_gui_menu_append_item(m, IDM_UPDATE, QObject::tr("&Update"),
                           t_gui_load_png_resource(L"update16_png"));
    t_gui_menu_append_item(m, IDM_SHOW_DETAILS, QObject::tr("Show details"), NULL);
    t_gui_menu_append_item(m, IDM_SHOW_CHANGELOG,
        QObject::tr("Show changelog"), NULL);
    t_gui_menu_append_item(m, IDM_RUN, QObject::tr("Run"), NULL);
    t_gui_menu_append_item(m, IDM_OPEN_FOLDER, QObject::tr("Open folder"), NULL);
    t_gui_menu_append_item(m, IDM_OPEN_WEB_SITE, QObject::tr("&Open web site"),
                t_gui_load_png_resource(L"gotosite16_png"));
    t_gui_menu_append_item(m, IDM_TEST_DOWNLOAD_SITE,
        QObject::tr("&Test download site"), NULL);
    t_gui_menu_append_item(m, IDM_EXPORT, QObject::tr("Export..."), NULL);

    SetForegroundWindow(this->packagesPanel);

    // TODO: TPM_LEFGALIGN: see remarks on https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-trackpopupmenu
    TrackPopupMenu(m, TPM_TOPALIGN | TPM_LEFTALIGN, x, y, 0, this->packagesPanel, NULL);

    DestroyMenu(m);
}
