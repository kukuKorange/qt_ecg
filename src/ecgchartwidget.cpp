#include "ecgchartwidget.h"
#include <QVBoxLayout>
#include <QPen>
#include <QBrush>

using namespace Qt;

EcgChartWidget::EcgChartWidget(QWidget* parent)
    : QWidget(parent)
    , m_chartView(new QChartView(this))
    , m_chart(new QChart())
    , m_series(new QLineSeries())
    , m_rpeakSeries(new QScatterSeries())
    , m_axisX(new QValueAxis())
    , m_axisY(new QValueAxis())
    , m_playbackTimer(new QTimer(this))
    , m_lineColor(QColor("#00ff88"))
    , m_backgroundColor(QColor("#0a1628"))
    , m_gridColor(QColor("#1a3a5c"))
    , m_rpeakDetector(new RPeakDetector(this))
{
    setupChart();

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_chartView);

    m_maxPoints = m_displayDuration * m_sampleRate;
    m_rpeakDetector->setSampleRate(m_sampleRate);

    connect(m_playbackTimer, &QTimer::timeout, this, &EcgChartWidget::onPlaybackTimer);
    connect(m_rpeakDetector, &RPeakDetector::heartRateUpdated, this, &EcgChartWidget::heartRateFromEcg);
}

EcgChartWidget::~EcgChartWidget()
{
    stopPlayback();
}

void EcgChartWidget::setupChart()
{
    // 配置图表外观
    m_chart->setBackgroundBrush(QBrush(m_backgroundColor));
    m_chart->setBackgroundRoundness(0);
    m_chart->legend()->hide();
    m_chart->setMargins(QMargins(10, 10, 10, 10));
    m_chart->setTitle("");
    
    // 配置系列线条
    QPen pen(m_lineColor);
    pen.setWidth(2);
    m_series->setPen(pen);
    m_chart->addSeries(m_series);

    // 配置R波标记点
    m_rpeakSeries->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    m_rpeakSeries->setMarkerSize(10);
    m_rpeakSeries->setColor(QColor("#ff4444"));
    m_rpeakSeries->setBorderColor(QColor("#ff8888"));
    m_chart->addSeries(m_rpeakSeries);
    
    // 配置X轴
    m_axisX->setRange(0, m_displayDuration);
    m_axisX->setLabelFormat("%.2f");
    m_axisX->setTitleText(QStringLiteral("时间 (秒)"));
    m_axisX->setTitleBrush(QBrush(Qt::white));
    m_axisX->setLabelsBrush(QBrush(Qt::white));
    m_axisX->setGridLineColor(m_gridColor);
    m_axisX->setLinePen(QPen(m_gridColor));
    m_axisX->setTickCount(11);  // 每0.5秒一个刻度 (5秒 / 0.5 + 1)
    m_axisX->setMinorTickCount(4);  // 次刻度
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_series->attachAxis(m_axisX);
    m_rpeakSeries->attachAxis(m_axisX);
    
    // 配置Y轴 - ADC转换后范围约 -1650mV 到 +1650mV
    m_axisY->setRange(-500.0, 500.0);  // 默认显示 ±500mV
    m_axisY->setLabelFormat("%.0f");
    m_axisY->setTitleText(QStringLiteral("电压 (mV)"));
    m_axisY->setTitleBrush(QBrush(Qt::white));
    m_axisY->setLabelsBrush(QBrush(Qt::white));
    m_axisY->setGridLineColor(m_gridColor);
    m_axisY->setLinePen(QPen(m_gridColor));
    m_axisY->setTickCount(11);  // 每100mV一个刻度
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    m_series->attachAxis(m_axisY);
    m_rpeakSeries->attachAxis(m_axisY);
    
    // 配置图表视图
    m_chartView->setChart(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setBackgroundBrush(QBrush(m_backgroundColor));
}

void EcgChartWidget::addDataPoint(double value)
{
    // 应用低通滤波
    double filteredValue = applyLowPassFilter(value);

    double x = static_cast<double>(m_currentIndex) / m_sampleRate;
    m_series->append(x, filteredValue);
    m_currentIndex++;

    // R波检测 (使用滤波后的值)
    if (m_rpeakEnabled) {
        m_rpeakDetector->processSample(filteredValue);
    }

    // 限制显示点数
    while (m_series->count() > m_maxPoints) {
        m_series->remove(0);
    }

    updateAxisRange();
    updateRPeakMarkers();
}

void EcgChartWidget::addDataPoints(const QVector<double>& values)
{
    for (double value : values) {
        // 应用低通滤波
        double filteredValue = applyLowPassFilter(value);

        double x = static_cast<double>(m_currentIndex) / m_sampleRate;
        m_series->append(x, filteredValue);
        m_currentIndex++;

        // R波检测
        if (m_rpeakEnabled) {
            m_rpeakDetector->processSample(filteredValue);
        }
    }

    // 限制显示点数
    while (m_series->count() > m_maxPoints) {
        m_series->remove(0);
    }

    updateAxisRange();
    updateRPeakMarkers();
}

void EcgChartWidget::clear()
{
    m_series->clear();
    m_rpeakSeries->clear();
    m_currentIndex = 0;
    m_axisX->setRange(0, m_displayDuration);

    // 重置滤波器状态
    m_filterInitialized = false;
    m_lastFilteredValue = 0.0;

    // 重置R波检测器
    m_rpeakDetector->reset();
}

void EcgChartWidget::setDisplayDuration(int seconds)
{
    m_displayDuration = seconds;
    m_maxPoints = m_displayDuration * m_sampleRate;
    updateAxisRange();
}

void EcgChartWidget::setSampleRate(int samplesPerSecond)
{
    m_sampleRate = samplesPerSecond;
    m_maxPoints = m_displayDuration * m_sampleRate;
    m_rpeakDetector->setSampleRate(samplesPerSecond);
}

void EcgChartWidget::setGridVisible(bool visible)
{
    m_axisX->setGridLineVisible(visible);
    m_axisY->setGridLineVisible(visible);
}

void EcgChartWidget::setAnimationEnabled(bool enabled)
{
    m_chart->setAnimationOptions(enabled ? QChart::SeriesAnimations : QChart::NoAnimation);
}

void EcgChartWidget::startPlayback(const QVector<double>& data, int sampleRate)
{
    if (data.isEmpty()) return;
    
    stopPlayback();
    clear();
    
    m_playbackData = data;
    m_sampleRate = sampleRate;
    m_playbackIndex = 0;
    m_isPlaying = true;
    
    // 每次回放10个点以保持流畅
    m_playbackTimer->start(1000 / (sampleRate / 10));
}

void EcgChartWidget::stopPlayback()
{
    m_playbackTimer->stop();
    m_isPlaying = false;
    m_playbackData.clear();
    m_playbackIndex = 0;
}

void EcgChartWidget::onPlaybackTimer()
{
    if (!m_isPlaying || m_playbackIndex >= m_playbackData.size()) {
        stopPlayback();
        emit playbackFinished();
        return;
    }
    
    // 每次添加10个点
    int batchSize = 10;
    for (int i = 0; i < batchSize && m_playbackIndex < m_playbackData.size(); ++i) {
        addDataPoint(m_playbackData[m_playbackIndex++]);
    }
}

void EcgChartWidget::updateAxisRange()
{
    if (m_series->count() == 0) return;
    
    double minX = m_series->at(0).x();
    double maxX = m_series->at(m_series->count() - 1).x();
    
    if (maxX - minX > m_displayDuration) {
        m_axisX->setRange(maxX - m_displayDuration, maxX);
    } else {
        m_axisX->setRange(minX, minX + m_displayDuration);
    }
    
    // 自动调整Y轴范围
    double minY = 0, maxY = 0;
    for (int i = 0; i < m_series->count(); ++i) {
        double y = m_series->at(i).y();
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }
    
    double padding = (maxY - minY) * 0.2;
    if (padding < 50.0) padding = 50.0;  // 最小50mV的边距
    m_axisY->setRange(minY - padding, maxY + padding);
}

void EcgChartWidget::setLineColor(const QColor& color)
{
    m_lineColor = color;
    QPen pen(color);
    pen.setWidth(2);
    m_series->setPen(pen);
}

void EcgChartWidget::setBackgroundColor(const QColor& color)
{
    m_backgroundColor = color;
    m_chart->setBackgroundBrush(QBrush(color));
    m_chartView->setBackgroundBrush(QBrush(color));
}

void EcgChartWidget::setGridColor(const QColor& color)
{
    m_gridColor = color;
    m_axisX->setGridLineColor(color);
    m_axisY->setGridLineColor(color);
}

void EcgChartWidget::setFilterEnabled(bool enabled)
{
    m_filterEnabled = enabled;
    if (!enabled) {
        // 禁用滤波时重置滤波器状态
        m_filterInitialized = false;
        m_lastFilteredValue = 0.0;
    }
}

void EcgChartWidget::setFilterCoefficient(double alpha)
{
    // 限制系数范围在 0.01 到 1.0 之间
    m_filterAlpha = qBound(0.01, alpha, 1.0);
}

void EcgChartWidget::setRPeakDetectionEnabled(bool enabled)
{
    m_rpeakEnabled = enabled;
    m_rpeakSeries->setVisible(enabled);
    if (!enabled) {
        m_rpeakSeries->clear();
        m_rpeakDetector->reset();
    }
}

void EcgChartWidget::updateRPeakMarkers()
{
    if (!m_rpeakEnabled) return;

    // 获取当前显示的X轴范围
    double xMin = m_axisX->min();
    double xMax = m_axisX->max();

    // 重建可见范围内的R波标记
    m_rpeakSeries->clear();
    const auto& peaks = m_rpeakDetector->detectedPeaks();
    for (int i = peaks.size() - 1; i >= 0; --i) {
        double t = peaks[i].timestamp;
        if (t < xMin) break;
        if (t <= xMax) {
            m_rpeakSeries->append(t, peaks[i].amplitude);
        }
    }
}

double EcgChartWidget::applyLowPassFilter(double rawValue)
{
    if (!m_filterEnabled) {
        return rawValue;
    }
    
    if (!m_filterInitialized) {
        m_lastFilteredValue = rawValue;
        m_filterInitialized = true;
        return rawValue;
    }
    
    // 低通滤波: filtered = last_filtered + (raw - last_filtered) * alpha
    double filtered = m_lastFilteredValue + (rawValue - m_lastFilteredValue) * m_filterAlpha;
    m_lastFilteredValue = filtered;
    return filtered;
}