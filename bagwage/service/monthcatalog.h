#ifndef MONTHCATALOG_H
#define MONTHCATALOG_H

#include <QStringList>

// 月份下拉的候选列表（工资页和图表页共用）。
//
// 只列「有记录的月份」会导致没有数据的月份永远选不到，
// 「本月无计件记录」这类提示也就永远看不到。
class MonthCatalog
{
public:
    // 有计件记录的月份（'yyyy-MM'），倒序
    static QStringList withRecords();

    // 有记录的月份 ∪ 最近 recentMonths 个月（含本月），倒序
    static QStringList selectable(int recentMonths = 12);
};

#endif // MONTHCATALOG_H
