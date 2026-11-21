#include "ui/mainwindow.h"
#include "ui/bagdialog.h"
#include "ui/employeedialog.h"
#include "ui/processdialog.h"
#include "ui/placeholderpage.h"
#include "db/dbmanager.h"
#include "service/bagservice.h"
#include "service/employeeservice.h"
#include "service/processservice.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabs(new QTabWidget(this))
    , m_dbInfo(new QLabel(this))
    , m_employeeTable(new QTableWidget(this))
    , m_editButton(0)
    , m_removeButton(0)
    , m_processTable(new QTableWidget(this))
    , m_editProcessButton(0)
    , m_removeProcessButton(0)
    , m_bagTable(new QTableWidget(this))
    , m_editBagButton(0)
    , m_removeBagButton(0)
{
    setWindowTitle(QStringLiteral("书包生产记账系统 — bagwage"));
    resize(1000, 640);

    // 顶部信息条：库文件位置 + 已建表，方便一眼确认环境对不对
    QStringList tables = DatabaseManager::existingTables();
    m_dbInfo->setText(QStringLiteral("数据库：%1　　已建表：%2")
                          .arg(QDir::toNativeSeparators(DatabaseManager::databasePath()))
                          .arg(tables.isEmpty() ? QStringLiteral("（无）")
                                                : tables.join(QStringLiteral(", "))));
    m_dbInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_dbInfo->setStyleSheet(QStringLiteral("color:#555; padding:4px;"));

    m_tabs->addTab(createEmployeePage(), QStringLiteral("员工"));
    m_tabs->addTab(createProcessPage(), QStringLiteral("工序"));
    m_tabs->addTab(createBagPage(), QStringLiteral("书包"));
    m_tabs->addTab(makePlaceholderPage(
                       QStringLiteral("订单管理"),
                       QStringLiteral("订单 = 某楼层生产某书包的数量；明细 = 谁做了哪道工序、做了多少。"
                                      "<b>分页必须按订单算</b>，一页 10 个订单。"),
                       QStringList() << QStringLiteral("新建订单 + 一次性批量录入明细（事务写入）")
                                     << QStringLiteral("订单列表分页（DISTINCT order_id 子查询）")
                                     << QStringLiteral("按员工查看其参与的订单，同样按订单分页"))
                   , QStringLiteral("订单"));
    m_tabs->addTab(makePlaceholderPage(
                       QStringLiteral("工资核算"),
                       QStringLiteral("工资 = Σ(明细数量 × <b>bag_process</b> 单价)，"
                                      "按员工、按月份汇总。"),
                       QStringList() << QStringLiteral("选择月份，汇总每个员工的计件工资")
                                     << QStringLiteral("展开某员工查看构成（工序、数量、单价、小计）")
                                     << QStringLiteral("空数据要有提示，不能显示 0 元误导"))
                   , QStringLiteral("工资"));
    m_tabs->addTab(makePlaceholderPage(
                       QStringLiteral("月度统计图表"),
                       QStringLiteral("柱状图（各员工产量对比）+ 饼图（各工序占比）。"
                                      "<b>聚合在子线程做</b>，算完用信号回主线程画图。"),
                       QStringList() << QStringLiteral("子线程自建 SQLite 连接（不能复用 main）")
                                     << QStringLiteral("按月份聚合各员工产量 / 各工序件数")
                                     << QStringLiteral("空数据直接返回，不要对空容器取最大值")
                                     << QStringLiteral("QChartView 放进本页"))
                   , QStringLiteral("图表"));

    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->addWidget(m_dbInfo);
    layout->addWidget(m_tabs, 1);
    setCentralWidget(central);

    statusBar()->showMessage(QStringLiteral("就绪"));
    refreshEmployees();
    refreshProcesses();
    refreshBags();
}

QWidget *MainWindow::createEmployeePage()
{
    QWidget *page = new QWidget(this);

    m_employeeTable->setColumnCount(3);
    m_employeeTable->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("ID") << QStringLiteral("姓名") << QStringLiteral("楼层"));
    m_employeeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_employeeTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_employeeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_employeeTable->verticalHeader()->setVisible(false);
    m_employeeTable->horizontalHeader()->setStretchLastSection(true);
    m_employeeTable->setColumnWidth(0, 60);

    QPushButton *addButton = new QPushButton(QStringLiteral("新增"), page);
    m_editButton = new QPushButton(QStringLiteral("编辑"), page);
    m_removeButton = new QPushButton(QStringLiteral("删除"), page);
    QPushButton *refreshButton = new QPushButton(QStringLiteral("刷新"), page);

    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addWidget(addButton);
    buttons->addWidget(m_editButton);
    buttons->addWidget(m_removeButton);
    buttons->addWidget(refreshButton);
    buttons->addStretch(1);

    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->addLayout(buttons);
    layout->addWidget(m_employeeTable, 1);

    connect(addButton, SIGNAL(clicked()), this, SLOT(addEmployee()));
    connect(m_editButton, SIGNAL(clicked()), this, SLOT(editEmployee()));
    connect(m_removeButton, SIGNAL(clicked()), this, SLOT(removeEmployee()));
    connect(refreshButton, SIGNAL(clicked()), this, SLOT(refreshEmployees()));

    return page;
}

QWidget *MainWindow::createProcessPage()
{
    QWidget *page = new QWidget(this);

    m_processTable->setColumnCount(3);
    m_processTable->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("ID") << QStringLiteral("工序名称")
                      << QStringLiteral("通用单价（元/件）"));
    m_processTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_processTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_processTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_processTable->verticalHeader()->setVisible(false);
    m_processTable->horizontalHeader()->setStretchLastSection(true);
    m_processTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_processTable->setColumnWidth(0, 60);

    QPushButton *addButton = new QPushButton(QStringLiteral("新增"), page);
    m_editProcessButton = new QPushButton(QStringLiteral("编辑"), page);
    m_removeProcessButton = new QPushButton(QStringLiteral("删除"), page);
    QPushButton *refreshButton = new QPushButton(QStringLiteral("刷新"), page);

    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addWidget(addButton);
    buttons->addWidget(m_editProcessButton);
    buttons->addWidget(m_removeProcessButton);
    buttons->addWidget(refreshButton);
    buttons->addStretch(1);

    QLabel *hint = new QLabel(
        QStringLiteral("提示：这里的「通用单价」只作参考。实际算工资用的是"
                       "「书包 × 工序」单价（在书包页配置）。"), page);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#777;"));

    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->addLayout(buttons);
    layout->addWidget(hint);
    layout->addWidget(m_processTable, 1);

    connect(addButton, SIGNAL(clicked()), this, SLOT(addProcess()));
    connect(m_editProcessButton, SIGNAL(clicked()), this, SLOT(editProcess()));
    connect(m_removeProcessButton, SIGNAL(clicked()), this, SLOT(removeProcess()));
    connect(refreshButton, SIGNAL(clicked()), this, SLOT(refreshProcesses()));

    return page;
}

QWidget *MainWindow::createBagPage()
{
    QWidget *page = new QWidget(this);

    m_bagTable->setColumnCount(3);
    m_bagTable->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("ID") << QStringLiteral("书包名称")
                      << QStringLiteral("已配置单价的工序数"));
    m_bagTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_bagTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_bagTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_bagTable->verticalHeader()->setVisible(false);
    // 全部固定列宽：Stretch 的中间列会把后面的列挤出视口，这里不需要自适应
    m_bagTable->horizontalHeader()->setStretchLastSection(false);
    m_bagTable->setColumnWidth(0, 60);
    m_bagTable->setColumnWidth(1, 520);
    m_bagTable->setColumnWidth(2, 170);

    QPushButton *addButton = new QPushButton(QStringLiteral("新增"), page);
    m_editBagButton = new QPushButton(QStringLiteral("编辑 / 单价"), page);
    m_removeBagButton = new QPushButton(QStringLiteral("删除"), page);
    QPushButton *refreshButton = new QPushButton(QStringLiteral("刷新"), page);

    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addWidget(addButton);
    buttons->addWidget(m_editBagButton);
    buttons->addWidget(m_removeBagButton);
    buttons->addWidget(refreshButton);
    buttons->addStretch(1);

    QLabel *hint = new QLabel(
        QStringLiteral("同一款书包对每道工序可以定不同单价；点「编辑 / 单价」维护。"
                       "只配了部分工序时，没配的工序在工资核算里会单独提示「未配置单价」。"),
        page);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#777;"));

    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->addLayout(buttons);
    layout->addWidget(hint);
    layout->addWidget(m_bagTable, 1);

    connect(addButton, SIGNAL(clicked()), this, SLOT(addBag()));
    connect(m_editBagButton, SIGNAL(clicked()), this, SLOT(editBag()));
    connect(m_removeBagButton, SIGNAL(clicked()), this, SLOT(removeBag()));
    connect(refreshButton, SIGNAL(clicked()), this, SLOT(refreshBags()));

    return page;
}

QWidget *MainWindow::makePlaceholderPage(const QString &title,
                                         const QString &summary,
                                         const QStringList &todo)
{
    return new PlaceholderPage(title, summary, todo, this);
}

// ---------------------------------------------------------------- 员工

void MainWindow::refreshEmployees()
{
    EmployeeService service;
    const QList<Employee> employees = service.listAll();

    m_employeeTable->setRowCount(0);
    m_employeeTable->setRowCount(employees.size());
    for (int row = 0; row < employees.size(); ++row) {
        const Employee &e = employees.at(row);
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(e.id));
        idItem->setData(Qt::UserRole, e.id);
        m_employeeTable->setItem(row, 0, idItem);
        m_employeeTable->setItem(row, 1, new QTableWidgetItem(e.name));
        m_employeeTable->setItem(row, 2, new QTableWidgetItem(e.floor));
    }

    if (m_employeeTable->rowCount() > 0 && currentEmployeeRow() < 0)
        m_employeeTable->selectRow(0);

    statusBar()->showMessage(QStringLiteral("员工共 %1 人").arg(employees.size()));
    m_editButton->setEnabled(m_employeeTable->rowCount() > 0);
    m_removeButton->setEnabled(m_employeeTable->rowCount() > 0);
}

int MainWindow::currentEmployeeRow() const
{
    const QList<QTableWidgetItem *> selected = m_employeeTable->selectedItems();
    if (selected.isEmpty())
        return -1;
    return selected.first()->row();
}

int MainWindow::currentEmployeeId() const
{
    const int row = currentEmployeeRow();
    if (row < 0)
        return -1;
    QTableWidgetItem *item = m_employeeTable->item(row, 0);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

void MainWindow::showServiceFailure(const QString &message)
{
    QMessageBox::warning(this, QStringLiteral("操作未完成"), message);
}

void MainWindow::addEmployee()
{
    EmployeeDialog dialog(this);
    dialog.setEmployee(Employee());
    if (dialog.exec() != QDialog::Accepted)
        return;

    Employee emp = dialog.employee();
    EmployeeService service;
    const ServiceResult result = service.save(&emp);
    if (!result.ok) {
        showServiceFailure(result.message);
        return;
    }
    refreshEmployees();
    statusBar()->showMessage(QStringLiteral("已新增员工：%1").arg(emp.name), 5000);
}

void MainWindow::editEmployee()
{
    const int empId = currentEmployeeId();
    if (empId < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先在表格里选择一位员工"));
        return;
    }

    EmployeeService service;
    Employee current;
    // 直接用列表里的数据，避免 UI 再反向依赖 DAO
    QList<Employee> all = service.listAll();
    bool found = false;
    for (int i = 0; i < all.size(); ++i) {
        if (all.at(i).id == empId) {
            current = all.at(i);
            found = true;
            break;
        }
    }
    if (!found) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("该员工已不存在，请刷新后重试"));
        refreshEmployees();
        return;
    }

    EmployeeDialog dialog(this);
    dialog.setEmployee(current);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Employee edited = dialog.employee();
    const ServiceResult result = service.save(&edited);
    if (!result.ok) {
        showServiceFailure(result.message);
        return;
    }
    refreshEmployees();
    statusBar()->showMessage(QStringLiteral("已保存员工：%1").arg(edited.name), 5000);
}

void MainWindow::removeEmployee()
{
    const int empId = currentEmployeeId();
    if (empId < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先在表格里选择一位员工"));
        return;
    }

    const int row = currentEmployeeRow();
    const QString name = m_employeeTable->item(row, 1)->text();

    const QMessageBox::StandardButton answer =
        QMessageBox::question(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定要删除员工「%1」吗？").arg(name),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    EmployeeService service;
    const ServiceResult result = service.remove(empId);
    if (!result.ok) {
        showServiceFailure(result.message);
        return;
    }
    refreshEmployees();
    statusBar()->showMessage(QStringLiteral("已删除员工：%1").arg(name), 5000);
}

// ---------------------------------------------------------------- 工序

void MainWindow::refreshProcesses()
{
    ProcessService service;
    const QList<Process> processes = service.listAll();

    m_processTable->setRowCount(0);
    m_processTable->setRowCount(processes.size());
    for (int row = 0; row < processes.size(); ++row) {
        const Process &p = processes.at(row);

        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(p.id));
        idItem->setData(Qt::UserRole, p.id);
        m_processTable->setItem(row, 0, idItem);

        m_processTable->setItem(row, 1, new QTableWidgetItem(p.name));

        QTableWidgetItem *priceItem =
            new QTableWidgetItem(QString::number(p.price, 'f', 2));
        priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_processTable->setItem(row, 2, priceItem);
    }

    if (m_processTable->rowCount() > 0 && currentProcessRow() < 0)
        m_processTable->selectRow(0);

    const bool hasRows = m_processTable->rowCount() > 0;
    m_editProcessButton->setEnabled(hasRows);
    m_removeProcessButton->setEnabled(hasRows);
}

int MainWindow::currentProcessRow() const
{
    const QList<QTableWidgetItem *> selected = m_processTable->selectedItems();
    if (selected.isEmpty())
        return -1;
    return selected.first()->row();
}

int MainWindow::currentProcessId() const
{
    const int row = currentProcessRow();
    if (row < 0)
        return -1;
    QTableWidgetItem *item = m_processTable->item(row, 0);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

void MainWindow::addProcess()
{
    ProcessDialog dialog(this);
    dialog.setProcess(Process());
    if (dialog.exec() != QDialog::Accepted)
        return;

    Process p = dialog.process();
    ProcessService service;
    const ServiceResult result = service.save(&p);
    if (!result.ok) {
        showServiceFailure(result.message);
        return;
    }
    refreshProcesses();
    statusBar()->showMessage(QStringLiteral("已新增工序：%1").arg(p.name), 5000);
}

void MainWindow::editProcess()
{
    const int processId = currentProcessId();
    if (processId < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先在表格里选择一道工序"));
        return;
    }

    ProcessService service;
    Process current;
    const QList<Process> all = service.listAll();
    bool found = false;
    for (int i = 0; i < all.size(); ++i) {
        if (all.at(i).id == processId) {
            current = all.at(i);
            found = true;
            break;
        }
    }
    if (!found) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("该工序已不存在，请刷新后重试"));
        refreshProcesses();
        return;
    }

    ProcessDialog dialog(this);
    dialog.setProcess(current);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Process edited = dialog.process();
    const ServiceResult result = service.save(&edited);
    if (!result.ok) {
        showServiceFailure(result.message);
        return;
    }
    refreshProcesses();
    statusBar()->showMessage(QStringLiteral("已保存工序：%1").arg(edited.name), 5000);
}

void MainWindow::removeProcess()
{
    const int processId = currentProcessId();
    if (processId < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先在表格里选择一道工序"));
        return;
    }

    const int row = currentProcessRow();
    const QString name = m_processTable->item(row, 1)->text();

    const QMessageBox::StandardButton answer =
        QMessageBox::question(this, QStringLiteral("确认删除"),
                              QStringLiteral("确定要删除工序「%1」吗？").arg(name),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    ProcessService service;
    const ServiceResult result = service.remove(processId);
    if (!result.ok) {
        showServiceFailure(result.message);
        return;
    }
    refreshProcesses();
    statusBar()->showMessage(QStringLiteral("已删除工序：%1").arg(name), 5000);
}

// ---------------------------------------------------------------- 书包

void MainWindow::refreshBags()
{
    BagService service;
    const QList<BagSummary> bags = service.listAll();

    m_bagTable->setRowCount(0);
    m_bagTable->setRowCount(bags.size());
    for (int row = 0; row < bags.size(); ++row) {
        const BagSummary &s = bags.at(row);

        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(s.id));
        idItem->setData(Qt::UserRole, s.id);
        m_bagTable->setItem(row, 0, idItem);

        m_bagTable->setItem(row, 1, new QTableWidgetItem(s.name));

        QTableWidgetItem *countItem = new QTableWidgetItem(QString::number(s.priceCount));
        countItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_bagTable->setItem(row, 2, countItem);
    }

    if (m_bagTable->rowCount() > 0 && currentBagRow() < 0)
        m_bagTable->selectRow(0);

    const bool hasRows = m_bagTable->rowCount() > 0;
    m_editBagButton->setEnabled(hasRows);
    m_removeBagButton->setEnabled(hasRows);
}

int MainWindow::currentBagRow() const
{
    const QList<QTableWidgetItem *> selected = m_bagTable->selectedItems();
    if (selected.isEmpty())
        return -1;
    return selected.first()->row();
}

int MainWindow::currentBagId() const
{
    const int row = currentBagRow();
    if (row < 0)
        return -1;
    QTableWidgetItem *item = m_bagTable->item(row, 0);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

void MainWindow::addBag()
{
    ProcessService processService;
    const QList<Process> processes = processService.listAll();

    BagDialog dialog(processes, this);
    dialog.setBag(Bag(), QList<BagProcess>());
    if (dialog.exec() != QDialog::Accepted)
        return;

    Bag bag = dialog.bag();
    BagService service;
    const ServiceResult result = service.save(&bag, dialog.prices());
    if (!result.ok) {
        showServiceFailure(result.message);
        return;
    }
    refreshBags();
    statusBar()->showMessage(QStringLiteral("已新增书包：%1").arg(bag.name), 5000);
}

void MainWindow::editBag()
{
    const int bagId = currentBagId();
    if (bagId < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先在表格里选择一款书包"));
        return;
    }

    BagService service;
    Bag bag;
    QList<BagProcess> prices;
    if (!service.load(bagId, &bag, &prices)) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("该书包已不存在，请刷新后重试"));
        refreshBags();
        return;
    }

    ProcessService processService;
    const QList<Process> processes = processService.listAll();

    BagDialog dialog(processes, this);
    dialog.setBag(bag, prices);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Bag edited = dialog.bag();
    const ServiceResult result = service.save(&edited, dialog.prices());
    if (!result.ok) {
        showServiceFailure(result.message);
        return;
    }
    refreshBags();
    statusBar()->showMessage(QStringLiteral("已保存书包：%1").arg(edited.name), 5000);
}

void MainWindow::removeBag()
{
    const int bagId = currentBagId();
    if (bagId < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先在表格里选择一款书包"));
        return;
    }

    const int row = currentBagRow();
    const QString name = m_bagTable->item(row, 1)->text();
    const int priceCount = m_bagTable->item(row, 2)->text().toInt();

    QString question = QStringLiteral("确定要删除书包「%1」吗？").arg(name);
    if (priceCount > 0)
        question += QStringLiteral("\n它的 %1 条单价配置会一起删除。").arg(priceCount);

    const QMessageBox::StandardButton answer =
        QMessageBox::question(this, QStringLiteral("确认删除"), question,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    BagService service;
    const ServiceResult result = service.remove(bagId);
    if (!result.ok) {
        showServiceFailure(result.message);
        return;
    }

    // 单价可能被别处引用（比如订单页正开着），这里统一刷新一下相关页
    refreshBags();
    statusBar()->showMessage(QStringLiteral("已删除书包：%1").arg(name), 5000);
}
