#include "ui/processdialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

ProcessDialog::ProcessDialog(QWidget *parent)
    : QDialog(parent)
    , m_nameEdit(new QLineEdit(this))
    , m_priceSpin(new QDoubleSpinBox(this))
{
    setWindowTitle(QStringLiteral("工序信息"));
    setMinimumWidth(320);

    m_nameEdit->setPlaceholderText(QStringLiteral("例如：缝纫 / 剪线 / 包装"));
    m_priceSpin->setRange(0.0, 10000.0);
    m_priceSpin->setDecimals(2);
    m_priceSpin->setSingleStep(0.05);
    m_priceSpin->setSuffix(QStringLiteral(" 元/件"));

    QFormLayout *form = new QFormLayout;
    form->addRow(QStringLiteral("工序名称："), m_nameEdit);
    form->addRow(QStringLiteral("通用单价："), m_priceSpin);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                                 this);
    box->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    box->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(box);

    connect(box, SIGNAL(accepted()), this, SLOT(onAccept()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));
}

void ProcessDialog::setProcess(const Process &p)
{
    m_process = p;
    m_nameEdit->setText(p.name);
    m_priceSpin->setValue(p.price);
    setWindowTitle(p.id > 0 ? QStringLiteral("编辑工序") : QStringLiteral("新增工序"));
}

Process ProcessDialog::process() const
{
    Process p = m_process;
    p.name = m_nameEdit->text();
    p.price = m_priceSpin->value();
    return p;
}

void ProcessDialog::onAccept()
{
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请填写工序名称"));
        m_nameEdit->setFocus();
        return;
    }
    accept();
}
