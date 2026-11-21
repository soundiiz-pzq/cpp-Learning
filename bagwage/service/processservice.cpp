#include "service/processservice.h"
#include "dao/processdao.h"

#include <QStringList>
#include <cmath>

QList<Process> ProcessService::listAll() const
{
    ProcessDao dao;
    return dao.listAll();
}

void ProcessService::normalize(Process *p)
{
    p->name = p->name.trimmed();
    // 单价统一保留两位小数，避免 0.4500000001 这种脏数据
    p->price = std::floor(p->price * 100.0 + 0.5) / 100.0;
}

ServiceResult ProcessService::save(Process *p)
{
    if (!p)
        return ServiceResult::failure(QStringLiteral("内部错误：工序数据为空"));

    normalize(p);

    if (p->name.isEmpty())
        return ServiceResult::failure(QStringLiteral("工序名称不能为空"));
    if (p->price < 0.0)
        return ServiceResult::failure(QStringLiteral("单价不能为负数"));
    if (p->price > 10000.0)
        return ServiceResult::failure(QStringLiteral("单价看起来过大（上限 10000），请确认"));

    ProcessDao dao;
    const bool ok = (p->id > 0) ? dao.update(*p) : dao.insert(p);
    if (!ok)
        return ServiceResult::failure(QStringLiteral("保存失败：%1").arg(dao.lastError()));

    return ServiceResult::success();
}

ServiceResult ProcessService::remove(int processId)
{
    if (processId <= 0)
        return ServiceResult::failure(QStringLiteral("内部错误：无效的工序 id"));

    ProcessDao dao;

    // 第一处引用：书包单价表
    const int bagRefs = dao.countBagProcesses(processId);
    if (bagRefs < 0)
        return ServiceResult::failure(QStringLiteral("检查书包引用时出错：%1").arg(dao.lastError()));

    // 第二处引用：计件明细
    const int detailRefs = dao.countOrderDetails(processId);
    if (detailRefs < 0)
        return ServiceResult::failure(QStringLiteral("检查计件明细时出错：%1").arg(dao.lastError()));

    if (bagRefs == 0 && detailRefs == 0) {
        if (!dao.remove(processId))
            return ServiceResult::failure(QStringLiteral("删除失败：%1").arg(dao.lastError()));
        return ServiceResult::success();
    }

    // 把「被谁引用」说清楚，别让用户去猜
    QStringList reasons;

    if (bagRefs > 0) {
        const QList<ReferenceInfo> bags = dao.referencingBags(processId);
        QStringList names;
        for (int i = 0; i < bags.size() && i < 3; ++i)
            names << bags.at(i).name;

        QString line = QStringLiteral("被 %1 款书包配了单价").arg(bagRefs);
        if (!names.isEmpty()) {
            line += QStringLiteral("（%1%2）")
                        .arg(names.join(QStringLiteral("、")),
                             bags.size() > 3 ? QStringLiteral(" 等") : QString());
        }
        reasons << line;
    }

    if (detailRefs > 0)
        reasons << QStringLiteral("被 %1 条计件明细使用").arg(detailRefs);

    return ServiceResult::failure(
        QStringLiteral("这道工序不能删除，它%1。\n"
                       "请先在书包单价表或计件明细里处理好这些引用。")
            .arg(reasons.join(QStringLiteral("，且"))));
}
