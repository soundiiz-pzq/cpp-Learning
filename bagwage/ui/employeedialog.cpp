#include "ui/employeedialog.h"
#include "service/employeeservice.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

EmployeeDialog::EmployeeDialog(QWidget *parent)
    : QDialog(parent)
    , m_nameEdit(new QLineEdit(this))
    , m_floorEdit(new QLineEdit(this))
{
    setWindowTitle(QStringLiteral("员工信息"));
    setMinimumWidth(320);

    QFormLayout *form = new QFormLayout;
    form->addRow(QStringLiteral("姓名："), m_nameEdit);
    form->addRow(QStringLiteral("楼层："), m_floorEdit);
    m_floorEdit->setPlaceholderText(QStringLiteral("例如：一楼 / 2F"));

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

void EmployeeDialog::setEmployee(const Employee &e)
{
    m_employee = e;
    m_nameEdit->setText(e.name);
    m_floorEdit->setText(e.floor);
    setWindowTitle(e.id > 0 ? QStringLiteral("编辑员工") : QStringLiteral("新增员工"));
}

Employee EmployeeDialog::employee() const
{
    Employee e = m_employee;
    e.name = m_nameEdit->text();
    e.floor = m_floorEdit->text();
    return e;
}

void EmployeeDialog::onAccept()
{
    // 只做最基础的必填提示，规范化与业务校验交给 Service
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请填写员工姓名"));
        m_nameEdit->setFocus();
        return;
    }
    if (m_floorEdit->text().trimmed().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请填写楼层"));
        m_floorEdit->setFocus();
        return;
    }
    accept();
}
