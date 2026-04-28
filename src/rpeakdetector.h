#pragma once
#include <QObject>
#include <QVector>
#include <QPointF>
#include <deque>

// R波检测结果
struct RPeakInfo {
    int sampleIndex;       // R波在全局样本中的索引
    double amplitude;      // R波幅值 (mV)
    double timestamp;      // R波时间 (秒)
    double rrInterval;     // 与前一个R波的间隔 (秒), 首个为0
    double instantHR;      // 瞬时心率 (bpm), 首个为0
};

// 基于简化Pan-Tompkins算法的实时R波检测器
class RPeakDetector : public QObject {
    Q_OBJECT

public:
    explicit RPeakDetector(QObject* parent = nullptr);

    void setSampleRate(int sampleRate);
    int sampleRate() const { return m_sampleRate; }

    // 逐点/批量输入 (滤波后的mV值)
    void processSample(double value);
    void processSamples(const QVector<double>& values);

    void reset();

    // 查询结果
    const QVector<RPeakInfo>& detectedPeaks() const { return m_peaks; }
    int currentHeartRate() const { return m_currentHR; }
    double lastRRInterval() const;

signals:
    void rPeakDetected(const RPeakInfo& peak);
    void heartRateUpdated(int bpm);

private:
    // Pan-Tompkins 各阶段
    double bandpassFilter(double x);
    double derivative(double x);
    double squaring(double x);
    double movingWindowIntegration(double x);
    void detectPeak(double integratedValue, double originalValue);

    int m_sampleRate = 200;
    int m_globalIndex = 0;

    // 带通滤波器状态 (二阶IIR: 5-15Hz)
    double m_bpX[3] = {};  // 输入历史
    double m_bpY[3] = {};  // 输出历史
    // 高通部分
    double m_hpX[3] = {};
    double m_hpY[3] = {};

    // 微分器缓存
    std::deque<double> m_diffBuf;

    // 滑动窗口积分
    std::deque<double> m_intBuf;
    double m_intSum = 0.0;
    int m_windowSize = 30; // ~150ms at 200Hz

    // 峰值检测
    double m_threshold = 0.0;
    double m_signalLevel = 0.0;
    double m_noiseLevel = 0.0;
    int m_lastPeakIndex = -1;
    int m_refractorySamples = 40; // 200ms at 200Hz

    // 寻峰缓冲 (在积分信号上升沿结束后回溯找原始信号最大值)
    bool m_rising = false;
    double m_candidateMax = 0.0;
    int m_candidateIndex = -1;
    double m_candidateOriginal = 0.0;
    int m_candidateOriginalIndex = -1;

    // 原始值环形缓冲 (用于回溯)
    std::deque<double> m_originalBuf;
    int m_originalBufStart = 0; // 缓冲起始的全局索引

    // 检测结果
    QVector<RPeakInfo> m_peaks;
    int m_currentHR = 0;

    void updateHeartRate();
    void updateThreshold(double peakValue, bool isSignal);
};
