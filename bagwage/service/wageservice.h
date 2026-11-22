#ifndef WAGESERVICE_H
#define WAGESERVICE_H

#include "dao/models.h"
#include "service/serviceresult.h"

#include <QList>
#include <QString>
#include <QStringList>

// 计件工资核算。
//
// 规则：
//   工资 = Σ(明细数量 × 该订单书包 × 该工序的单价)
// 金额一律按「分」累加（qint64），最后格式化成元，避免 0.1+0.2 的浮点误差。
//
// 没配单价的明细**不能按 0 元算进总额**，而是单独通过 unpriced 返回，
// 界面要明确列出来让人去补价。
class WageService
{
public:
    // 有计件记录的月份（倒序）
    QStringList months() const;

    // 下拉框可选的月份：库里有记录的月份 ∪ 最近 recentMonths 个月（含本月），倒序。
    // 只给「有数据的月份」的话，没数据的月份永远选不到，
    // 「本月无计件记录」这句提示也就永远看不到。
    QStringList selectableMonths(int recentMonths = 12) const;

    QList<WageSummary> monthlySummary(const QString &month,
                                      QList<WageDetailLine> *unpriced) const;

    QList<WageBreakdown> breakdown(int empId, const QString &month) const;

    // 数量 × 单价，四舍五入到分
    static qint64 centsOf(int qty, double unitPrice);

    // 3950 -> "39.50"
    static QString formatCents(qint64 cents);
};

#endif // WAGESERVICE_H
