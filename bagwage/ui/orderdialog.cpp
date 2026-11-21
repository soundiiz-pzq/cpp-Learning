#include "ui/orderdialog.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QString formatDate(const QDate &date)
{
    return date.toString(QStringLiteral("yyyy-MM-dd"));
}

QDate parseDate(const QString &text)
{
    const QDate date = QDate::fromString(text, QStringLiteral("yyyy-MM-dd"));
    return date.isValid() ? date : QDate::currentDate();
}

} // namespace

OrderDialog::OrderDialog(const QList<Bag> &bags,
                         const QList<Employee> &employees,
                         const QList<Process> &processes,
                         const QHash<int, QList<BagProcess> > &pricesByBag,
                         QWidget *parent)
    : QDialog(parent)
    , m_bagCombo(new QComboBox(this))
    , m_floorCombo(new QComboBox(this))
    , m_qtySpin(new QSpinBox(this))
    , m_dateEdit(new QDateEdit(this))
    , m_priceHint(new QLabel(this))
    , m_detailTable(new QTableWidget(this))
    , m_addRowButton(new QPushButton(QStringLiteral("添加一行"), this))
    , m_removeRowButton(new QPushButton(QStringLiteral("删除选中行"), this))
    , m_bags(bags)
    , m_employees(employees)
    , m_processes(processes)
    , m_pricesByBag(pricesByBag)
{
    setWindowTitle(QStringLiteral("订单信息"));
    resize(900, 600);

    for (int i = 0; i < m_bags.size(); ++i)
        m_bagCombo->addItem(m_bags.at(i).name, m_bags.at(i).id);

    m_floorCombo->setEditable(true);
    QStringList floors;
    for (int i = 0; i < m_employees.size(); ++i) {
        const QString floor = m_employees.at(i).floor;
        if (!floor.isEmpty() && !floors.contains(floor))
            floors << floor;
    }
    m_floorCombo->addItems(floors);
    m_floorCombo->setCurrentText(floors.isEmpty() ? QString() : floors.first());

    m_qtySpin->setRange(1, 1000000);
    m_qtySpin->setValue(10);
    m_qtySpin->setSuffix(QStringLiteral(" 件"));

    m_dateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDate(QDate::currentDate());

    m_priceHint->setWordWrap(true);
    m_priceHint->setStyleSheet(QStringLiteral("color:#777;"));

    m_detailTable->setColumnCount(4);
    m_detailTable->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("员工") << QStringLiteral("工序")
                      << QStringLiteral("数量") << QStringLiteral("日期"));
    m_detailTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_detailTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_detailTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_detailTable->verticalHeader()->setVisible(false);
    m_detailTable->horizontalHeader()->setStretchLastSection(true);
    m_detailTable->setColumnWidth(0, 200);
    m_detailTable->setColumnWidth(1, 300);
    m_detailTable->setColumnWidth(2, 110);

    QFormLayout *headerForm = new QFormLayout;
    headerForm->addRow(QStringLiteral("书包："), m_bagCombo);
    headerForm->addRow(QStringLiteral("楼层："), m_floorCombo);
    headerForm->addRow(QStringLiteral("订单数量："), m_qtySpin);
    headerForm->addRow(QStringLiteral("订单日期："), m_dateEdit);

    QHBoxLayout *tableButtons = new QHBoxLayout;
    tableButtons->addWidget(m_addRowButton);
    tableButtons->addWidget(m_removeRowButton);
    tableButtons->addStretch(1);

    QLabel *tip = new QLabel(
        QStringLiteral("明细 = 哪个员工做了哪道工序、做了多少件。一次可以录多行，保存时一次性写入。"),
        this);
    tip->setWordWrap(true);
    tip->setStyleSheet(QStringLiteral("color:#777;"));

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                                 this);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("保存"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(headerForm);
    layout->addWidget(tip);
    layout->addWidget(m_priceHint);
    layout->addLayout(tableButtons);
    layout->addWidget(m_detailTable, 1);
    layout->addWidget(box);

    connect(m_addRowButton, SIGNAL(clicked()), this, SLOT(addDetailRow()));
    connect(m_removeRowButton, SIGNAL(clicked()), this, SLOT(removeDetailRow()));
    connect(m_bagCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onBagChanged()));
    connect(box, SIGNAL(accepted()), this, SLOT(onAccept()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));

    refreshProcessLabels();
}

double OrderDialog::priceFor(int processId) const
{
    const int bagId = m_bagCombo->currentData().toInt();
    const QList<BagProcess> prices = m_pricesByBag.value(bagId);
    for (int i = 0; i < prices.size(); ++i) {
        if (prices.at(i).processId == processId)
            return prices.at(i).price;
    }
    return -1.0;
}

void OrderDialog::updatePriceHint()
{
    const int bagId = m_bagCombo->currentData().toInt();
    const int count = m_pricesByBag.value(bagId).size();
    if (count == 0) {
        m_priceHint->setText(QStringLiteral(
            "这款书包还没有配置任何工序单价，明细无法保存。请先到「书包」页补齐单价。"));
        m_priceHint->setStyleSheet(QStringLiteral("color:#b45309;"));
    } else {
        m_priceHint->setText(QStringLiteral(
            "这款书包已配置 %1 道工序的单价，下拉里括号内就是该书包的单价。").arg(count));
        m_priceHint->setStyleSheet(QStringLiteral("color:#777;"));
    }
}

void OrderDialog::refreshProcessLabels()
{
    QHash<int, double> priceMap;
    const int bagId = m_bagCombo->currentData().toInt();
    const QList<BagProcess> prices = m_pricesByBag.value(bagId);
    for (int i = 0; i < prices.size(); ++i)
        priceMap.insert(prices.at(i).processId, prices.at(i).price);

    for (int row = 0; row < m_detailTable->rowCount(); ++row) {
        QComboBox *combo = qobject_cast<QComboBox *>(m_detailTable->cellWidget(row, 1));
        if (!combo)
            continue;

        const int keepId = combo->currentData().toInt();
        combo->blockSignals(true);
        combo->clear();
        for (int i = 0; i < m_processes.size(); ++i) {
            const Process &p = m_processes.at(i);
            const QString text = priceMap.contains(p.id)
                ? QStringLiteral("%1（%2）").arg(p.name, QString::number(priceMap.value(p.id), 'f', 2))
                : QStringLiteral("%1（未定价）").arg(p.name);
            combo->addItem(text, p.id);
        }
        const int index = combo->findData(keepId);
        combo->setCurrentIndex(index >= 0 ? index : 0);
        combo->blockSignals(false);
    }
    updatePriceHint();
}

void OrderDialog::onBagChanged()
{
    refreshProcessLabels();
}

void OrderDialog::addRowWithValues(int empId, int processId, int qty, const QString &date)
{
    const int row = m_detailTable->rowCount();
    m_detailTable->insertRow(row);

    QComboBox *empCombo = new QComboBox(m_detailTable);
    for (int i = 0; i < m_employees.size(); ++i)
        empCombo->addItem(m_employees.at(i).name, m_employees.at(i).id);
    empCombo->installEventFilter(this);
    m_detailTable->setCellWidget(row, 0, empCombo);

    QComboBox *processCombo = new QComboBox(m_detailTable);
    processCombo->installEventFilter(this);
    m_detailTable->setCellWidget(row, 1, processCombo);

    QSpinBox *qtySpin = new QSpinBox(m_detailTable);
    qtySpin->setRange(1, 1000000);
    qtySpin->installEventFilter(this);
    m_detailTable->setCellWidget(row, 2, qtySpin);

    QDateEdit *dateEdit = new QDateEdit(m_detailTable);
    dateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    dateEdit->setCalendarPopup(true);
    dateEdit->installEventFilter(this);
    m_detailTable->setCellWidget(row, 3, dateEdit);

    // 先填工序下拉（依赖当前书包），再定位到指定员工/工序
    refreshProcessLabels();

    const int empIndex = empCombo->findData(empId);
    if (empIndex >= 0)
        empCombo->setCurrentIndex(empIndex);

    const int processIndex = processCombo->findData(processId);
    if (processIndex >= 0)
        processCombo->setCurrentIndex(processIndex);

    qtySpin->setValue(qty > 0 ? qty : 1);
    dateEdit->setDate(parseDate(date.isEmpty() ? formatDate(m_dateEdit->date()) : date));

    highlightRow(row);
}

void OrderDialog::addDetailRow()
{
    if (m_employees.isEmpty() || m_processes.isEmpty()) {
        QMessageBox::information(
            this, QStringLiteral("提示"),
            QStringLiteral("还没有员工或工序，请先在「员工」「工序」页添加。"));
        return;
    }

    // 默认选一个员工还没用过的员工；工序留给 refreshProcessLabels 选第一个
    QSet<int> usedEmployees;
    for (int row = 0; row < m_detailTable->rowCount(); ++row) {
        QComboBox *combo = qobject_cast<QComboBox *>(m_detailTable->cellWidget(row, 0));
        if (combo)
            usedEmployees.insert(combo->currentData().toInt());
    }

    int preferredEmp = m_employees.first().id;
    for (int i = 0; i < m_employees.size(); ++i) {
        if (!usedEmployees.contains(m_employees.at(i).id)) {
            preferredEmp = m_employees.at(i).id;
            break;
        }
    }
    addRowWithValues(preferredEmp, 0, 1, QString());
}

void OrderDialog::removeDetailRow()
{
    int row = m_detailTable->currentRow();
    if (row < 0)
        row = m_detailTable->rowCount() - 1;
    if (row < 0)
        return;

    m_detailTable->removeRow(row);
    if (m_detailTable->rowCount() > 0)
        highlightRow(qMin(row, m_detailTable->rowCount() - 1));
}

int OrderDialog::rowOfWidget(const QObject *widget) const
{
    for (int row = 0; row < m_detailTable->rowCount(); ++row) {
        for (int col = 0; col < m_detailTable->columnCount(); ++col) {
            if (m_detailTable->cellWidget(row, col) == widget)
                return row;
        }
    }
    return -1;
}

void OrderDialog::highlightRow(int row)
{
    if (row < 0 || row >= m_detailTable->rowCount())
        return;
    m_detailTable->setCurrentCell(row, 0);
    m_detailTable->selectRow(row);
}

bool OrderDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusIn) {
        const int row = rowOfWidget(watched);
        if (row >= 0)
            highlightRow(row);
    }
    return QDialog::eventFilter(watched, event);
}

void OrderDialog::setOrder(const Order &order, const QList<OrderDetail> &details)
{
    m_order = order;

    const int bagIndex = m_bagCombo->findData(order.bagId);
    if (bagIndex >= 0)
        m_bagCombo->setCurrentIndex(bagIndex);

    if (!order.floor.isEmpty())
        m_floorCombo->setCurrentText(order.floor);
    if (order.qty > 0)
        m_qtySpin->setValue(order.qty);
    if (!order.orderDate.isEmpty())
        m_dateEdit->setDate(parseDate(order.orderDate));

    m_detailTable->setRowCount(0);
    for (int i = 0; i < details.size(); ++i) {
        addRowWithValues(details.at(i).empId,
                         details.at(i).processId,
                         details.at(i).qty,
                         details.at(i).workDate);
    }

    setWindowTitle(order.id > 0 ? QStringLiteral("编辑订单 #%1").arg(order.id)
                                : QStringLiteral("新建订单"));
}

Order OrderDialog::order() const
{
    Order o = m_order;
    o.bagId = m_bagCombo->currentData().toInt();
    o.floor = m_floorCombo->currentText();
    o.qty = m_qtySpin->value();
    o.orderDate = formatDate(m_dateEdit->date());
    return o;
}

QList<OrderDetail> OrderDialog::details() const
{
    QList<OrderDetail> result;
    for (int row = 0; row < m_detailTable->rowCount(); ++row) {
        QComboBox *empCombo = qobject_cast<QComboBox *>(m_detailTable->cellWidget(row, 0));
        QComboBox *processCombo = qobject_cast<QComboBox *>(m_detailTable->cellWidget(row, 1));
        QSpinBox *qtySpin = qobject_cast<QSpinBox *>(m_detailTable->cellWidget(row, 2));
        QDateEdit *dateEdit = qobject_cast<QDateEdit *>(m_detailTable->cellWidget(row, 3));
        if (!empCombo || !processCombo || !qtySpin || !dateEdit)
            continue;

        OrderDetail d;
        d.orderId = m_order.id;
        d.empId = empCombo->currentData().toInt();
        d.processId = processCombo->currentData().toInt();
        d.qty = qtySpin->value();
        d.workDate = formatDate(dateEdit->date());
        result << d;
    }
    return result;
}

void OrderDialog::onAccept()
{
    if (m_bagCombo->currentData().toInt() <= 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请选择书包"));
        return;
    }

    if (m_floorCombo->currentText().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请填写楼层"));
        m_floorCombo->setFocus();
        return;
    }

    if (m_detailTable->rowCount() == 0) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("至少添加一条明细"));
        return;
    }

    // 未定价的工序先在这里提示一次，避免点保存后再看到服务层报错
    const int bagId = m_bagCombo->currentData().toInt();
    QStringList unpriced;
    QList<BagProcess> prices = m_pricesByBag.value(bagId);
    for (int row = 0; row < m_detailTable->rowCount(); ++row) {
        QComboBox *combo = qobject_cast<QComboBox *>(m_detailTable->cellWidget(row, 1));
        if (!combo)
            continue;
        bool priced = false;
        for (int i = 0; i < prices.size(); ++i) {
            if (prices.at(i).processId == combo->currentData().toInt())
                priced = true;
        }
        if (!priced)
            unpriced << QString::number(row + 1);
    }
    if (!unpriced.isEmpty()) {
        QMessageBox::information(
            this, QStringLiteral("提示"),
            QStringLiteral("第 %1 行选的工序，这款书包没有配单价。\n"
                           "请先到「书包」页补齐单价，或换一道工序。")
                .arg(unpriced.join(QStringLiteral("、"))));
        return;
    }

    accept();
}
