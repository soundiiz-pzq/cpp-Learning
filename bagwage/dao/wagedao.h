#ifndef WAGEDAO_H
#define WAGEDAO_H

#include "dao/models.h"

#include <QList>
#include <QString>
#include <QStringList>

// 工资核算的数据访问：按月拉出计件明细，带上员工名、工序名、
// 以及「该订单书包 × 该工序」的单价（没配价时 priced = false）。
// 金额不在这里算，交给 WageService 按「分」聚合。
class WageDao
{
public:
    // 有计件记录的月份（'yyyy-MM'），倒序
    QStringList months();

    // 某月的全部明细（含未定价的，priced 区分）
    QList<WageDetailLine> monthLines(const QString &month);

    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;
};

#endif // WAGEDAO_H
