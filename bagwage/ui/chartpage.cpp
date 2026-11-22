#include "ui/chartpage.h"
#include "service/chartworker.h"
#include "service/monthcatalog.h"

#include <QComboBox>
#include <QDate>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QValueAxis>

QT_CHARTS_USE_NAMESPACE

ChartPage::ChartPage(QWidget *parent)
    : QWidget(parent)
    , m_monthCombo(new QComboBox(this))
    , m_statusLabel(new QLabel(this))
    , m_chartsWidget(new QWidget(this))
    , m_emptyLabel(new QLabel(this))
    , m_barView(new QChartView(this))
    , m_pieView(new QChartView(this))
    , m_requestId(0)
{
    m_monthCombo->setObjectName(QStringLiteral("chartMonthCombo"));
    QPushButton *refreshButton = new QPushButton(QStringLiteral("刷新"), this);

    m_statusLabel->setObjectName(QStringLiteral("chartStatusLabel"));
    m_statusLabel->setStyleSheet(QStringLiteral("color:#777;"));

    QHBoxLayout *top = new QHBoxLayout;
    top->addWidget(new QLabel(QStringLiteral("月份："), this));
    top->addWidget(m_monthCombo);
    top->addWidget(refreshButton);
    top->addSpacing(16);
    top->addWidget(m_statusLabel);
    top->addStretch(1);

    m_barView->setRenderHint(QPainter::Antialiasing);
    m_pieView->setRenderHint(QPainter::Antialiasing);
    // QGraphicsView 的 sizeHint 来自图表场景，横向并排会把页面撑得比窗口还宽；
    // 改成上下堆叠，宽度只跟页面走，轴标签和饼图都不会被挤出去
    m_barView->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_pieView->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    // 显式给一个极小的最小尺寸：QGraphicsView 的 minimumSizeHint 会把页面撑宽
    m_barView->setMinimumSize(1, 200);
    m_pieView->setMinimumSize(1, 180);
    // 宽度上限防止 QGraphicsView 把页面撑宽；高度上限防止两张图把页面撑高
    m_barView->setMaximumSize(1400, 360);
    m_pieView->setMaximumSize(1400, 270);
    m_barView->setFrameShape(QFrame::NoFrame);
    m_pieView->setFrameShape(QFrame::NoFrame);

    QVBoxLayout *charts = new QVBoxLayout(m_chartsWidget);
    charts->setContentsMargins(0, 0, 0, 0);
    charts->addWidget(m_barView, 3);
    charts->addWidget(m_pieView, 2);

    m_emptyLabel->setObjectName(QStringLiteral("chartEmptyLabel"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setVisible(false);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_chartsWidget, 1);
    layout->addWidget(m_emptyLabel, 1);

    connect(m_monthCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onMonthChanged()));
    connect(refreshButton, SIGNAL(clicked()), this, SLOT(onRefreshClicked()));

    refreshMonths();
    requestData(m_monthCombo->currentData().toString());
}

ChartPage::~ChartPage()
{
    // 等还在跑的统计线程结束，避免退出时访问已销毁的接收者
    const QList<ChartWorker *> workers = findChildren<ChartWorker *>();
    for (int i = 0; i < workers.size(); ++i) {
        if (workers.at(i)->isRunning())
            workers.at(i)->wait();
    }
}

void ChartPage::refreshMonths()
{
    const QStringList withData = MonthCatalog::withRecords();
    const QStringList options = MonthCatalog::selectable(12);
    const QString keep = m_monthCombo->currentData().toString();

    QString target = keep;
    if (target.isEmpty() || !options.contains(target)) {
        target = withData.isEmpty()
            ? QDate::currentDate().toString(QStringLiteral("yyyy-MM"))
            : withData.first();
    }

    QSignalBlocker blocker(m_monthCombo);
    m_monthCombo->clear();
    for (int i = 0; i < options.size(); ++i)
        m_monthCombo->addItem(options.at(i), options.at(i));

    const int index = m_monthCombo->findData(target);
    m_monthCombo->setCurrentIndex(index >= 0 ? index : 0);
}

void ChartPage::onMonthChanged()
{
    requestData(m_monthCombo->currentData().toString());
}

void ChartPage::onRefreshClicked()
{
    const QString keep = m_monthCombo->currentData().toString();
    refreshMonths();
    const int index = m_monthCombo->findData(keep);
    if (index >= 0)
        m_monthCombo->setCurrentIndex(index);
    requestData(m_monthCombo->currentData().toString());
}

void ChartPage::requestData(const QString &month)
{
    ++m_requestId;

    if (month.isEmpty()) {
        showEmpty(QStringLiteral("还没有任何计件记录"), false);
        return;
    }

    m_statusLabel->setText(QStringLiteral("统计中…"));
    m_emptyLabel->setVisible(false);

    ChartWorker *worker = new ChartWorker(month, m_requestId, this);
    connect(worker, SIGNAL(dataReady(ChartData)), this, SLOT(onDataReady(ChartData)));
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    worker->start();
}

void ChartPage::onDataReady(const ChartData &data)
{
    // 连点月份时旧线程可能后回来，直接丢弃
    if (data.requestId != m_requestId)
        return;

    if (!data.ok) {
        showEmpty(QStringLiteral("统计失败：%1").arg(data.error), true);
        return;
    }

    if (data.employees.isEmpty() && data.processes.isEmpty()) {
        showEmpty(QStringLiteral("该月没有计件记录"), false);
        return;
    }

    rebuildCharts(data);
}

void ChartPage::showEmpty(const QString &message, bool isError)
{
    m_chartsWidget->setVisible(false);
    m_emptyLabel->setText(message);
    m_emptyLabel->setStyleSheet(isError ? QStringLiteral("color:#b91c1c;")
                                        : QStringLiteral("color:#777;"));
    m_emptyLabel->setVisible(true);
    m_statusLabel->clear();
}

void ChartPage::rebuildCharts(const ChartData &data)
{
    // ---- 柱状图：各员工产量 ----
    QBarSet *barSet = new QBarSet(QStringLiteral("件数"));
    QStringList categories;
    for (int i = 0; i < data.employees.size(); ++i) {
        *barSet << data.employees.at(i).qty;
        categories << data.employees.at(i).label;
    }

    QBarSeries *barSeries = new QBarSeries();
    barSeries->append(barSet);

    QChart *barChart = new QChart();
    barChart->addSeries(barSeries);
    barChart->setTitle(QStringLiteral("各员工产量（%1）").arg(data.month));
    barChart->setAnimationOptions(QChart::SeriesAnimations);
    barChart->legend()->setVisible(false);

    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    barChart->addAxis(axisX, Qt::AlignBottom);
    barSeries->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis();
    axisY->setLabelFormat(QStringLiteral("%d"));
    axisY->setTitleText(QStringLiteral("件数"));
    barChart->addAxis(axisY, Qt::AlignLeft);
    barSeries->attachAxis(axisY);

    QChart *oldBar = m_barView->chart();
    m_barView->setChart(barChart);
    if (oldBar)
        delete oldBar;

    // ---- 饼图：各工序件数占比 ----
    QPieSeries *pieSeries = new QPieSeries();
    for (int i = 0; i < data.processes.size(); ++i) {
        QPieSlice *slice =
            pieSeries->append(data.processes.at(i).label, data.processes.at(i).qty);
        slice->setLabelVisible(true);
    }

    QChart *pieChart = new QChart();
    pieChart->addSeries(pieSeries);
    pieChart->setTitle(QStringLiteral("各工序件数占比（%1）").arg(data.month));
    pieChart->setAnimationOptions(QChart::SeriesAnimations);
    pieChart->legend()->setAlignment(Qt::AlignRight);

    QChart *oldPie = m_pieView->chart();
    m_pieView->setChart(pieChart);
    if (oldPie)
        delete oldPie;

    m_chartsWidget->setVisible(true);
    m_emptyLabel->clear();
    m_emptyLabel->setVisible(false);

    int total = 0;
    for (int i = 0; i < data.employees.size(); ++i)
        total += data.employees.at(i).qty;
    m_statusLabel->setText(QStringLiteral("统计完成：%1 名员工 / %2 道工序，共 %3 件")
                               .arg(data.employees.size())
                               .arg(data.processes.size())
                               .arg(total));
}
