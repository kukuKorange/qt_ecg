#include "vitalschartwidget.h"
#include <QVBoxLayout>
#include <QPen>
#include <QBrush>

using namespace Qt;

VitalsChartWidget::VitalsChartWidget(ChartType type, QWidget* parent)
    : QWidget(parent)
    , m_chartType(type)
    , m_chartView(new QChartView(this))
    , m_chart(new QChart())
    , m_tempSeries(new QLineSeries())
    , m_hrSeries(new QLineSeries())
    , m_spo2Series(new QLineSeries())
    , m_axisX(new QDateTimeAxis())
    , m_axisYTemp(new QValueAxis())
    , m_axisYHr(new QValueAxis())
    , m_axisYSpo2(new QValueAxis())
{
    setupChart();
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_chartView);
}

VitalsChartWidget::~VitalsChartWidget()
{
}

void VitalsChartWidget::setupChart()
{
    // 应用深色主题
    applyTheme();
    
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);
    m_chart->legend()->setLabelBrush(QBrush(Qt::white));
    m_chart->setMargins(QMargins(10, 10, 10, 10));
    
    // 配置时间轴
    m_axisX->setFormat("HH:mm:ss");
    m_axisX->setTitleText(QStringLiteral("时间"));
    m_axisX->setTitleBrush(QBrush(Qt::white));
    m_axisX->setLabelsBrush(QBrush(Qt::white));
    m_axisX->setGridLineColor(QColor("#2a4a6a"));
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    
    // 体温系列 - 橙红色
    m_tempSeries->setName(QStringLiteral("体温 (°C)"));
    QPen tempPen(QColor("#ff6b6b"));
    tempPen.setWidth(2);
    m_tempSeries->setPen(tempPen);
    
    m_axisYTemp->setRange(35.0, 42.0);
    m_axisYTemp->setTitleText(QStringLiteral("体温 (°C)"));
    m_axisYTemp->setTitleBrush(QBrush(QColor("#ff6b6b")));
    m_axisYTemp->setLabelsBrush(QBrush(QColor("#ff6b6b")));
    m_axisYTemp->setGridLineColor(QColor("#2a4a6a"));
    
    // 心率系列 - 青色
    m_hrSeries->setName(QStringLiteral("心率 (bpm)"));
    QPen hrPen(QColor("#4ecdc4"));
    hrPen.setWidth(2);
    m_hrSeries->setPen(hrPen);
    
    m_axisYHr->setRange(40, 180);
    m_axisYHr->setTitleText(QStringLiteral("心率 (bpm)"));
    m_axisYHr->setTitleBrush(QBrush(QColor("#4ecdc4")));
    m_axisYHr->setLabelsBrush(QBrush(QColor("#4ecdc4")));
    m_axisYHr->setGridLineColor(QColor("#2a4a6a"));
    
    // 血氧系列 - 蓝色
    m_spo2Series->setName(QStringLiteral("血氧 (%)"));
    QPen spo2Pen(QColor("#45b7d1"));
    spo2Pen.setWidth(2);
    m_spo2Series->setPen(spo2Pen);
    
    m_axisYSpo2->setRange(85, 100);
    m_axisYSpo2->setTitleText(QStringLiteral("血氧 (%)"));
    m_axisYSpo2->setTitleBrush(QBrush(QColor("#45b7d1")));
    m_axisYSpo2->setLabelsBrush(QBrush(QColor("#45b7d1")));
    m_axisYSpo2->setGridLineColor(QColor("#2a4a6a"));
    
    // 根据图表类型添加系列和轴
    switch (m_chartType) {
        case Temperature:
            m_chart->addSeries(m_tempSeries);
            m_chart->addAxis(m_axisYTemp, Qt::AlignLeft);
            m_tempSeries->attachAxis(m_axisX);
            m_tempSeries->attachAxis(m_axisYTemp);
            m_chart->setTitle(QStringLiteral("体温趋势"));
            break;
            
        case HeartRate:
            m_chart->addSeries(m_hrSeries);
            m_chart->addAxis(m_axisYHr, Qt::AlignLeft);
            m_hrSeries->attachAxis(m_axisX);
            m_hrSeries->attachAxis(m_axisYHr);
            m_chart->setTitle(QStringLiteral("心率趋势"));
            break;
            
        case BloodOxygen:
            m_chart->addSeries(m_spo2Series);
            m_chart->addAxis(m_axisYSpo2, Qt::AlignLeft);
            m_spo2Series->attachAxis(m_axisX);
            m_spo2Series->attachAxis(m_axisYSpo2);
            m_chart->setTitle(QStringLiteral("血氧趋势"));
            break;
            
        case Combined:
        default:
            m_chart->addSeries(m_tempSeries);
            m_chart->addSeries(m_hrSeries);
            m_chart->addSeries(m_spo2Series);
            
            m_chart->addAxis(m_axisYTemp, Qt::AlignLeft);
            m_chart->addAxis(m_axisYHr, Qt::AlignRight);
            
            m_tempSeries->attachAxis(m_axisX);
            m_tempSeries->attachAxis(m_axisYTemp);
            m_hrSeries->attachAxis(m_axisX);
            m_hrSeries->attachAxis(m_axisYHr);
            m_spo2Series->attachAxis(m_axisX);
            m_spo2Series->attachAxis(m_axisYHr);
            
            m_chart->setTitle(QStringLiteral("生命体征趋势"));
            break;
    }
    
    m_chart->setTitleBrush(QBrush(Qt::white));
    m_chartView->setChart(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    
    updateTimeAxis();
}

void VitalsChartWidget::applyTheme()
{
    QColor bgColor("#0a1628");
    m_chart->setBackgroundBrush(QBrush(bgColor));
    m_chart->setBackgroundRoundness(8);
    m_chartView->setBackgroundBrush(QBrush(bgColor));
}

void VitalsChartWidget::addTemperaturePoint(double value, const QDateTime& timestamp)
{
    m_tempSeries->append(timestamp.toMSecsSinceEpoch(), value);
    
    while (m_tempSeries->count() > m_maxPoints) {
        m_tempSeries->remove(0);
    }
    
    updateTimeAxis();
}

void VitalsChartWidget::addHeartRatePoint(int value, const QDateTime& timestamp)
{
    m_hrSeries->append(timestamp.toMSecsSinceEpoch(), value);
    
    while (m_hrSeries->count() > m_maxPoints) {
        m_hrSeries->remove(0);
    }
    
    updateTimeAxis();
}

void VitalsChartWidget::addBloodOxygenPoint(int value, const QDateTime& timestamp)
{
    m_spo2Series->append(timestamp.toMSecsSinceEpoch(), value);
    
    while (m_spo2Series->count() > m_maxPoints) {
        m_spo2Series->remove(0);
    }
    
    updateTimeAxis();
}

void VitalsChartWidget::setData(const QVector<QPair<QDateTime, double>>& tempData,
                                 const QVector<QPair<QDateTime, int>>& hrData,
                                 const QVector<QPair<QDateTime, int>>& spo2Data)
{
    m_tempSeries->clear();
    m_hrSeries->clear();
    m_spo2Series->clear();
    
    for (const auto& pair : tempData) {
        m_tempSeries->append(pair.first.toMSecsSinceEpoch(), pair.second);
    }
    
    for (const auto& pair : hrData) {
        m_hrSeries->append(pair.first.toMSecsSinceEpoch(), pair.second);
    }
    
    for (const auto& pair : spo2Data) {
        m_spo2Series->append(pair.first.toMSecsSinceEpoch(), pair.second);
    }
    
    updateTimeAxis();
}

void VitalsChartWidget::setTimeRange(int minutes)
{
    m_timeRangeMinutes = minutes;
    updateTimeAxis();
}

void VitalsChartWidget::clear()
{
    m_tempSeries->clear();
    m_hrSeries->clear();
    m_spo2Series->clear();
    updateTimeAxis();
}

void VitalsChartWidget::setChartType(ChartType type)
{
    if (m_chartType == type) return;
    
    m_chartType = type;
    
    // 重新设置图表
    m_chart->removeAllSeries();
    
    // 重新创建系列
    m_tempSeries = new QLineSeries();
    m_hrSeries = new QLineSeries();
    m_spo2Series = new QLineSeries();
    
    setupChart();
}

void VitalsChartWidget::updateTimeAxis()
{
    QDateTime now = QDateTime::currentDateTime();
    QDateTime start = now.addSecs(-m_timeRangeMinutes * 60);
    
    m_axisX->setRange(start, now);
    
    // 根据时间范围调整时间格式
    if (m_timeRangeMinutes <= 5) {
        m_axisX->setFormat("HH:mm:ss");
    } else if (m_timeRangeMinutes <= 60) {
        m_axisX->setFormat("HH:mm");
    } else {
        m_axisX->setFormat("MM-dd HH:mm");
    }
}
