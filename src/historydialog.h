#pragma once
#include <QDialog>
#include <QDateTimeEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include "vitalschartwidget.h"
#include "ecgchartwidget.h"
#include "datamanager.h"

class HistoryDialog : public QDialog {
    Q_OBJECT

public:
    explicit HistoryDialog(DataManager* dataManager, QWidget* parent = nullptr);
    ~HistoryDialog();

private slots:
    void onQueryClicked();
    void onExportCsvClicked();
    void onExportJsonClicked();
    void onTableSelectionChanged();
    void onPlaybackClicked();
    void onTimeRangeChanged(int index);

private:
    void setupUI();
    void loadData();
    void populateTable(const QVector<VitalData>& data);
    void updateStatistics(const QVector<VitalData>& data);
    
    DataManager* m_dataManager;
    
    // 时间范围选择
    QComboBox* m_timeRangeCombo;
    QDateTimeEdit* m_startDateTime;
    QDateTimeEdit* m_endDateTime;
    QPushButton* m_queryButton;
    
    // 数据表格
    QTableWidget* m_dataTable;
    
    // 图表
    VitalsChartWidget* m_chartWidget;
    EcgChartWidget* m_ecgWidget;
    
    // 统计信息
    QLabel* m_avgTempLabel;
    QLabel* m_avgHrLabel;
    QLabel* m_avgSpo2Label;
    QLabel* m_recordCountLabel;
    
    // 导出和回放按钮
    QPushButton* m_exportCsvButton;
    QPushButton* m_exportJsonButton;
    QPushButton* m_playbackButton;
    
    QVector<VitalData> m_currentData;
};
