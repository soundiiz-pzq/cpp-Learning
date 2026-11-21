#ifndef EMPLOYEESERVICE_H
#define EMPLOYEESERVICE_H

#include "dao/models.h"
#include "service/serviceresult.h"

#include <QList>

// 员工业务逻辑：负责「输入校验 + 引用校验」。
// UI 只调用这一层，不直接碰 EmployeeDao。
class EmployeeService
{
public:
    QList<Employee> listAll() const;

    // 新增（emp.id == 0）或修改（emp.id > 0）。
    // 成功时把新生成的 id 写回 emp->id。
    ServiceResult save(Employee *emp);

    // 被 order_detail 引用的员工不允许删除
    ServiceResult remove(int empId);

private:
    static void normalize(Employee *emp);
};

#endif // EMPLOYEESERVICE_H
