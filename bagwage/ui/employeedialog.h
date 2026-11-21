#ifndef EMPLOYEEDIALOG_H
#define EMPLOYEEDIALOG_H

#include "dao/models.h"

#include <QDialog>

class QLineEdit;

// 员工录入/编辑对话框。
// UI 层只做「收集输入 + 弹提示」，校验规则在 EmployeeService。
class EmployeeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EmployeeDialog(QWidget *parent = 0);

    void     setEmployee(const Employee &e);
    Employee employee() const;

private slots:
    void onAccept();

private:
    QLineEdit *m_nameEdit;
    QLineEdit *m_floorEdit;
    Employee   m_employee;   // 保留 id，修改时用
};

#endif // EMPLOYEEDIALOG_H
