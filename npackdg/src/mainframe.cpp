#include <algorithm>

#include "mainframe.h"
#include "ui_mainframe.h"

#include <QListWidgetItem>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QAction>

#include "dbrepository.h"
#include "mainwindow.h"
#include "package.h"
#include "packageitemmodel.h"
#include "windowsregistry.h"
#include "abstractrepository.h"
#include "uiutils.h"
#include "wpmutils.h"

MainFrame::MainFrame(QWidget *parent) :
    QFrame(parent), Selection(),
    ui(new Ui::MainFrame)
{
    ui->setupUi(this);

    this->categoryCombosEvents = true;

    QTableView* t = this->ui->tableWidget;

    QItemSelectionModel *sm = t->selectionModel();
    QAbstractItemModel* m = t->model();
    t->setModel(new PackageItemModel(std::vector<QString>()));
    delete sm;
    delete m;

    t->setTextElideMode(Qt::ElideRight);
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
    t->setColumnWidth(7, 100);
    t->setColumnWidth(8, 100);
    t->setColumnWidth(9, 20);
    t->horizontalHeader()->setSectionHidden(6, true);
    t->horizontalHeader()->setSectionHidden(7, true);
    t->horizontalHeader()->setSectionHidden(8, true);
    t->horizontalHeader()->setSectionHidden(9, true);
    t->setIconSize(QSize(UIUtils::ICON_SIZE, UIUtils::ICON_SIZE));

    connect(t->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this,
            SLOT(tableWidget_selectionChanged()));
}

MainFrame::~MainFrame()
{
    QTableView* t = this->ui->tableWidget;
    QItemSelectionModel *sm = t->selectionModel();
    QAbstractItemModel* m = t->model();

    qDeleteAll(this->selectedPackages);
    delete ui;

    delete sm;
    delete m;
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

        std::vector<QString> v;
        if (err.isEmpty()) {
            v = wr.loadStringList(&err);
        }

        if (err.isEmpty()) {
            QTableView* t = this->ui->tableWidget;
            for (int i = 0; i < std::min<int>(t->model()->columnCount(),
                    v.size()); i++) {
                bool ok;
                int w = v.at(i).toInt(&ok);
                if (!ok)
                    break;

                t->setColumnWidth(i, w);
            }
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
        this->ui->comboBoxCategory0->clear();
        this->ui->comboBoxCategory0->addItem(QObject::tr("All") +
                " (" + QString::number(count) + ")");
        this->ui->comboBoxCategory0->addItems(WPMUtils::toQStringList(labels));
        this->ui->comboBoxCategory0->setEnabled(true);
        this->categories0 = cats;

        this->categories1.clear();
        this->ui->comboBoxCategory1->setEnabled(false);
    } else if (level == 1) {
        this->ui->comboBoxCategory1->clear();
        if (labels.size() > 0) {
            this->ui->comboBoxCategory1->addItem(QObject::tr("All") +
                    " (" + QString::number(count) + ")");
            this->ui->comboBoxCategory1->addItems(WPMUtils::toQStringList(labels));
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

void MainFrame::setDuration(int d)
{
    this->ui->labelDuration->setText(QObject::tr("Found in %1 ms").arg(d));
}

void MainFrame::setCategoryFilter(int level, int v)
{
    this->categoryCombosEvents = false;

    if (level == 0) {
        int newCurrentIndex = this->ui->comboBoxCategory0->currentIndex();
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
        if (this->ui->comboBoxCategory0->currentIndex() != newCurrentIndex) {
            this->ui->comboBoxCategory0->setCurrentIndex(newCurrentIndex);
            this->ui->comboBoxCategory1->clear();
        }
    } else if (level == 1) {
        int newCurrentIndex = this->ui->comboBoxCategory1->currentIndex();
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

void MainFrame::setStatusFilter(int status)
{
    if (status == 1)
        this->ui->radioButtonInstalled->setChecked(true);
    else if (status == 2)
        this->ui->radioButtonUpdateable->setChecked(true);
    else
        this->ui->radioButtonAll->setChecked(true);
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

    QAbstractItemModel* m = this->ui->tableWidget->model();
    QItemSelectionModel* sm = this->ui->tableWidget->selectionModel();
    QModelIndexList sel = sm->selectedRows();
    DBRepository* r = DBRepository::getDefault();
    for (int i = 0; i < sel.count(); i++) {
        QModelIndex index = m->index(sel.at(i).row(), 1);
        const QVariant v = index.data(Qt::UserRole);
        QString name = v.toString();
        Package* p = r->findPackage_(name);
        this->selectedPackages.push_back(p);
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
    QItemSelectionModel* sm = this->ui->tableWidget->selectionModel();
    if (!sm->hasSelection()) {
        this->ui->tableWidget->selectRow(0);
    }
}

void MainFrame::on_radioButtonAll_toggled(bool /*checked*/)
{
    fillList();
    selectSomething();
}

void MainFrame::on_radioButtonInstalled_toggled(bool /*checked*/)
{
    fillList();
    selectSomething();
}

void MainFrame::on_radioButtonUpdateable_toggled(bool /*checked*/)
{
    fillList();
    selectSomething();
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
