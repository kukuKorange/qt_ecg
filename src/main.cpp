#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QStyleFactory>

#ifdef Q_OS_WIN
#pragma comment(lib, "user32.lib")
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName(QStringLiteral("智能健康监护系统"));
    app.setOrganizationName("HealthMonitor");
    app.setOrganizationDomain("healthmonitor.example.com");
    app.setApplicationVersion("1.0.0");
    
    // 设置Fusion风格以获得更好的跨平台一致性
    app.setStyle(QStyleFactory::create("Fusion"));
    
    // 创建并显示主窗口
    MainWindow window;
    window.show();
    
    return app.exec();
}
