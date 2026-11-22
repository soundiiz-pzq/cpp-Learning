#include "service/wageservice.h"
#include "service/monthcatalog.h"
#include "dao/wagedao.h"

#include <QHash>
#include <QtGlobal>

#include <algorithm>

namespace {

bool byCentsDesc(const WageSummary &a, const WageSummary &b)
{
    if (a.totalCents != b.totalCents)
        return a.totalCents > b.totalCents;
    return a.empId < b.empId;
}

} // namespace

QStringList WageService::months() const
{
    return MonthCatalog::withRecords();
}

QStringList WageService::selectableMonths(int recentMonths) const
{
    return MonthCatalog::selectable(recentMonths);
}

qint64 WageService::centsOf(int qty, double unitPrice)
{
    // 单价先四舍五入到分，再乘数量：0.45 -> 45 分
    return qint64(qty) * qint64(qRound64(unitPrice * 100.0));
}

QString WageService::formatCents(qint64 cents)
{
    const bool negative = cents < 0;
    const qint64 abs = negative ? -cents : cents;
    const qint64 yuan = abs / 100;
    const qint64 fen = abs % 100;
    return QStringLiteral("%1%2.%3")
        .arg(negative ? QStringLiteral("-") : QString(),
             QString::number(yuan),
             QString::number(fen).rightJustified(2, QChar('0')));
}

QList<WageSummary> WageService::monthlySummary(const QString &month,
                                               QList<WageDetailLine> *unpriced) const
{
    QList<WageSummary> result;
    if (unpriced)
        unpriced->clear();

    WageDao dao;
    const QList<WageDetailLine> lines = dao.monthLines(month);

    QHash<int, int> indexOfEmp;   // empId -> result 下标
    for (int i = 0; i < lines.size(); ++i) {
        const WageDetailLine &line = lines.at(i);

        if (!line.priced) {
            if (unpriced)
                *unpriced << line;
            continue;   // 未定价：不进总额
        }

        const qint64 cents = centsOf(line.qty, line.unitPrice);

        if (!indexOfEmp.contains(line.empId)) {
            WageSummary s;
            s.empId = line.empId;
            s.empName = line.empName;
            indexOfEmp.insert(line.empId, result.size());
            result << s;
        }

        WageSummary &s = result[indexOfEmp.value(line.empId)];
        s.lineCount += 1;
        s.totalQty += line.qty;
        s.totalCents += cents;
    }

    std::sort(result.begin(), result.end(), byCentsDesc);
    return result;
}

QList<WageBreakdown> WageService::breakdown(int empId, const QString &month) const
{
    QList<WageBreakdown> result;
    if (empId <= 0)
        return result;

    WageDao dao;
    const QList<WageDetailLine> lines = dao.monthLines(month);

    QHash<int, int> indexOfProcess;
    for (int i = 0; i < lines.size(); ++i) {
        const WageDetailLine &line = lines.at(i);
        if (line.empId != empId || !line.priced)
            continue;

        if (!indexOfProcess.contains(line.processId)) {
            WageBreakdown b;
            b.processId = line.processId;
            b.processName = line.processName;
            b.unitPrice = line.unitPrice;
            indexOfProcess.insert(line.processId, result.size());
            result << b;
        }

        WageBreakdown &b = result[indexOfProcess.value(line.processId)];
        b.totalQty += line.qty;
        b.amountCents += centsOf(line.qty, line.unitPrice);
    }

    return result;
}
