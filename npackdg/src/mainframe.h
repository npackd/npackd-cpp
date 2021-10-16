#ifndef MAINFRAME_H
#define MAINFRAME_H

#include <QFrame>
#include <QFrame>
#include <QString>
#include <QTableWidget>
#include <QComboBox>
#include <QTreeWidget>
#include <QCache>

#include "package.h"
#include "selection.h"
#include "version.h"

/**
 * Main frame
 */
class MainFrame : public QObject, public Selection
{
    Q_OBJECT
private:
    /** true = events from the category comboboxes are enabled */
    bool categoryCombosEvents;

    /** ID, COUNT, NAME */
    std::vector<std::vector<QString>> categories0, categories1;

    std::vector<Package*> selectedPackages;

    /** label where the search duration is shown */
    HWND labelDuration;

    /** filter "all" */
    HWND buttonAll;

    /** filter "installed" */
    HWND buttonInstalled;

    /** filter "updateable" */
    HWND buttonUpdateable;

    /** top-level category filter */
    HWND comboBoxCategory0;

    /** second level category filter */
    HWND comboBoxCategory1;

    /** panel for packages */
    HWND packagesPanel;

    QBrush obsoleteBrush;

    mutable int maxStars;

    std::vector<QString> packages;

    /**
     * @brief package information for one row
     */
    class Info {
    public:
        QString avail;
        QString installed;
        bool up2date;
        QString newestDownloadURL;
        QString shortenDescription;
        QString title;
        QString licenseTitle;
        QString icon;
        QString category;
        QString tags;
        int stars;
    };

    mutable QCache<QString, Info> cache;

    Info *createInfo(Package *p) const;

    void fillList();

    /**
     * @brief creates the table with packages
     * @param parent parent window
     * @return window handle
     */
    HWND createTable(HWND parent);
public:
    /** table with packages */
    HWND table;

    /** edit field for the filter */
    HWND filterLineEdit;

    explicit MainFrame(QWidget *parent = nullptr);
    ~MainFrame();

    /**
     * @brief filter for the status
     * @return 0=All, 1=Installed, 2=Updateable
     */
    int getStatusFilter() const;

    /**
     * @brief returns selected category
     * @param level 0, 1, ...
     * @return category ID or -1 for "All" or 0 for "Uncategorized"
     */
    int getCategoryFilter(int level) const;

    std::vector<void*> getSelected(const QString& type) const;

    /**
     * @brief layout the controls on the packages page
     */
    void packagesPanelLayout();

    /**
     * @brief create the panel with packages table and all the fiters
     *
     * @param parent parent window
     * @return handle
     */
    HWND createPackagesPanel(HWND parent);

    /**
     * This method returns all selected Package* items
     *
     * @return selected packages
     */
    std::vector<Package *> getSelectedPackagesInTable() const;

    /**
     * @return table with packages
     */
    QTableView *getTableWidget() const;

    /**
     * @return filter line edit
     */
    QLineEdit* getFilterLineEdit() const;

    /**
     * @return the number of packages
     */
    int getRowCount() const;

    /**
     * @brief saves the column widths in the Windows registry
     */
    void saveColumns() const;

    /**
     * @brief load column widths from the Windows registry
     */
    void loadColumns() const;

    /**
     * @brief changes the list of available categories. The chosen category is
     *     reset
     * @param level category level (0, 1, ...)
     * @param cats new categories: ID, COUNT, NAME
     */
    void setCategories(int level, const std::vector<std::vector<QString> > &cats);

    /**
     * @brief changes the chosen category. Changing the category at level 0 will
     *     reset the filters at levels 1, 2, 3, ... etc.
     * @param level 0, 1, ...
     * @param v new filter value or -1 for "All" or 0 for "Uncategorized"
     */
    void setCategoryFilter(int level, int v);

    /**
     * @brief shows a dialog to choose the visible columns
     */
    void chooseColumns();

    /**
     * @brief changes the search duration shown in the UI
     * @param d duration in milliseconds
     */
    void setDuration(int d);

    /**
     * @brief changes the status filter
     * @param status new filter. 0=All, 1=Installed, 2=Updateable
     */
    void setStatusFilter(int status);

    /**
     * @brief cell text
     * @param row row index
     * @param column column index
     * @return text
     */
    QString getCellText(int row, int column) const;

    /**
     * @brief changes the list of packages
     * @param packages list of package names
     */
    void setPackages(const std::vector<QString> &packages);

    /**
     * @brief should be called if an icon has changed
     * @param url URL of the icon
     */
    void iconUpdated(const QString& url);

    /**
     * @brief should be called if the "installed" status of a package version
     *     has changed
     * @param package full package name
     * @param version version number
     */
    void installedStatusChanged(const QString &package, const Version &version);

    /**
     * @brief clears the cache
     */
    void clearCache();

    /**
     * @brief should be called if a download size for a package was computed
     * @param url URL of the binary
     */
    void downloadSizeUpdated(const QString &url);
private:
    void selectSomething();
private slots:
    void on_tableWidget_doubleClicked(QModelIndex index);
    void on_lineEditText_textChanged(QString );
    void tableWidget_selectionChanged();
    void on_radioButtonAll_toggled(bool checked);
    void on_radioButtonInstalled_toggled(bool checked);
    void on_radioButtonUpdateable_toggled(bool checked);
    void on_comboBoxCategory0_currentIndexChanged(int index);
    void on_comboBoxCategory1_currentIndexChanged(int index);
};

#endif // MAINFRAME_H
