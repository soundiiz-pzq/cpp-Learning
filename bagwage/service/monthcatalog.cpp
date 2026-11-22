#include "service/monthcatalog.h"
#include "dao/wagedao.h"

#include <QDate>
#include <QSet>

#include <algorithm>

QStringList MonthCatalog::withRecords()
{
    WageDao dao;
    return dao.months();
}

QStringList MonthCatalog::selectable(int recentMonths)
{
    QSet<QString> merged;
    const QStringList recorded = withRecords();
    for (int i = 0; i < recorded.size(); ++i)
        merged.insert(recorded.at(i));

    if (recentMonths > 0) {
        const QDate today = QDate::currentDate();
        for (int i = 0; i < recentMonths; ++i)
            merged.insert(today.addMonths(-i).toString(QStringLiteral("yyyy-MM")));
    }

    QStringList result = merged.toList();
    std::sort(result.begin(), result.end());
    std::reverse(result.begin(), result.end());   // 'yyyy-MM' 倒序即时间倒序
    return result;
}
