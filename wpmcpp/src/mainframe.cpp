#include <algorithm>

#include "mainframe.h"
#include "ui_mainframe.h"

#include <QDebug>
#include <QList>
#include <QListWidgetItem>
#include <QListWidget>
#include <QDialogButtonBox>

#include "mainwindow.h"
#include "package.h"
#include "packageitemmodel.h"
#include "windowsregistry.h"

MainFrame::MainFrame(QWidget *parent) :
    QFrame(parent), Selection(),
    ui(new Ui::MainFrame)
{
    ui->setupUi(this);

    this->categoryCombosEvents = true;

    QTableView* t = this->ui->tableWidget;
    t->setModel(new PackageItemModel(QList<Package*>()));
    t->setEditTriggers(QTableWidget::NoEditTriggers);
    t->setSortingEnabled(false);
    t->horizontalHeader()->setSectionsMovable(true);

    t->verticalHeader()->setDefaultSectionSize(36);
    t->setColumnWidth(0, 40);
    t->setColumnWidth(1, 150);
    t->setColumnWidth(2, 300);
    t->setColumnWidth(3, 100);
    t->setColumnWidth(4, 100);
    t->setColumnWidth(5, 100);
    t->setColumnWidth(6, 100);
    t->horizontalHeader()->setSectionHidden(6, true);
    t->setIconSize(QSize(32, 32));

    connect(t->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this,
            SLOT(tableWidget_selectionChanged()));
}

MainFrame::~MainFrame()
{
    delete ui;
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
        QTableView* t = this->ui->tableWidget;
        wrr.setBytes("MainTableState", t->horizontalHeader()->saveState());
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
            QTableView* t = this->ui->tableWidget;
            t->horizontalHeader()->restoreState(ba);
        }
    } else {
        QString err = wr.open(HKEY_CURRENT_USER,
                "Software\\Npackd\\Npackd\\TableColumns", false,
                KEY_READ);

        QStringList v;
        if (err.isEmpty()) {
            v = wr.loadStringList(&err);
        }

        if (err.isEmpty()) {
            QTableView* t = this->ui->tableWidget;
            for (int i = 0; i < std::min(t->model()->columnCount(),
                    v.count()); i++) {
                bool ok;
                int w = v.at(i).toInt(&ok);
                if (!ok)
                    break;

                t->setColumnWidth(i, w);
            }
        }
    }
}

void MainFrame::setCategories(int level, const QList<QStringList> &cats)
{
    this->categoryCombosEvents = false;

    QStringList labels;
    int count = 0;
    for (int i = 0; i < cats.count(); i++) {
        QString n;
        n = cats.at(i).at(2);
        if (n.isEmpty()) {
            n = QObject::tr("Uncategorized");
        }
        labels.append(n + " (" + cats.at(i).at(1) + ")");
        count += cats.at(i).at(1).toInt();
    }

    if (level == 0) {
        this->ui->comboBoxCategory0->clear();
        this->ui->comboBoxCategory0->addItem(QObject::tr("All") +
                " (" + QString::number(count) + ")");
        this->ui->comboBoxCategory0->addItems(labels);
        this->ui->comboBoxCategory0->setEnabled(true);
        this->categories0 = cats;

        this->categories1.clear();
        this->ui->comboBoxCategory1->setEnabled(false);
    } else if (level == 1) {
        this->ui->comboBoxCategory1->clear();
        if (labels.count() > 0) {
            this->ui->comboBoxCategory1->addItem(QObject::tr("All") +
                    " (" + QString::number(count) + ")");
            this->ui->comboBoxCategory1->addItems(labels);
            this->ui->comboBoxCategory1->setEnabled(true);
        } else {
            this->ui->comboBoxCategory1->setEnabled(false);
        }
        this->categories1 = cats;
    }

    this->categoryCombosEvents = true;
}

int MainFrame::getCategoryFilter(int level) const
{
    int r;
    if (level == 0) {
        int sel = this->ui->comboBoxCategory0->currentIndex();
        if (sel <= 0)
            r = -1;
        else
            r = this->categories0.at(sel - 1).at(0).toInt();
    } else if (level == 1) {
        int sel = this->ui->comboBoxCategory1->currentIndex();
        if (sel <= 0)
            r = -1;
        else
            r = this->categories1.at(sel - 1).at(0).toInt();
    } else
        r = -1;
    return r;
}

void MainFrame::setCategoryFilter(int level, int v)
{
    this->categoryCombosEvents = false;

    if (level == 0) {
        int newCurrentIndex = this->ui->comboBoxCategory0->currentIndex();
        if (v == -1)
            newCurrentIndex = 0;
        else {
            for (int i = 0; i < this->categories0.count(); i++) {
                int c = this->categories0.at(i).at(0).toInt();
                if (c == v) {
                    newCurrentIndex = i + 1;
                    break;
                }
            }
        }
        if (this->ui->comboBoxCategory0->currentIndex() != newCurrentIndex) {
            this->ui->comboBoxCategory0->setCurrentIndex(newCurrentIndex);
            this->ui->comboBoxCategory1->clear();
        }
    } else if (level == 1) {
        int newCurrentIndex = this->ui->comboBoxCategory1->currentIndex();
        if (v == -1)
            newCurrentIndex = 0;
        else {
            for (int i = 0; i < this->categories1.count(); i++) {
                int c = this->categories1.at(i).at(0).toInt();
                if (c == v) {
                    newCurrentIndex = i + 1;
                    break;
                }
            }
        }
        if (this->ui->comboBoxCategory1->currentIndex() != newCurrentIndex) {
            this->ui->comboBoxCategory1->setCurrentIndex(newCurrentIndex);
        }
    }

    this->categoryCombosEvents = true;
}

void MainFrame::chooseColumns()
{
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
}

QTableView * MainFrame::getTableWidget() const
{
    return this->ui->tableWidget;
}

QLineEdit *MainFrame::getFilterLineEdit() const
{
    return this->ui->lineEditText;
}

int MainFrame::getStatusFilter() const
{
    int r;
    if (this->ui->radioButtonInstalled->isChecked())
        r = 1;
    else if (this->ui->radioButtonUpdateable->isChecked())
        r = 2;
    else
        r = 0;

    return r;
}

QList<void*> MainFrame::getSelected(const QString& type) const
{
    QList<void*> res;
    if (type == "Package") {
        QList<Package*> ps = this->getSelectedPackagesInTable();
        for (int i = 0; i < ps.count(); i++) {
            res.append(ps.at(i));
        }
    }
    return res;
}

Package* MainFrame::getSelectedPackageInTable()
{
    QAbstractItemModel* m = this->ui->tableWidget->model();
    QItemSelectionModel* sm = this->ui->tableWidget->selectionModel();
    QModelIndexList sel = sm->selectedRows();
    if (sel.count() > 0) {
        QModelIndex index = m->index(sel.at(0).row(), 1);
        const QVariant v = index.data(Qt::UserRole);
        Package* p = (Package*) v.value<void*>();
        return p;
    }
    return 0;
}

QList<Package*> MainFrame::getSelectedPackagesInTable() const
{
    QList<Package*> result;
    QAbstractItemModel* m = this->ui->tableWidget->model();
    QItemSelectionModel* sm = this->ui->tableWidget->selectionModel();
    QModelIndexList sel = sm->selectedRows();
    for (int i = 0; i < sel.count(); i++) {
        QModelIndex index = m->index(sel.at(i).row(), 1);
        const QVariant v = index.data(Qt::UserRole);
        Package* p = (Package*) v.value<void*>();
        result.append(p);
    }
    return result;
}

void MainFrame::on_tableWidget_doubleClicked(QModelIndex index)
{
    MainWindow* mw = MainWindow::getInstance();
    QAction *a = mw->findChild<QAction *>("actionShow_Details");
    if (a)
        a->trigger();
}

void MainFrame::on_lineEditText_textChanged(QString )
{
    fillList();
}

void MainFrame::tableWidget_selectionChanged()
{
    MainWindow::getInstance()->updateActions();
}

void MainFrame::fillList()
{
    MainWindow* mw = MainWindow::getInstance();
    mw->fillList();
}

void MainFrame::on_radioButtonAll_toggled(bool checked)
{
    fillList();
}

void MainFrame::on_radioButtonInstalled_toggled(bool checked)
{
    fillList();
}

void MainFrame::on_radioButtonUpdateable_toggled(bool checked)
{
    fillList();
}

void MainFrame::on_comboBoxCategory0_currentIndexChanged(int index)
{
    if (categoryCombosEvents) {
        fillList();
    }
}

void MainFrame::on_comboBoxCategory1_currentIndexChanged(int index)
{
    if (categoryCombosEvents) {
        fillList();
    }
}
