#include "rpeakdetector.h"
#include <QtMath>
#include <QDebug>
#include <algorithm>
#include <numeric>

RPeakDetector::RPeakDetector(QObject* parent)
    : QObject(parent)
{
    setSampleRate(200);
}

void RPeakDetector::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate;
    // 滑动窗口 ~150ms
    m_windowSize = qMax(1, static_cast<int>(0.15 * sampleRate));
    // 不应期 ~200ms (生理上QRS波群最短间隔)
    m_refractorySamples = static_cast<int>(0.2 * sampleRate);
    reset();
}

void RPeakDetector::reset()
{
    m_globalIndex = 0;
    std::fill(std::begin(m_bpX), std::end(m_bpX), 0.0);
    std::fill(std::begin(m_bpY), std::end(m_bpY), 0.0);
    std::fill(std::begin(m_hpX), std::end(m_hpX), 0.0);
    std::fill(std::begin(m_hpY), std::end(m_hpY), 0.0);
    m_diffBuf.clear();
    m_intBuf.clear();
    m_intSum = 0.0;
    m_threshold = 0.0;
    m_signalLevel = 0.0;
    m_noiseLevel = 0.0;
    m_lastPeakIndex = -1;
    m_rising = false;
    m_candidateMax = 0.0;
    m_candidateIndex = -1;
    m_candidateOriginal = 0.0;
    m_candidateOriginalIndex = -1;
    m_originalBuf.clear();
    m_originalBufStart = 0;
    m_peaks.clear();
    m_currentHR = 0;
}

void RPeakDetector::processSample(double value)
{
    // 保存原始值用于回溯找R波真实幅值
    m_originalBuf.push_back(value);
    // 保留最近2秒数据
    int maxBuf = m_sampleRate * 2;
    while (static_cast<int>(m_originalBuf.size()) > maxBuf) {
        m_originalBuf.pop_front();
        m_originalBufStart++;
    }

    // Pan-Tompkins 处理流水线
    double bp = bandpassFilter(value);
    double diff = derivative(bp);
    double sq = squaring(diff);
    double integ = movingWindowIntegration(sq);

    detectPeak(integ, value);

    m_globalIndex++;
}

void RPeakDetector::processSamples(const QVector<double>& values)
{
    for (double v : values) {
        processSample(v);
    }
}

// ============================================================
// 带通滤波: 低通 + 高通 实现 5-15Hz 带通
// 使用二阶Butterworth IIR滤波器
// ============================================================

double RPeakDetector::bandpassFilter(double x)
{
    // --- 低通滤波 (截止频率 ~15Hz) ---
    // 二阶Butterworth低通, 根据采样率计算系数
    // 使用预翘曲的双线性变换简化系数
    double fc_lp = 15.0;
    double w_lp = qTan(M_PI * fc_lp / m_sampleRate);
    double w2_lp = w_lp * w_lp;
    double k_lp = 1.0 / (1.0 + M_SQRT2 * w_lp + w2_lp);

    double a0_lp = w2_lp * k_lp;
    double a1_lp = 2.0 * a0_lp;
    double a2_lp = a0_lp;
    double b1_lp = 2.0 * (w2_lp - 1.0) * k_lp;
    double b2_lp = (1.0 - M_SQRT2 * w_lp + w2_lp) * k_lp;

    // 移位
    m_bpX[2] = m_bpX[1]; m_bpX[1] = m_bpX[0]; m_bpX[0] = x;
    m_bpY[2] = m_bpY[1]; m_bpY[1] = m_bpY[0];
    m_bpY[0] = a0_lp * m_bpX[0] + a1_lp * m_bpX[1] + a2_lp * m_bpX[2]
               - b1_lp * m_bpY[1] - b2_lp * m_bpY[2];

    double lpOut = m_bpY[0];

    // --- 高通滤波 (截止频率 ~5Hz) ---
    double fc_hp = 5.0;
    double w_hp = qTan(M_PI * fc_hp / m_sampleRate);
    double w2_hp = w_hp * w_hp;
    double k_hp = 1.0 / (1.0 + M_SQRT2 * w_hp + w2_hp);

    double a0_hp = k_hp;
    double a1_hp = -2.0 * k_hp;
    double a2_hp = k_hp;
    double b1_hp = 2.0 * (w2_hp - 1.0) * k_hp;
    double b2_hp = (1.0 - M_SQRT2 * w_hp + w2_hp) * k_hp;

    m_hpX[2] = m_hpX[1]; m_hpX[1] = m_hpX[0]; m_hpX[0] = lpOut;
    m_hpY[2] = m_hpY[1]; m_hpY[1] = m_hpY[0];
    m_hpY[0] = a0_hp * m_hpX[0] + a1_hp * m_hpX[1] + a2_hp * m_hpX[2]
               - b1_hp * m_hpY[1] - b2_hp * m_hpY[2];

    return m_hpY[0];
}

double RPeakDetector::derivative(double x)
{
    // 五点微分: y[n] = (1/8T)(-x[n-2] - 2x[n-1] + 2x[n+1] + x[n+2])
    // 因为实时处理, 使用因果版本: y[n] = (2x[n] + x[n-1] - x[n-3] - 2x[n-4]) / 8
    m_diffBuf.push_back(x);
    if (m_diffBuf.size() < 5) return 0.0;
    if (m_diffBuf.size() > 5) m_diffBuf.pop_front();

    double y = (2.0 * m_diffBuf[4] + m_diffBuf[3] - m_diffBuf[1] - 2.0 * m_diffBuf[0]) / 8.0;
    return y;
}

double RPeakDetector::squaring(double x)
{
    return x * x;
}

double RPeakDetector::movingWindowIntegration(double x)
{
    m_intBuf.push_back(x);
    m_intSum += x;

    while (static_cast<int>(m_intBuf.size()) > m_windowSize) {
        m_intSum -= m_intBuf.front();
        m_intBuf.pop_front();
    }

    return m_intSum / m_windowSize;
}

void RPeakDetector::detectPeak(double integratedValue, double originalValue)
{
    // 初始学习阶段: 前2秒只收集统计数据
    if (m_globalIndex < m_sampleRate * 2) {
        // 初始化阈值
        if (integratedValue > m_signalLevel) {
            m_signalLevel = integratedValue;
        }
        m_threshold = m_signalLevel * 0.25;
        return;
    }

    // 跟踪上升/下降沿
    if (integratedValue > m_candidateMax) {
        m_candidateMax = integratedValue;
        m_candidateIndex = m_globalIndex;
        m_rising = true;
    }

    // 检测下降沿 (积分值从峰值下降超过40%)
    bool pastPeak = m_rising && (integratedValue < m_candidateMax * 0.6);

    if (pastPeak && m_candidateMax > m_threshold) {
        // 检查不应期
        if (m_lastPeakIndex < 0 ||
            (m_candidateIndex - m_lastPeakIndex) >= m_refractorySamples) {

            // 在候选点附近的原始信号中找真正的R波峰值
            int searchStart = m_candidateIndex - m_windowSize;
            int searchEnd = m_candidateIndex + 2;
            double maxOriginal = -1e9;
            int maxOriginalIdx = m_candidateIndex;

            for (int i = searchStart; i <= searchEnd; ++i) {
                int bufIdx = i - m_originalBufStart;
                if (bufIdx >= 0 && bufIdx < static_cast<int>(m_originalBuf.size())) {
                    if (m_originalBuf[bufIdx] > maxOriginal) {
                        maxOriginal = m_originalBuf[bufIdx];
                        maxOriginalIdx = i;
                    }
                }
            }

            // 构建R波信息
            RPeakInfo peak;
            peak.sampleIndex = maxOriginalIdx;
            peak.amplitude = maxOriginal;
            peak.timestamp = static_cast<double>(maxOriginalIdx) / m_sampleRate;

            if (!m_peaks.isEmpty()) {
                const RPeakInfo& prev = m_peaks.last();
                peak.rrInterval = peak.timestamp - prev.timestamp;
                if (peak.rrInterval > 0.0) {
                    peak.instantHR = 60.0 / peak.rrInterval;
                }
            } else {
                peak.rrInterval = 0.0;
                peak.instantHR = 0.0;
            }

            m_peaks.append(peak);
            m_lastPeakIndex = maxOriginalIdx;

            updateThreshold(m_candidateMax, true);
            updateHeartRate();

            emit rPeakDetected(peak);
        } else {
            // 不应期内, 视为噪声
            updateThreshold(m_candidateMax, false);
        }

        // 重置候选
        m_candidateMax = 0.0;
        m_candidateIndex = -1;
        m_rising = false;
    }

    // 如果长时间没有检测到峰值 (>1.66s, 即<36bpm), 降低阈值
    if (m_lastPeakIndex >= 0 &&
        (m_globalIndex - m_lastPeakIndex) > static_cast<int>(1.66 * m_sampleRate)) {
        m_threshold *= 0.5;
        // 重置上升沿跟踪以重新寻找
        m_candidateMax = 0.0;
        m_rising = false;
    }
}

void RPeakDetector::updateThreshold(double peakValue, bool isSignal)
{
    if (isSignal) {
        // 信号峰: signalLevel = 0.125 * peak + 0.875 * signalLevel
        m_signalLevel = 0.125 * peakValue + 0.875 * m_signalLevel;
    } else {
        // 噪声峰: noiseLevel = 0.125 * peak + 0.875 * noiseLevel
        m_noiseLevel = 0.125 * peakValue + 0.875 * m_noiseLevel;
    }
    // 阈值 = noiseLevel + 0.25 * (signalLevel - noiseLevel)
    m_threshold = m_noiseLevel + 0.25 * (m_signalLevel - m_noiseLevel);
}

void RPeakDetector::updateHeartRate()
{
    // 用最近8个R-R间隔计算平均心率
    int count = m_peaks.size();
    if (count < 2) return;

    int n = qMin(8, count - 1);
    double sumRR = 0.0;
    for (int i = count - n; i < count; ++i) {
        sumRR += m_peaks[i].rrInterval;
    }

    double avgRR = sumRR / n;
    if (avgRR > 0.0) {
        int hr = qRound(60.0 / avgRR);
        // 合理范围 30-220 bpm
        if (hr >= 30 && hr <= 220) {
            m_currentHR = hr;
            emit heartRateUpdated(m_currentHR);
        }
    }
}

double RPeakDetector::lastRRInterval() const
{
    if (m_peaks.size() < 2) return 0.0;
    return m_peaks.last().rrInterval;
}

RPeakDetector::AnalysisReport RPeakDetector::generateReport() const
{
    AnalysisReport report;
    report.totalPeaks = m_peaks.size();

    if (m_peaks.size() < 2) {
        report.findings.append(QStringLiteral("数据不足，无法进行有效分析（至少需要2个R波）"));
        return report;
    }

    report.durationSeconds = m_peaks.last().timestamp - m_peaks.first().timestamp;

    // ---- 收集有效的R-R间期和瞬时心率 ----
    QVector<double> rrIntervals;  // 秒
    QVector<double> heartRates;   // bpm
    QVector<double> amplitudes;

    for (int i = 0; i < m_peaks.size(); ++i) {
        amplitudes.append(m_peaks[i].amplitude);
        if (i > 0 && m_peaks[i].rrInterval > 0.0) {
            rrIntervals.append(m_peaks[i].rrInterval);
            heartRates.append(m_peaks[i].instantHR);
        }
    }

    if (rrIntervals.isEmpty()) return report;

    // ---- 心率统计 ----
    double sumHR = std::accumulate(heartRates.begin(), heartRates.end(), 0.0);
    report.avgHR = sumHR / heartRates.size();
    report.minHR = *std::min_element(heartRates.begin(), heartRates.end());
    report.maxHR = *std::max_element(heartRates.begin(), heartRates.end());

    double sqSumHR = 0.0;
    for (double hr : heartRates) sqSumHR += (hr - report.avgHR) * (hr - report.avgHR);
    report.stdHR = qSqrt(sqSumHR / heartRates.size());

    // ---- R-R间期统计 (转ms) ----
    QVector<double> rrMs;
    for (double rr : rrIntervals) rrMs.append(rr * 1000.0);

    double sumRR = std::accumulate(rrMs.begin(), rrMs.end(), 0.0);
    report.avgRR = sumRR / rrMs.size();
    report.minRR = *std::min_element(rrMs.begin(), rrMs.end());
    report.maxRR = *std::max_element(rrMs.begin(), rrMs.end());

    double sqSumRR = 0.0;
    for (double rr : rrMs) sqSumRR += (rr - report.avgRR) * (rr - report.avgRR);
    report.stdRR = qSqrt(sqSumRR / rrMs.size());

    // ---- HRV 指标 ----
    // SDNN: R-R间期标准差
    report.sdnn = report.stdRR;

    // RMSSD: 相邻R-R间期差值的均方根
    double sumDiffSq = 0.0;
    int nn50Count = 0;
    for (int i = 1; i < rrMs.size(); ++i) {
        double diff = rrMs[i] - rrMs[i - 1];
        sumDiffSq += diff * diff;
        if (qAbs(diff) > 50.0) nn50Count++;
    }
    if (rrMs.size() > 1) {
        report.rmssd = qSqrt(sumDiffSq / (rrMs.size() - 1));
        report.pnn50 = 100.0 * nn50Count / (rrMs.size() - 1);
    }

    // ---- R波幅值统计 ----
    double sumAmp = std::accumulate(amplitudes.begin(), amplitudes.end(), 0.0);
    report.avgAmplitude = sumAmp / amplitudes.size();
    report.minAmplitude = *std::min_element(amplitudes.begin(), amplitudes.end());
    report.maxAmplitude = *std::max_element(amplitudes.begin(), amplitudes.end());

    // ============================================================
    // 医学评估
    // ============================================================

    // -- 心率评估 --
    if (report.avgHR < 60.0) {
        report.findings.append(QStringLiteral("窦性心动过缓: 平均心率 %1 bpm (< 60 bpm)")
                                   .arg(report.avgHR, 0, 'f', 1));
        if (report.avgHR < 50.0) {
            report.suggestions.append(QStringLiteral("心率显著偏低，建议进一步检查是否存在房室传导阻滞"));
        } else {
            report.suggestions.append(QStringLiteral("轻度心动过缓，运动员或睡眠状态可能属正常，如有头晕等症状建议就医"));
        }
    } else if (report.avgHR > 100.0) {
        report.findings.append(QStringLiteral("窦性心动过速: 平均心率 %1 bpm (> 100 bpm)")
                                   .arg(report.avgHR, 0, 'f', 1));
        if (report.avgHR > 150.0) {
            report.suggestions.append(QStringLiteral("心率过快，建议立即就医排除室上性心动过速等病因"));
        } else {
            report.suggestions.append(QStringLiteral("心率偏快，可能与运动、情绪、发热等有关，持续出现建议就医"));
        }
    } else {
        report.findings.append(QStringLiteral("心率正常: 平均 %1 bpm (60-100 bpm)")
                                   .arg(report.avgHR, 0, 'f', 1));
    }

    // -- 心率变异性评估 --
    double rrVariation = (report.maxRR - report.minRR) / report.avgRR * 100.0;
    if (rrVariation > 20.0) {
        report.findings.append(QStringLiteral("R-R间期变异较大 (%.1f%%), 可能存在心律不齐")
                                   .arg(rrVariation));
        report.suggestions.append(QStringLiteral("建议进行24小时动态心电图(Holter)检查以明确心律失常类型"));
    }

    // -- SDNN 评估 --
    if (report.sdnn < 50.0 && rrIntervals.size() >= 10) {
        report.findings.append(QStringLiteral("HRV偏低: SDNN = %.1f ms (< 50 ms)").arg(report.sdnn));
        report.suggestions.append(QStringLiteral("心率变异性偏低可能与自主神经功能下降有关，建议关注心血管健康"));
    } else if (report.sdnn > 50.0 && report.sdnn < 100.0 && rrIntervals.size() >= 10) {
        report.findings.append(QStringLiteral("HRV正常: SDNN = %.1f ms").arg(report.sdnn));
    } else if (report.sdnn >= 100.0 && rrIntervals.size() >= 10) {
        report.findings.append(QStringLiteral("HRV良好: SDNN = %.1f ms").arg(report.sdnn));
    }

    // -- R波幅值评估 --
    double ampVariation = (report.maxAmplitude - report.minAmplitude);
    if (report.avgAmplitude > 0 && ampVariation / report.avgAmplitude > 0.5) {
        report.findings.append(QStringLiteral("R波幅值变异较大 (%.0f - %.0f mV), 注意电极接触")
                                   .arg(report.minAmplitude).arg(report.maxAmplitude));
    }

    // -- 早搏检测 (R-R间期突然缩短>20%) --
    int prematureCount = 0;
    for (int i = 2; i < rrIntervals.size(); ++i) {
        double prevAvg = (rrIntervals[i - 1] + rrIntervals[i - 2]) / 2.0;
        if (prevAvg > 0 && rrIntervals[i] < prevAvg * 0.80) {
            prematureCount++;
        }
    }
    if (prematureCount > 0) {
        report.findings.append(QStringLiteral("检测到 %1 次疑似早搏 (R-R间期突然缩短>20%)")
                                   .arg(prematureCount));
        if (prematureCount > 5) {
            report.suggestions.append(QStringLiteral("频发早搏，建议心内科进一步评估"));
        } else {
            report.suggestions.append(QStringLiteral("偶发早搏，多数情况下无需特殊处理，如有症状建议就医"));
        }
    }

    // -- 总结建议 --
    if (report.suggestions.isEmpty()) {
        report.suggestions.append(QStringLiteral("各项指标在正常范围内，请继续保持健康的生活方式"));
    }

    return report;
}
