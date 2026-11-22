#include "ui/mainwindow.h"
#include "ui/bagdialog.h"
#include "ui/employeedialog.h"
#include "ui/orderdialog.h"
#include "ui/processdialog.h"
#include "ui/placeholderpage.h"
#include "db/dbmanager.h"
#include "service/bagservice.h"
#include "service/employeeservice.h"
#include "service/orderservice.h"
#include "service/processservice.h"
#include "service/wageservice.h"

#include <QApplication>
#include <QComboBox>
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
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
    , m_orderTable(new QTableWidget(this))
    , m_editOrderButton(0)
    , m_removeOrderButton(0)
    , m_orderEmpFilterCombo(0)
    , m_orderPageLabel(0)
    , m_prevOrderPageButton(0)
    , m_nextOrderPageButton(0)
    , m_orderPage(0)
    , m_orderEmpFilter(0)
    , m_orderTotal(0)
    , m_wageMonthCombo(0)
    , m_wageSummaryTable(0)
    , m_wageBreakdownTable(0)
    , m_wageUnpricedTable(0)
    , m_wageUnpricedGroup(0)
    , m_wageStatusLabel(0)
    , m_wageTotalLabel(0)
    , m_wageBreakdownTitle(0)
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
    m_tabs->addTab(createOrderPage(), QStringLiteral("订单"));
    m_tabs->addTab(createWagePage(), QStringLiteral("工资"));
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
    refreshOrders();
    refreshWages();
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

QWidget *MainWindow::createOrderPage()
{
    QWidget *page = new QWidget(this);

    m_orderTable->setColumnCount(7);
    m_orderTable->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("ID") << QStringLiteral("日期")
                      << QStringLiteral("书包") << QStringLiteral("楼层")
                      << QStringLiteral("数量") << QStringLiteral("明细数")
                      << QStringLiteral("合计金额（元）"));
    m_orderTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_orderTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_orderTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_orderTable->verticalHeader()->setVisible(false);
    m_orderTable->horizontalHeader()->setStretchLastSection(false);
    m_orderTable->setColumnWidth(0, 60);
    m_orderTable->setColumnWidth(1, 110);
    m_orderTable->setColumnWidth(2, 220);
    m_orderTable->setColumnWidth(3, 90);
    m_orderTable->setColumnWidth(4, 80);
    m_orderTable->setColumnWidth(5, 80);
    m_orderTable->setColumnWidth(6, 140);

    QPushButton *addButton = new QPushButton(QStringLiteral("新建订单"), page);
    m_editOrderButton = new QPushButton(QStringLiteral("编辑"), page);
    m_removeOrderButton = new QPushButton(QStringLiteral("删除"), page);
    QPushButton *refreshButton = new QPushButton(QStringLiteral("刷新"), page);

    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addWidget(addButton);
    buttons->addWidget(m_editOrderButton);
    buttons->addWidget(m_removeOrderButton);
    buttons->addWidget(refreshButton);
    buttons->addStretch(1);

    QLabel *hint = new QLabel(
        QStringLiteral("订单 = 某个楼层生产某款书包的数量；明细才是「谁做了哪道工序」的计件记录。"
                       "列表按订单分页（每页 10 个订单），删除订单会连带删除它的全部明细。"
                       "合计金额只算已配置单价的明细，缺价的会在「工资」页单独列出。"),
        page);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#777;"));

    // 分页 / 筛选控制条
    m_orderEmpFilterCombo = new QComboBox(page);
    m_orderPageLabel = new QLabel(page);
    m_prevOrderPageButton = new QPushButton(QStringLiteral("上一页"), page);
    m_nextOrderPageButton = new QPushButton(QStringLiteral("下一页"), page);

    QHBoxLayout *pager = new QHBoxLayout;
    pager->addWidget(new QLabel(QStringLiteral("员工筛选："), page));
    pager->addWidget(m_orderEmpFilterCombo);
    pager->addSpacing(16);
    pager->addWidget(m_prevOrderPageButton);
    pager->addWidget(m_orderPageLabel);
    pager->addWidget(m_nextOrderPageButton);
    pager->addSpacing(8);
    pager->addWidget(new QLabel(QStringLiteral("每页 10 个订单"), page));
    pager->addStretch(1);

    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->addLayout(buttons);
    layout->addWidget(hint);
    layout->addLayout(pager);
    layout->addWidget(m_orderTable, 1);

    connect(addButton, SIGNAL(clicked()), this, SLOT(addOrder()));
    connect(m_editOrderButton, SIGNAL(clicked()), this, SLOT(editOrder()));
    connect(m_removeOrderButton, SIGNAL(clicked()), this, SLOT(removeOrder()));
    connect(refreshButton, SIGNAL(clicked()), this, SLOT(refreshOrders()));
    connect(m_prevOrderPageButton, SIGNAL(clicked()), this, SLOT(goPrevOrderPage()));
    connect(m_nextOrderPageButton, SIGNAL(clicked()), this, SLOT(goNextOrderPage()));
    connect(m_orderEmpFilterCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onOrderFilterChanged()));

    populateOrderEmployeeFilter();

    return page;
}

QWidget *MainWindow::createWagePage()
{
    QWidget *page = new QWidget(this);

    m_wageMonthCombo = new QComboBox(page);
    m_wageMonthCombo->setObjectName(QStringLiteral("wageMonthCombo"));
    QPushButton *refreshButton = new QPushButton(QStringLiteral("刷新"), page);
    m_wageTotalLabel = new QLabel(page);
    m_wageTotalLabel->setObjectName(QStringLiteral("wageTotalLabel"));
    m_wageTotalLabel->setStyleSheet(QStringLiteral("font-weight:bold;"));
    m_wageStatusLabel = new QLabel(page);
    m_wageStatusLabel->setObjectName(QStringLiteral("wageStatusLabel"));
    m_wageStatusLabel->setStyleSheet(QStringLiteral("color:#b45309;"));

    QHBoxLayout *top = new QHBoxLayout;
    top->addWidget(new QLabel(QStringLiteral("月份："), page));
    top->addWidget(m_wageMonthCombo);
    top->addWidget(refreshButton);
    top->addSpacing(24);
    top->addWidget(m_wageTotalLabel);
    top->addSpacing(16);
    top->addWidget(m_wageStatusLabel);
    top->addStretch(1);

    // 员工汇总
    m_wageSummaryTable = new QTableWidget(page);
    m_wageSummaryTable->setColumnCount(4);
    m_wageSummaryTable->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("员工") << QStringLiteral("明细条数")
                      << QStringLiteral("件数") << QStringLiteral("工资（元）"));
    m_wageSummaryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_wageSummaryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_wageSummaryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_wageSummaryTable->verticalHeader()->setVisible(false);
    m_wageSummaryTable->horizontalHeader()->setStretchLastSection(false);
    m_wageSummaryTable->setColumnWidth(0, 110);
    m_wageSummaryTable->setColumnWidth(1, 90);
    m_wageSummaryTable->setColumnWidth(2, 80);
    m_wageSummaryTable->setColumnWidth(3, 140);

    QGroupBox *summaryGroup = new QGroupBox(QStringLiteral("员工月度计件工资"), page);
    summaryGroup->setFixedWidth(460);
    QVBoxLayout *summaryLayout = new QVBoxLayout(summaryGroup);
    summaryLayout->addWidget(m_wageSummaryTable);

    // 某个员工的构成
    m_wageBreakdownTitle = new QLabel(QStringLiteral("（选择左侧员工查看构成）"), page);
    m_wageBreakdownTitle->setStyleSheet(QStringLiteral("color:#555;"));

    m_wageBreakdownTable = new QTableWidget(page);
    m_wageBreakdownTable->setColumnCount(4);
    m_wageBreakdownTable->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("工序") << QStringLiteral("数量")
                      << QStringLiteral("单价（元）") << QStringLiteral("小计（元）"));
    m_wageBreakdownTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_wageBreakdownTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_wageBreakdownTable->verticalHeader()->setVisible(false);
    m_wageBreakdownTable->horizontalHeader()->setStretchLastSection(false);
    m_wageBreakdownTable->setColumnWidth(0, 130);
    m_wageBreakdownTable->setColumnWidth(1, 90);
    m_wageBreakdownTable->setColumnWidth(2, 110);
    m_wageBreakdownTable->setColumnWidth(3, 130);

    QGroupBox *breakdownGroup = new QGroupBox(QStringLiteral("工资构成"), page);
    QVBoxLayout *breakdownLayout = new QVBoxLayout(breakdownGroup);
    breakdownLayout->addWidget(m_wageBreakdownTitle);
    breakdownLayout->addWidget(m_wageBreakdownTable);

    QHBoxLayout *middle = new QHBoxLayout;
    middle->addWidget(summaryGroup);
    middle->addWidget(breakdownGroup, 1);
    // 未定价明细（默认隐藏）
    m_wageUnpricedTable = new QTableWidget(page);
    m_wageUnpricedTable->setColumnCount(5);
    m_wageUnpricedTable->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("员工") << QStringLiteral("书包")
                      << QStringLiteral("工序") << QStringLiteral("数量")
                      << QStringLiteral("日期"));
    m_wageUnpricedTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_wageUnpricedTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_wageUnpricedTable->verticalHeader()->setVisible(false);
    m_wageUnpricedTable->horizontalHeader()->setStretchLastSection(true);
    m_wageUnpricedTable->setColumnWidth(0, 120);
    m_wageUnpricedTable->setColumnWidth(1, 200);
    m_wageUnpricedTable->setColumnWidth(2, 120);
    m_wageUnpricedTable->setColumnWidth(3, 80);

    m_wageUnpricedGroup = new QGroupBox(QStringLiteral("未配置单价的明细（不计入总额）"), page);
    QVBoxLayout *unpricedLayout = new QVBoxLayout(m_wageUnpricedGroup);
    QLabel *unpricedHint = new QLabel(
        QStringLiteral("这些明细所在的「书包 × 工序」没有单价，所以无法计算工资。"
                       "请到「书包」页补齐单价后刷新。"), m_wageUnpricedGroup);
    unpricedHint->setWordWrap(true);
    unpricedHint->setStyleSheet(QStringLiteral("color:#b45309;"));
    unpricedLayout->addWidget(unpricedHint);
    unpricedLayout->addWidget(m_wageUnpricedTable);
    m_wageUnpricedGroup->setVisible(false);

    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->addLayout(top);
    layout->addLayout(middle, 1);
    layout->addWidget(m_wageUnpricedGroup);

    connect(refreshButton, SIGNAL(clicked()), this, SLOT(refreshWages()));
    connect(m_wageMonthCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onWageMonthChanged()));
    connect(m_wageSummaryTable, SIGNAL(itemSelectionChanged()),
            this, SLOT(onWageSummarySelectionChanged()));

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
    const bool hasRows = m_employeeTable->rowCount() > 0;
    m_editButton->setEnabled(hasRows);
    m_removeButton->setEnabled(hasRows);

    // 员工变了，订单页的筛选下拉也要跟着更新（保留当前选中项）
    populateOrderEmployeeFilter();
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

// ---------------------------------------------------------------- 订单

bool MainWindow::loadBagPriceMap(QList<Bag> *bags, QHash<int, QList<BagProcess> > *pricesByBag)
{
    BagService service;
    const QList<BagSummary> summaries = service.listAll();

    bags->clear();
    pricesByBag->clear();
    for (int i = 0; i < summaries.size(); ++i) {
        Bag bag;
        QList<BagProcess> prices;
        if (!service.load(summaries.at(i).id, &bag, &prices))
            continue;
        bags->append(bag);
        pricesByBag->insert(bag.id, prices);
    }
    return !bags->isEmpty();
}

void MainWindow::refreshOrders()
{
    OrderService service;
    int total = 0;
    const QList<OrderSummary> orders =
        service.page(m_orderPage, kOrderPageSize, m_orderEmpFilter, &total);

    // 删除或筛选后当前页可能越界：自动回到最后一页
    if (orders.isEmpty() && m_orderPage > 0 && total > 0) {
        --m_orderPage;
        refreshOrders();
        return;
    }
    if (orders.isEmpty() && m_orderPage > 0) {
        m_orderPage = 0;
        refreshOrders();
        return;
    }

    m_orderTotal = total;

    m_orderTable->setRowCount(0);
    m_orderTable->setRowCount(orders.size());
    for (int row = 0; row < orders.size(); ++row) {
        const OrderSummary &o = orders.at(row);

        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(o.id));
        idItem->setData(Qt::UserRole, o.id);
        m_orderTable->setItem(row, 0, idItem);

        m_orderTable->setItem(row, 1, new QTableWidgetItem(o.orderDate));
        m_orderTable->setItem(row, 2, new QTableWidgetItem(o.bagName));

        QTableWidgetItem *floorItem = new QTableWidgetItem(o.floor);
        floorItem->setTextAlignment(Qt::AlignCenter);
        m_orderTable->setItem(row, 3, floorItem);

        QTableWidgetItem *qtyItem = new QTableWidgetItem(QString::number(o.qty));
        qtyItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_orderTable->setItem(row, 4, qtyItem);

        QTableWidgetItem *countItem = new QTableWidgetItem(QString::number(o.detailCount));
        countItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_orderTable->setItem(row, 5, countItem);

        QTableWidgetItem *amountItem =
            new QTableWidgetItem(QString::number(o.totalAmount, 'f', 2));
        amountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_orderTable->setItem(row, 6, amountItem);
    }

    if (m_orderTable->rowCount() > 0 && currentOrderRow() < 0)
        m_orderTable->selectRow(0);

    const bool hasRows = m_orderTable->rowCount() > 0;
    m_editOrderButton->setEnabled(hasRows);
    m_removeOrderButton->setEnabled(hasRows);

    updateOrderPageControls();
}

void MainWindow::populateOrderEmployeeFilter()
{
    if (!m_orderEmpFilterCombo)
        return;

    const int keep = m_orderEmpFilter;
    QSignalBlocker blocker(m_orderEmpFilterCombo);

    m_orderEmpFilterCombo->clear();
    m_orderEmpFilterCombo->addItem(QStringLiteral("全部员工"), 0);

    EmployeeService service;
    const QList<Employee> employees = service.listAll();
    for (int i = 0; i < employees.size(); ++i)
        m_orderEmpFilterCombo->addItem(employees.at(i).name, employees.at(i).id);

    const int index = m_orderEmpFilterCombo->findData(keep);
    m_orderEmpFilterCombo->setCurrentIndex(index >= 0 ? index : 0);
    m_orderEmpFilter = m_orderEmpFilterCombo->currentData().toInt();
}

void MainWindow::updateOrderPageControls()
{
    const int pages = m_orderTotal > 0
        ? (m_orderTotal + kOrderPageSize - 1) / kOrderPageSize
        : 0;

    if (pages == 0) {
        m_orderPageLabel->setText(QStringLiteral("暂无订单"));
    } else {
        m_orderPageLabel->setText(QStringLiteral("第 %1/%2 页（共 %3 个订单）")
                                      .arg(m_orderPage + 1)
                                      .arg(pages)
                                      .arg(m_orderTotal));
    }

    m_prevOrderPageButton->setEnabled(m_orderPage > 0);
    m_nextOrderPageButton->setEnabled(pages > 0 && m_orderPage + 1 < pages);
}

void MainWindow::onOrderFilterChanged()
{
    if (!m_orderEmpFilterCombo)
        return;
    m_orderEmpFilter = m_orderEmpFilterCombo->currentData().toInt();
    m_orderPage = 0;
    refreshOrders();
}

void MainWindow::goPrevOrderPage()
{
    if (m_orderPage <= 0)
        return;
    --m_orderPage;
    refreshOrders();
}

void MainWindow::goNextOrderPage()
{
    const int pages = m_orderTotal > 0
        ? (m_orderTotal + kOrderPageSize - 1) / kOrderPageSize
        : 0;
    if (m_orderPage + 1 >= pages)
        return;
    ++m_orderPage;
    refreshOrders();
}

int MainWindow::currentOrderRow() const
{
    const QList<QTableWidgetItem *> selected = m_orderTable->selectedItems();
    if (selected.isEmpty())
        return -1;
    return selected.first()->row();
}

int MainWindow::currentOrderId() const
{
    const int row = currentOrderRow();
    if (row < 0)
        return -1;
    QTableWidgetItem *item = m_orderTable->item(row, 0);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

void MainWindow::addOrder()
{
    QList<Bag> bags;
    QHash<int, QList<BagProcess> > pricesByBag;
    if (!loadBagPriceMap(&bags, &pricesByBag)) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("还没有书包，请先到「书包」页添加。"));
        return;
    }

    EmployeeService employeeService;
    const QList<Employee> employees = employeeService.listAll();
    if (employees.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("还没有员工，请先到「员工」页添加。"));
        return;
    }

    ProcessService processService;
    const QList<Process> processes = processService.listAll();
    if (processes.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("还没有工序，请先到「工序」页添加。"));
        return;
    }

    OrderDialog dialog(bags, employees, processes, pricesByBag, this);
    dialog.setOrder(Order(), QList<OrderDetail>());
    if (dialog.exec() != QDialog::Accepted)
        return;

    Order order = dialog.order();
    OrderService service;
    const ServiceResult result = service.save(&order, dialog.details());
    if (!result.ok) {
        showServiceFailure(result.message);
        return;
    }

    // 新订单是最新的，跳到第 1 页让用户直接看到
    m_orderPage = 0;
    refreshOrders();
    statusBar()->showMessage(QStringLiteral("已保存订单 #%1（%2 层）").arg(order.id).arg(order.floor),
                             5000);
}

void MainWindow::editOrder()
{
    const int orderId = currentOrderId();
    if (orderId < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先在表格里选择一个订单"));
        return;
    }

    OrderService service;
    Order order;
    QList<OrderDetail> details;
    if (!service.load(orderId, &order, &details)) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("该订单已不存在，请刷新后重试"));
        refreshOrders();
        return;
    }

    QList<Bag> bags;
    QHash<int, QList<BagProcess> > pricesByBag;
    if (!loadBagPriceMap(&bags, &pricesByBag)) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("书包数据缺失，无法编辑订单"));
        return;
    }

    EmployeeService employeeService;
    const QList<Employee> employees = employeeService.listAll();
    ProcessService processService;
    const QList<Process> processes = processService.listAll();

    OrderDialog dialog(bags, employees, processes, pricesByBag, this);
    dialog.setOrder(order, details);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Order edited = dialog.order();
    const ServiceResult result = service.save(&edited, dialog.details());
    if (!result.ok) {
        showServiceFailure(result.message);
        return;
    }

    refreshOrders();
    statusBar()->showMessage(QStringLiteral("已保存订单 #%1").arg(edited.id), 5000);
}

void MainWindow::removeOrder()
{
    const int orderId = currentOrderId();
    if (orderId < 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先在表格里选择一个订单"));
        return;
    }

    const int row = currentOrderRow();
    const int detailCount = m_orderTable->item(row, 5)->text().toInt();

    QString question = QStringLiteral("确定要删除订单 #%1 吗？").arg(orderId);
    if (detailCount > 0)
        question += QStringLiteral("\n它的 %1 条明细会一起删除，且无法恢复。").arg(detailCount);

    const QMessageBox::StandardButton answer =
        QMessageBox::question(this, QStringLiteral("确认删除"), question,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    OrderService service;
    const ServiceResult result = service.remove(orderId);
    if (!result.ok) {
        showServiceFailure(result.message);
        return;
    }

    refreshOrders();
    // 订单删掉后，相关书包才可能重新可删
    refreshBags();
    statusBar()->showMessage(QStringLiteral("已删除订单 #%1").arg(orderId), 5000);
}

// ---------------------------------------------------------------- 工资

void MainWindow::refreshWages()
{
    if (!m_wageMonthCombo)
        return;

    WageService service;
    const QStringList withData = service.months();
    // 下拉 = 有记录的月份 ∪ 最近 12 个月，否则没数据的月份永远选不到
    const QStringList options = service.selectableMonths(12);
    const QString keep = m_wageMonthCombo->currentData().toString();

    QString target = keep;
    if (target.isEmpty() || !options.contains(target)) {
        // 首次打开优先落在「最近一个真的有数据的月份」，没有数据才用本月
        target = withData.isEmpty()
            ? QDate::currentDate().toString(QStringLiteral("yyyy-MM"))
            : withData.first();
    }

    {
        QSignalBlocker blocker(m_wageMonthCombo);
        m_wageMonthCombo->clear();
        for (int i = 0; i < options.size(); ++i)
            m_wageMonthCombo->addItem(options.at(i), options.at(i));

        const int index = m_wageMonthCombo->findData(target);
        m_wageMonthCombo->setCurrentIndex(index >= 0 ? index : 0);
    }

    loadWageMonth(m_wageMonthCombo->currentData().toString());
}

void MainWindow::onWageMonthChanged()
{
    loadWageMonth(m_wageMonthCombo->currentData().toString());
}

void MainWindow::onWageSummarySelectionChanged()
{
    const int row = m_wageSummaryTable->currentRow();
    if (row < 0)
        return;

    QTableWidgetItem *item = m_wageSummaryTable->item(row, 0);
    if (item)
        loadWageBreakdown(item->data(Qt::UserRole).toInt());
}

void MainWindow::loadWageMonth(const QString &month)
{
    m_wageSummaryTable->setRowCount(0);
    m_wageBreakdownTable->setRowCount(0);
    m_wageBreakdownTitle->setText(QStringLiteral("（选择左侧员工查看构成）"));
    m_wageUnpricedTable->setRowCount(0);
    m_wageUnpricedGroup->setVisible(false);
    m_wageTotalLabel->clear();
    m_wageStatusLabel->clear();

    if (month.isEmpty()) {
        m_wageStatusLabel->setText(QStringLiteral("还没有任何计件记录"));
        return;
    }

    WageService service;
    QList<WageDetailLine> unpriced;
    const QList<WageSummary> summaries = service.monthlySummary(month, &unpriced);

    qint64 totalCents = 0;
    m_wageSummaryTable->setRowCount(summaries.size());
    for (int row = 0; row < summaries.size(); ++row) {
        const WageSummary &s = summaries.at(row);
        totalCents += s.totalCents;

        QTableWidgetItem *nameItem = new QTableWidgetItem(s.empName);
        nameItem->setData(Qt::UserRole, s.empId);
        m_wageSummaryTable->setItem(row, 0, nameItem);

        QTableWidgetItem *linesItem = new QTableWidgetItem(QString::number(s.lineCount));
        linesItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_wageSummaryTable->setItem(row, 1, linesItem);

        QTableWidgetItem *qtyItem = new QTableWidgetItem(QString::number(s.totalQty));
        qtyItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_wageSummaryTable->setItem(row, 2, qtyItem);

        QTableWidgetItem *moneyItem =
            new QTableWidgetItem(WageService::formatCents(s.totalCents));
        moneyItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_wageSummaryTable->setItem(row, 3, moneyItem);
    }

    if (summaries.isEmpty()) {
        // 没有任何可计入的工资时不能显示「合计 0.00 元 / 0 人」，
        // 那会被读成「这个月工资是 0」，和「没有记录」是两件事
        m_wageTotalLabel->clear();
        m_wageStatusLabel->setText(unpriced.isEmpty()
                                       ? QStringLiteral("本月无计件记录")
                                       : QStringLiteral("本月没有已定价的计件记录"));
    } else {
        m_wageTotalLabel->setText(QStringLiteral("合计 %1 元 / %2 人")
                                      .arg(WageService::formatCents(totalCents))
                                      .arg(summaries.size()));
    }

    if (!unpriced.isEmpty()) {
        m_wageUnpricedTable->setRowCount(unpriced.size());
        for (int row = 0; row < unpriced.size(); ++row) {
            const WageDetailLine &line = unpriced.at(row);
            m_wageUnpricedTable->setItem(row, 0, new QTableWidgetItem(line.empName));
            m_wageUnpricedTable->setItem(row, 1, new QTableWidgetItem(line.bagName));
            m_wageUnpricedTable->setItem(row, 2, new QTableWidgetItem(line.processName));

            QTableWidgetItem *qtyItem = new QTableWidgetItem(QString::number(line.qty));
            qtyItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_wageUnpricedTable->setItem(row, 3, qtyItem);

            m_wageUnpricedTable->setItem(row, 4, new QTableWidgetItem(line.workDate));
        }
        m_wageUnpricedGroup->setTitle(
            QStringLiteral("未配置单价的明细（%1 条，不计入总额）").arg(unpriced.size()));
        m_wageUnpricedGroup->setVisible(true);
    }

    if (!summaries.isEmpty()) {
        m_wageSummaryTable->selectRow(0);
        loadWageBreakdown(summaries.first().empId);
    }
}

void MainWindow::loadWageBreakdown(int empId)
{
    m_wageBreakdownTable->setRowCount(0);
    if (empId <= 0)
        return;

    const QString month = m_wageMonthCombo->currentData().toString();
    if (month.isEmpty())
        return;

    // 从汇总表里取员工名，用于标题
    QString empName;
    for (int row = 0; row < m_wageSummaryTable->rowCount(); ++row) {
        QTableWidgetItem *item = m_wageSummaryTable->item(row, 0);
        if (item && item->data(Qt::UserRole).toInt() == empId) {
            empName = item->text();
            break;
        }
    }

    m_wageBreakdownTitle->setText(
        QStringLiteral("构成：%1（%2）").arg(empName, month));

    WageService service;
    const QList<WageBreakdown> rows = service.breakdown(empId, month);

    m_wageBreakdownTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const WageBreakdown &b = rows.at(i);

        m_wageBreakdownTable->setItem(i, 0, new QTableWidgetItem(b.processName));

        QTableWidgetItem *qtyItem = new QTableWidgetItem(QString::number(b.totalQty));
        qtyItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_wageBreakdownTable->setItem(i, 1, qtyItem);

        QTableWidgetItem *priceItem =
            new QTableWidgetItem(QString::number(b.unitPrice, 'f', 2));
        priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_wageBreakdownTable->setItem(i, 2, priceItem);

        QTableWidgetItem *amountItem =
            new QTableWidgetItem(WageService::formatCents(b.amountCents));
        amountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_wageBreakdownTable->setItem(i, 3, amountItem);
    }
}
