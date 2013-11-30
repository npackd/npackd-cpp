#ifndef MAINFRAME_H
#define MAINFRAME_H

#include <QFrame>
#include <QFrame>
#include <QList>
#include <QString>
#include <QTableWidget>
#include <QComboBox>
#include <QTreeWidget>

#include "package.h"
#include "selection.h"

namespace Ui {
    class MainFrame;
}

/**
 * Main frame
 */
class MainFrame : public QFrame, public Selection
{
    Q_OBJECT
private:
    /** true = events from the category comboboxes are enabled */
    bool categoryCombosEvents;

    /** ID, COUNT, NAME */
    QList<QStringList> categories0, categories1;

    void fillList();
public:
    explicit MainFrame(QWidget *parent = 0);
    ~MainFrame();

    QList<void*> getSelected(const QString& type) const;

    /**
     * This method returns a non-null Package* if something is selected
     * in the table.
     *
     * @return selected package or 0.
     */
    Package* getSelectedPackageInTable();

    /**
     * This method returns all selected Package* items
     *
     * @return [ownership:this] selected packages
     */
    QList<Package*> getSelectedPackagesInTable() const;

    /**
     * @return table with packages
     */
    QTableView *getTableWidget() const;

    /**
     * @return filter line edit
     */
    QLineEdit* getFilterLineEdit() const;

    /**
     * @brief filter for the status
     * @return 0=All, 1=Installed, 2=Updateable
     */
    int getStatusFilter() const;

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
    void setCategories(int level, const QList<QStringList> &cats);

    /**
     * @brief returns selected category
     * @param level 0, 1, ...
     * @return category ID or -1 for "All" or 0 for "Uncategorized"
     */
    int getCategoryFilter(int level) const;

    /**
     * @brief changes the chosen category. Changing the category at level 0 will
     *     reset the filters at levels 1, 2, 3, ... etc.
     * @param level 0, 1, ...
     * @param v new filter value or -1 for "All" or 0 for "Uncategorized"
     */
    void setCategoryFilter(int level, int v);
private:
    Ui::MainFrame *ui;
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
