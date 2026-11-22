#ifndef CHARTPAGE_H
#define CHARTPAGE_H

#include "dao/models.h"

#include <QtCharts/QChartGlobal>
#include <QWidget>

class QComboBox;
class QLabel;
class ChartWorker;

QT_CHARTS_BEGIN_NAMESPACE
class QChartView;
QT_CHARTS_END_NAMESPACE

// 月度统计图表页：柱状图（各员工产量）+ 饼图（各工序件数占比）。
// 聚合在子线程做，主线程只负责画图；切月份时用自增 requestId 丢弃过期结果。
class ChartPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChartPage(QWidget *parent = 0);
    ~ChartPage();

private slots:
    void onMonthChanged();
    void onRefreshClicked();
    void onDataReady(const ChartData &data);

private:
    void refreshMonths();
    void requestData(const QString &month);
    void rebuildCharts(const ChartData &data);
    void showEmpty(const QString &message, bool isError);

    QComboBox  *m_monthCombo;
    QLabel     *m_statusLabel;
    QWidget    *m_chartsWidget;
    QLabel     *m_emptyLabel;
    QtCharts::QChartView *m_barView;
    QtCharts::QChartView *m_pieView;

    int m_requestId;   // 自增，用来识别过期结果
};

#endif // CHARTPAGE_H
