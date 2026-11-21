#include "ui/mainwindow.h"
#include "ui/employeedialog.h"
#include "ui/placeholderpage.h"
#include "db/dbmanager.h"
#include "service/employeeservice.h"

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
    m_tabs->addTab(makePlaceholderPage(
                       QStringLiteral("工序管理"),
                       QStringLiteral("计件项目：每道工序一个通用单价。"
                                      "注意真正算工资时用的是 <b>书包×工序</b> 的单价"
                                      "（bag_process），不是这里的通用价。"),
                       QStringList() << QStringLiteral("列表展示：工序名、单价")
                                     << QStringLiteral("新增/编辑/删除对话框")
                                     << QStringLiteral("删除前校验 bag_process / order_detail 引用"))
                   , QStringLiteral("工序"));
    m_tabs->addTab(makePlaceholderPage(
                       QStringLiteral("书包管理"),
                       QStringLiteral("产品本身只有名称；<b>单价是这个模块的重点</b>："
                                      "同一款书包对每道工序单独定价。"),
                       QStringList() << QStringLiteral("书包名称的增删改查")
                                     << QStringLiteral("编辑书包时批量维护 bag_process 单价表")
                                     << QStringLiteral("保存时用事务：先写 bag，再循环写 bag_process"))
                   , QStringLiteral("书包"));
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

QWidget *MainWindow::makePlaceholderPage(const QString &title,
                                         const QString &summary,
                                         const QStringList &todo)
{
    return new PlaceholderPage(title, summary, todo, this);
}

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
    // 这里直接用列表里的数据，避免 UI 再反向依赖 DAO
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
