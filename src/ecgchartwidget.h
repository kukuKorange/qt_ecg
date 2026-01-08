#pragma once
#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QTimer>
#include <QVector>

class EcgChartWidget : public QWidget {
    Q_OBJECT

public:
    explicit EcgChartWidget(QWidget* parent = nullptr);
    ~EcgChartWidget();

    void addDataPoint(double value);
    void addDataPoints(const QVector<double>& values);
    void clear();
    
    void setDisplayDuration(int seconds);
    void setSampleRate(int samplesPerSecond);
    void setGridVisible(bool visible);
    void setAnimationEnabled(bool enabled);
    
    void startPlayback(const QVector<double>& data, int sampleRate = 250);
    void stopPlayback();
    bool isPlaying() const { return m_isPlaying; }
    
    void setLineColor(const QColor& color);
    void setBackgroundColor(const QColor& color);
    void setGridColor(const QColor& color);

signals:
    void playbackFinished();

private slots:
    void onPlaybackTimer();

private:
    void setupChart();
    void updateAxisRange();

    QChartView* m_chartView;
    QChart* m_chart;
    QLineSeries* m_series;
    QValueAxis* m_axisX;
    QValueAxis* m_axisY;
    
    int m_displayDuration = 5;  // seconds
    int m_sampleRate = 200;     // samples per second (20 points per 100ms)
    int m_maxPoints;
    int m_currentIndex = 0;
    
    QVector<double> m_playbackData;
    QTimer* m_playbackTimer;
    int m_playbackIndex = 0;
    bool m_isPlaying = false;
    
    QColor m_lineColor;
    QColor m_backgroundColor;
    QColor m_gridColor;
};
