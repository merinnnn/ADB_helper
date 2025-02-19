#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QTimer>
#include <QMediaPlayer>
#include <QVideoWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    // Override eventFilter to capture key presses in the Keyboard tab.
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    // Logcat slots
    void runLogcat();
    void stopLogcat();
    // Screencap slot
    void runScreencap();
    // Recording slots
    void startRecording();
    void stopRecording();
    // Sideload slot
    void runSideload();
    // Info slot
    void runInfo();
    // Shell slot
    void runShellCommand();
    // Buffered log update slot
    void updateLogOutput();

private:
    void startPythonScript();
    void trimLogcatOutput();
    void appendLog(const QString &text);
    int mapQtKeyToAndroidKey(int qtKey);  // Mapping function for Keyboard tab

    Ui::MainWindow *ui;
    QProcess *pythonProcess;
    QProcess *logcatProcess;
    QProcess *recordingProcess; // Process for screen recording
    QString m_logBuffer;
    QTimer *m_updateTimer;
    QMediaPlayer *m_mediaPlayer;  // For playing recording video
};

#endif // MAINWINDOW_H
