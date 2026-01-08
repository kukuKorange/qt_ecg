#include "historydialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSplitter>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QTabWidget>

HistoryDialog::HistoryDialog(DataManager* dataManager, QWidget* parent)
    : QDialog(parent)
    , m_dataManager(dataManager)
{
    setWindowTitle(QStringLiteral("历史数据查询"));
    setMinimumSize(1200, 800);
    setupUI();
    
    // 加载最近一小时的数据
    m_timeRangeCombo->setCurrentIndex(2);  // 1小时
    onTimeRangeChanged(2);
}

HistoryDialog::~HistoryDialog()
{
}

void HistoryDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    
    // 设置深色样式
    setStyleSheet(R"(
        QDialog {
            background-color: #0a1628;
            color: white;
        }
        QGroupBox {
            border: 1px solid #2a4a6a;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 15px;
            font-weight: bold;
            color: #4ecdc4;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 5px;
        }
        QLabel {
            color: #b0c4de;
        }
        QPushButton {
            background-color: #1a3a5c;
            border: 1px solid #2a5a8c;
            border-radius: 5px;
            padding: 8px 20px;
            color: white;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #2a5a8c;
        }
        QPushButton:pressed {
            background-color: #3a6a9c;
        }
        QDateTimeEdit, QComboBox {
            background-color: #1a3a5c;
            border: 1px solid #2a5a8c;
            border-radius: 5px;
            padding: 5px;
            color: white;
        }
        QTableWidget {
            background-color: #0d1f35;
            border: 1px solid #2a4a6a;
            border-radius: 5px;
            gridline-color: #2a4a6a;
            color: white;
        }
        QTableWidget::item {
            padding: 5px;
        }
        QTableWidget::item:selected {
            background-color: #2a5a8c;
        }
        QHeaderView::section {
            background-color: #1a3a5c;
            color: white;
            padding: 8px;
            border: none;
            border-bottom: 1px solid #2a4a6a;
            font-weight: bold;
        }
        QTabWidget::pane {
            border: 1px solid #2a4a6a;
            border-radius: 5px;
            background-color: #0a1628;
        }
        QTabBar::tab {
            background-color: #1a3a5c;
            color: white;
            padding: 10px 20px;
            margin-right: 2px;
            border-top-left-radius: 5px;
            border-top-right-radius: 5px;
        }
        QTabBar::tab:selected {
            background-color: #2a5a8c;
        }
    )");
    
    // ===== 查询区域 =====
    QGroupBox* queryGroup = new QGroupBox(QStringLiteral("查询条件"));
    QHBoxLayout* queryLayout = new QHBoxLayout(queryGroup);
    
    m_timeRangeCombo = new QComboBox();
    m_timeRangeCombo->addItem(QStringLiteral("最近10分钟"), 10);
    m_timeRangeCombo->addItem(QStringLiteral("最近30分钟"), 30);
    m_timeRangeCombo->addItem(QStringLiteral("最近1小时"), 60);
    m_timeRangeCombo->addItem(QStringLiteral("最近6小时"), 360);
    m_timeRangeCombo->addItem(QStringLiteral("最近24小时"), 1440);
    m_timeRangeCombo->addItem(QStringLiteral("最近7天"), 10080);
    m_timeRangeCombo->addItem(QStringLiteral("自定义"), -1);
    
    queryLayout->addWidget(new QLabel(QStringLiteral("时间范围:")));
    queryLayout->addWidget(m_timeRangeCombo);
    
    m_startDateTime = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(-3600));
    m_startDateTime->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_startDateTime->setCalendarPopup(true);
    
    m_endDateTime = new QDateTimeEdit(QDateTime::currentDateTime());
    m_endDateTime->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_endDateTime->setCalendarPopup(true);
    
    queryLayout->addWidget(new QLabel(QStringLiteral("开始:")));
    queryLayout->addWidget(m_startDateTime);
    queryLayout->addWidget(new QLabel(QStringLiteral("结束:")));
    queryLayout->addWidget(m_endDateTime);
    
    m_queryButton = new QPushButton(QStringLiteral("🔍 查询"));
    m_queryButton->setFixedWidth(100);
    queryLayout->addWidget(m_queryButton);
    
    queryLayout->addStretch();
    
    mainLayout->addWidget(queryGroup);
    
    // ===== 内容区域 =====
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    
    // 左侧：数据表格和统计
    QWidget* leftWidget = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    // 统计信息
    QGroupBox* statsGroup = new QGroupBox(QStringLiteral("统计信息"));
    QHBoxLayout* statsLayout = new QHBoxLayout(statsGroup);
    
    m_recordCountLabel = new QLabel(QStringLiteral("记录数: 0"));
    m_avgTempLabel = new QLabel(QStringLiteral("平均体温: --°C"));
    m_avgHrLabel = new QLabel(QStringLiteral("平均心率: -- bpm"));
    m_avgSpo2Label = new QLabel(QStringLiteral("平均血氧: --%"));
    
    statsLayout->addWidget(m_recordCountLabel);
    statsLayout->addWidget(m_avgTempLabel);
    statsLayout->addWidget(m_avgHrLabel);
    statsLayout->addWidget(m_avgSpo2Label);
    statsLayout->addStretch();
    
    leftLayout->addWidget(statsGroup);
    
    // 数据表格
    m_dataTable = new QTableWidget();
    m_dataTable->setColumnCount(5);
    m_dataTable->setHorizontalHeaderLabels({
        QStringLiteral("时间"),
        QStringLiteral("体温 (°C)"),
        QStringLiteral("心率 (bpm)"),
        QStringLiteral("血氧 (%)"),
        QStringLiteral("心电数据")
    });
    m_dataTable->horizontalHeader()->setStretchLastSection(true);
    m_dataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dataTable->setAlternatingRowColors(true);
    
    leftLayout->addWidget(m_dataTable);
    
    // 操作按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_exportCsvButton = new QPushButton(QStringLiteral("📊 导出CSV"));
    m_exportJsonButton = new QPushButton(QStringLiteral("📄 导出JSON"));
    m_playbackButton = new QPushButton(QStringLiteral("▶️ 回放心电"));
    m_playbackButton->setEnabled(false);
    
    buttonLayout->addWidget(m_exportCsvButton);
    buttonLayout->addWidget(m_exportJsonButton);
    buttonLayout->addWidget(m_playbackButton);
    buttonLayout->addStretch();
    
    leftLayout->addLayout(buttonLayout);
    
    splitter->addWidget(leftWidget);
    
    // 右侧：图表
    QTabWidget* chartTabs = new QTabWidget();
    
    m_chartWidget = new VitalsChartWidget(VitalsChartWidget::Combined);
    m_ecgWidget = new EcgChartWidget();
    
    chartTabs->addTab(m_chartWidget, QStringLiteral("趋势图"));
    chartTabs->addTab(m_ecgWidget, QStringLiteral("心电图回放"));
    
    splitter->addWidget(chartTabs);
    splitter->setSizes({500, 700});
    
    mainLayout->addWidget(splitter, 1);
    
    // 连接信号
    connect(m_timeRangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HistoryDialog::onTimeRangeChanged);
    connect(m_queryButton, &QPushButton::clicked, this, &HistoryDialog::onQueryClicked);
    connect(m_exportCsvButton, &QPushButton::clicked, this, &HistoryDialog::onExportCsvClicked);
    connect(m_exportJsonButton, &QPushButton::clicked, this, &HistoryDialog::onExportJsonClicked);
    connect(m_playbackButton, &QPushButton::clicked, this, &HistoryDialog::onPlaybackClicked);
    connect(m_dataTable, &QTableWidget::itemSelectionChanged,
            this, &HistoryDialog::onTableSelectionChanged);
}

void HistoryDialog::onTimeRangeChanged(int index)
{
    int minutes = m_timeRangeCombo->currentData().toInt();
    
    if (minutes > 0) {
        m_startDateTime->setDateTime(QDateTime::currentDateTime().addSecs(-minutes * 60));
        m_endDateTime->setDateTime(QDateTime::currentDateTime());
        m_startDateTime->setEnabled(false);
        m_endDateTime->setEnabled(false);
    } else {
        m_startDateTime->setEnabled(true);
        m_endDateTime->setEnabled(true);
    }
    
    onQueryClicked();
}

void HistoryDialog::onQueryClicked()
{
    loadData();
}

void HistoryDialog::loadData()
{
    QDateTime start = m_startDateTime->dateTime();
    QDateTime end = m_endDateTime->dateTime();
    
    m_currentData = m_dataManager->getVitalDataRange(start, end);
    populateTable(m_currentData);
    updateStatistics(m_currentData);
    
    // 更新图表
    QVector<QPair<QDateTime, double>> tempData;
    QVector<QPair<QDateTime, int>> hrData;
    QVector<QPair<QDateTime, int>> spo2Data;
    
    for (const VitalData& data : m_currentData) {
        if (data.temperature > 0) {
            tempData.append(qMakePair(data.timestamp, data.temperature));
        }
        if (data.heartRate > 0) {
            hrData.append(qMakePair(data.timestamp, data.heartRate));
        }
        if (data.bloodOxygen > 0) {
            spo2Data.append(qMakePair(data.timestamp, data.bloodOxygen));
        }
    }
    
    m_chartWidget->setData(tempData, hrData, spo2Data);
}

void HistoryDialog::populateTable(const QVector<VitalData>& data)
{
    m_dataTable->setRowCount(0);
    m_dataTable->setRowCount(data.size());
    
    for (int i = 0; i < data.size(); ++i) {
        const VitalData& d = data[i];
        
        m_dataTable->setItem(i, 0, new QTableWidgetItem(
            d.timestamp.toString("yyyy-MM-dd HH:mm:ss")));
        
        m_dataTable->setItem(i, 1, new QTableWidgetItem(
            d.temperature > 0 ? QString::number(d.temperature, 'f', 1) : "--"));
        
        m_dataTable->setItem(i, 2, new QTableWidgetItem(
            d.heartRate > 0 ? QString::number(d.heartRate) : "--"));
        
        m_dataTable->setItem(i, 3, new QTableWidgetItem(
            d.bloodOxygen > 0 ? QString::number(d.bloodOxygen) : "--"));
        
        m_dataTable->setItem(i, 4, new QTableWidgetItem(
            d.ecgData.isEmpty() ? QStringLiteral("无") : 
            QString("%1 点").arg(d.ecgData.size())));
    }
    
    m_dataTable->resizeColumnsToContents();
}

void HistoryDialog::updateStatistics(const QVector<VitalData>& data)
{
    m_recordCountLabel->setText(QStringLiteral("记录数: %1").arg(data.size()));
    
    double sumTemp = 0, sumHr = 0, sumSpo2 = 0;
    int countTemp = 0, countHr = 0, countSpo2 = 0;
    
    for (const VitalData& d : data) {
        if (d.temperature > 0) { sumTemp += d.temperature; countTemp++; }
        if (d.heartRate > 0) { sumHr += d.heartRate; countHr++; }
        if (d.bloodOxygen > 0) { sumSpo2 += d.bloodOxygen; countSpo2++; }
    }
    
    m_avgTempLabel->setText(countTemp > 0 ? 
        QStringLiteral("平均体温: %1°C").arg(sumTemp / countTemp, 0, 'f', 1) :
        QStringLiteral("平均体温: --°C"));
    
    m_avgHrLabel->setText(countHr > 0 ? 
        QStringLiteral("平均心率: %1 bpm").arg(qRound(sumHr / countHr)) :
        QStringLiteral("平均心率: -- bpm"));
    
    m_avgSpo2Label->setText(countSpo2 > 0 ? 
        QStringLiteral("平均血氧: %1%").arg(qRound(sumSpo2 / countSpo2)) :
        QStringLiteral("平均血氧: --%"));
}

void HistoryDialog::onTableSelectionChanged()
{
    QList<QTableWidgetItem*> selected = m_dataTable->selectedItems();
    if (selected.isEmpty()) {
        m_playbackButton->setEnabled(false);
        return;
    }
    
    int row = selected.first()->row();
    if (row >= 0 && row < m_currentData.size()) {
        const VitalData& data = m_currentData[row];
        m_playbackButton->setEnabled(!data.ecgData.isEmpty());
    }
}

void HistoryDialog::onPlaybackClicked()
{
    QList<QTableWidgetItem*> selected = m_dataTable->selectedItems();
    if (selected.isEmpty()) return;
    
    int row = selected.first()->row();
    if (row >= 0 && row < m_currentData.size()) {
        const VitalData& data = m_currentData[row];
        if (!data.ecgData.isEmpty()) {
            m_ecgWidget->startPlayback(data.ecgData);
        }
    }
}

void HistoryDialog::onExportCsvClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, 
        QStringLiteral("导出CSV文件"), 
        QString("health_data_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        "CSV Files (*.csv)");
    
    if (fileName.isEmpty()) return;
    
    if (m_dataManager->exportToCsv(fileName, 
            m_startDateTime->dateTime(), 
            m_endDateTime->dateTime())) {
        QMessageBox::information(this, QStringLiteral("导出成功"),
            QStringLiteral("数据已成功导出到:\n%1").arg(fileName));
    } else {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
            QStringLiteral("无法导出数据到指定文件。"));
    }
}

void HistoryDialog::onExportJsonClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, 
        QStringLiteral("导出JSON文件"), 
        QString("health_data_%1.json").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        "JSON Files (*.json)");
    
    if (fileName.isEmpty()) return;
    
    if (m_dataManager->exportToJson(fileName, 
            m_startDateTime->dateTime(), 
            m_endDateTime->dateTime())) {
        QMessageBox::information(this, QStringLiteral("导出成功"),
            QStringLiteral("数据已成功导出到:\n%1").arg(fileName));
    } else {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
            QStringLiteral("无法导出数据到指定文件。"));
    }
}
