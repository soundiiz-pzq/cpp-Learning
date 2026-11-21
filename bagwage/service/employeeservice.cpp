#include "service/employeeservice.h"
#include "dao/employeedao.h"
QList<Employee> EmployeeService::listAll() const
{
    EmployeeDao dao;
    return dao.listAll();
}

void EmployeeService::normalize(Employee *emp)
{
    // 去掉首尾空白，避免「张三 」和「张三」变成两个人
    emp->name = emp->name.trimmed();
    emp->floor = emp->floor.trimmed();
}

ServiceResult EmployeeService::save(Employee *emp)
{
    if (!emp)
        return ServiceResult::failure(QStringLiteral("内部错误：员工数据为空"));

    normalize(emp);

    if (emp->name.isEmpty())
        return ServiceResult::failure(QStringLiteral("员工姓名不能为空"));
    if (emp->floor.isEmpty())
        return ServiceResult::failure(QStringLiteral("楼层不能为空"));

    EmployeeDao dao;
    const bool ok = (emp->id > 0) ? dao.update(*emp) : dao.insert(emp);
    if (!ok)
        return ServiceResult::failure(QStringLiteral("保存失败：%1").arg(dao.lastError()));

    return ServiceResult::success();
}

ServiceResult EmployeeService::remove(int empId)
{
    if (empId <= 0)
        return ServiceResult::failure(QStringLiteral("内部错误：无效的员工 id"));

    EmployeeDao dao;

    // 引用校验：有计件明细就拦，不依赖数据库外键报错
    const int refs = dao.countOrderDetails(empId);
    if (refs < 0)
        return ServiceResult::failure(QStringLiteral("检查引用时出错：%1").arg(dao.lastError()));
    if (refs > 0) {
        return ServiceResult::failure(
            QStringLiteral("该员工还有 %1 条计件明细，不能删除。\n"
                           "请先在订单里移除这些明细。").arg(refs));
    }

    if (!dao.remove(empId))
        return ServiceResult::failure(QStringLiteral("删除失败：%1").arg(dao.lastError()));

    return ServiceResult::success();
}
