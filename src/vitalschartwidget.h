#pragma once
#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QVector>
#include <QDateTime>

class VitalsChartWidget : public QWidget {
    Q_OBJECT

public:
    enum ChartType {
        Temperature,
        HeartRate,
        BloodOxygen,
        Combined
    };
    
    explicit VitalsChartWidget(ChartType type = Combined, QWidget* parent = nullptr);
    ~VitalsChartWidget();

    void addTemperaturePoint(double value, const QDateTime& timestamp = QDateTime::currentDateTime());
    void addHeartRatePoint(int value, const QDateTime& timestamp = QDateTime::currentDateTime());
    void addBloodOxygenPoint(int value, const QDateTime& timestamp = QDateTime::currentDateTime());
    
    void setData(const QVector<QPair<QDateTime, double>>& tempData,
                 const QVector<QPair<QDateTime, int>>& hrData,
                 const QVector<QPair<QDateTime, int>>& spo2Data);
    
    void setTimeRange(int minutes);
    void clear();
    
    void setChartType(ChartType type);
    ChartType chartType() const { return m_chartType; }

private:
    void setupChart();
    void updateTimeAxis();
    void applyTheme();

    ChartType m_chartType;
    
    QChartView* m_chartView;
    QChart* m_chart;
    
    QLineSeries* m_tempSeries;
    QLineSeries* m_hrSeries;
    QLineSeries* m_spo2Series;
    
    QDateTimeAxis* m_axisX;
    QValueAxis* m_axisYTemp;
    QValueAxis* m_axisYHr;
    QValueAxis* m_axisYSpo2;
    
    int m_timeRangeMinutes = 60;
    int m_maxPoints = 360;  // 最多显示的点数
};
