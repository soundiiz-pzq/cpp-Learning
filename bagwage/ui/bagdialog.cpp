#include "ui/bagdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

BagDialog::BagDialog(const QList<Process> &processes, QWidget *parent)
    : QDialog(parent)
    , m_nameEdit(new QLineEdit(this))
    , m_priceTable(new QTableWidget(this))
    , m_addRowButton(new QPushButton(QStringLiteral("添加一行"), this))
    , m_removeRowButton(new QPushButton(QStringLiteral("删除选中行"), this))
    , m_processes(processes)
{
    setWindowTitle(QStringLiteral("书包信息"));
    resize(520, 430);

    m_nameEdit->setPlaceholderText(QStringLiteral("例如：双肩包 A 款"));

    m_priceTable->setColumnCount(2);
    m_priceTable->setHorizontalHeaderLabels(
        QStringList() << QStringLiteral("工序") << QStringLiteral("单价（元/件）"));
    m_priceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_priceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_priceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_priceTable->verticalHeader()->setVisible(false);
    m_priceTable->horizontalHeader()->setStretchLastSection(true);
    m_priceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_priceTable->setColumnWidth(1, 140);

    QLabel *hint = new QLabel(
        QStringLiteral("这款书包做每道工序的计件单价。留空表示还没定价。"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#777;"));

    QLabel *empty = 0;
    if (m_processes.isEmpty()) {
        empty = new QLabel(
            QStringLiteral("还没有工序，请先到「工序」页添加，再回来配置单价。"), this);
        empty->setWordWrap(true);
        empty->setStyleSheet(QStringLiteral("color:#b45309;"));
        m_addRowButton->setEnabled(false);
    }

    QHBoxLayout *tableButtons = new QHBoxLayout;
    tableButtons->addWidget(m_addRowButton);
    tableButtons->addWidget(m_removeRowButton);
    tableButtons->addStretch(1);

    QFormLayout *form = new QFormLayout;
    form->addRow(QStringLiteral("书包名称："), m_nameEdit);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                                 this);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("保存"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(new QLabel(QStringLiteral("计件单价："), this));
    layout->addWidget(hint);
    if (empty)
        layout->addWidget(empty);
    layout->addLayout(tableButtons);
    layout->addWidget(m_priceTable, 1);
    layout->addWidget(box);

    connect(m_addRowButton, SIGNAL(clicked()), this, SLOT(addPriceRow()));
    connect(m_removeRowButton, SIGNAL(clicked()), this, SLOT(removePriceRow()));
    connect(box, SIGNAL(accepted()), this, SLOT(onAccept()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));
}

int BagDialog::nextUnusedProcessId() const
{
    QSet<int> used;
    for (int row = 0; row < m_priceTable->rowCount(); ++row) {
        QComboBox *combo = qobject_cast<QComboBox *>(m_priceTable->cellWidget(row, 0));
        if (combo)
            used.insert(combo->currentData().toInt());
    }
    for (int i = 0; i < m_processes.size(); ++i) {
        if (!used.contains(m_processes.at(i).id))
            return m_processes.at(i).id;
    }
    return 0;
}

void BagDialog::addPriceRow()
{
    if (m_processes.isEmpty())
        return;

    const int row = m_priceTable->rowCount();
    m_priceTable->insertRow(row);

    QComboBox *combo = new QComboBox(m_priceTable);
    for (int i = 0; i < m_processes.size(); ++i) {
        const Process &p = m_processes.at(i);
        combo->addItem(QStringLiteral("%1（通用 %2）")
                           .arg(p.name, QString::number(p.price, 'f', 2)),
                       p.id);
    }
    combo->installEventFilter(this);
    m_priceTable->setCellWidget(row, 0, combo);

    QDoubleSpinBox *spin = new QDoubleSpinBox(m_priceTable);
    spin->setRange(0.0, 10000.0);
    spin->setDecimals(2);
    spin->setSingleStep(0.05);
    spin->setSuffix(QStringLiteral(" 元"));
    spin->installEventFilter(this);
    m_priceTable->setCellWidget(row, 1, spin);

    const int preferred = nextUnusedProcessId();
    if (preferred > 0) {
        const int index = combo->findData(preferred);
        if (index >= 0)
            combo->setCurrentIndex(index);
    }
    highlightRow(row);
    combo->setFocus();
}

int BagDialog::rowOfWidget(const QObject *widget) const
{
    for (int row = 0; row < m_priceTable->rowCount(); ++row) {
        if (m_priceTable->cellWidget(row, 0) == widget
            || m_priceTable->cellWidget(row, 1) == widget) {
            return row;
        }
    }
    return -1;
}

void BagDialog::highlightRow(int row)
{
    if (row < 0 || row >= m_priceTable->rowCount())
        return;
    m_priceTable->setCurrentCell(row, 0);
    m_priceTable->selectRow(row);
}

bool BagDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusIn) {
        const int row = rowOfWidget(watched);
        if (row >= 0)
            highlightRow(row);
    }
    return QDialog::eventFilter(watched, event);
}

void BagDialog::removePriceRow()
{
    int row = m_priceTable->currentRow();
    if (row < 0)
        row = m_priceTable->rowCount() - 1;
    if (row < 0)
        return;

    m_priceTable->removeRow(row);

    // 删完把选中行落到相邻行，避免 currentRow 悬空到已删除的行
    if (m_priceTable->rowCount() > 0)
        highlightRow(qMin(row, m_priceTable->rowCount() - 1));
}

void BagDialog::setRowProcess(int row, int processId)
{
    QComboBox *combo = qobject_cast<QComboBox *>(m_priceTable->cellWidget(row, 0));
    if (!combo)
        return;
    const int index = combo->findData(processId);
    if (index >= 0)
        combo->setCurrentIndex(index);
}

void BagDialog::setRowPrice(int row, double price)
{
    QDoubleSpinBox *spin = qobject_cast<QDoubleSpinBox *>(m_priceTable->cellWidget(row, 1));
    if (spin)
        spin->setValue(price);
}

void BagDialog::setBag(const Bag &bag, const QList<BagProcess> &prices)
{
    m_bag = bag;
    m_nameEdit->setText(bag.name);
    m_priceTable->setRowCount(0);

    for (int i = 0; i < prices.size(); ++i) {
        addPriceRow();
        const int row = m_priceTable->rowCount() - 1;
        setRowProcess(row, prices.at(i).processId);
        setRowPrice(row, prices.at(i).price);
    }

    setWindowTitle(bag.id > 0 ? QStringLiteral("编辑书包") : QStringLiteral("新增书包"));
}

Bag BagDialog::bag() const
{
    Bag b = m_bag;
    b.name = m_nameEdit->text();
    return b;
}

QList<BagProcess> BagDialog::prices() const
{
    QList<BagProcess> result;
    for (int row = 0; row < m_priceTable->rowCount(); ++row) {
        QComboBox *combo = qobject_cast<QComboBox *>(m_priceTable->cellWidget(row, 0));
        QDoubleSpinBox *spin = qobject_cast<QDoubleSpinBox *>(m_priceTable->cellWidget(row, 1));
        if (!combo || !spin)
            continue;

        BagProcess bp;
        bp.bagId = m_bag.id;
        bp.processId = combo->currentData().toInt();
        bp.price = spin->value();
        result << bp;
    }
    return result;
}

void BagDialog::onAccept()
{
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请填写书包名称"));
        m_nameEdit->setFocus();
        return;
    }

    // 同一道工序重复配置：在对话框里就拦掉，别等 Service 报错
    QSet<int> seen;
    for (int row = 0; row < m_priceTable->rowCount(); ++row) {
        QComboBox *combo = qobject_cast<QComboBox *>(m_priceTable->cellWidget(row, 0));
        if (!combo)
            continue;
        const int processId = combo->currentData().toInt();
        if (seen.contains(processId)) {
            QMessageBox::information(
                this, QStringLiteral("提示"),
                QStringLiteral("同一道工序只能配一个单价，第 %1 行重复了。").arg(row + 1));
            m_priceTable->selectRow(row);
            return;
        }
        seen.insert(processId);
    }

    accept();
}
