#ifndef EMPLOYEEDAO_H
#define EMPLOYEEDAO_H

#include "dao/models.h"

#include <QList>
#include <QString>

// 员工表的数据访问对象：只写 SQL，不做业务判断、不弹窗、不开事务。
// 所有方法失败时返回 false，原因写进 lastError()。
class EmployeeDao
{
public:
    QList<Employee> listAll();
    bool            findById(int id, Employee *out);

    // 新增：id 由 SQLite 生成；成功时把新 id 写回 out.id
    bool insert(Employee *out);
    bool update(const Employee &e);
    bool remove(int id);

    // 引用校验用的计数（业务层据此决定能否删除）
    int  countOrderDetails(int empId);

    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
};

#endif // EMPLOYEEDAO_H
