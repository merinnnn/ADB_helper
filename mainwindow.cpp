#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QProcess>
#include <QDebug>
#include <QRegularExpression>
#include <QScrollBar>
#include <QDir>
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QPixmap>
#include <QTimer>
#include <QStandardPaths>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QUrl>
#include <QKeyEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , pythonProcess(nullptr)
    , logcatProcess(nullptr)
    , recordingProcess(nullptr)
    , m_updateTimer(new QTimer(this))
    , m_mediaPlayer(new QMediaPlayer(this))
{
    ui->setupUi(this);

    // Set the video output for playing recordings.
    m_mediaPlayer->setVideoOutput(ui->recordingVideoWidget);

    // Connect Logcat buttons
    connect(ui->runLogcatButton, &QPushButton::clicked, this, &MainWindow::runLogcat);
    connect(ui->stopLogcatButton, &QPushButton::clicked, this, &MainWindow::stopLogcat);
    // Connect Screencap button
    connect(ui->runScreencapButton, &QPushButton::clicked, this, &MainWindow::runScreencap);
    // Connect Recording buttons
    connect(ui->startRecordingButton, &QPushButton::clicked, this, &MainWindow::startRecording);
    connect(ui->stopRecordingButton, &QPushButton::clicked, this, &MainWindow::stopRecording);
    // Connect Sideload button
    connect(ui->runSideloadButton, &QPushButton::clicked, this, &MainWindow::runSideload);
    // Connect Info button
    connect(ui->getInfoButton, &QPushButton::clicked, this, &MainWindow::runInfo);
    // Connect Shell tab: when the user presses Enter in the command field, run the command.
    connect(ui->shellCommandLineEdit, &QLineEdit::returnPressed, this, &MainWindow::runShellCommand);

    // Install event filter on the keyboard input field.
    ui->keyboardLineEdit->installEventFilter(this);

    // Timer for buffered log updates.
    m_updateTimer->setInterval(100);
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updateLogOutput);
    m_updateTimer->start();

    // Start a Python script if needed.
    startPythonScript();
}

MainWindow::~MainWindow()
{
    if (pythonProcess) {
        if (pythonProcess->state() == QProcess::Running) {
            pythonProcess->terminate();
            pythonProcess->waitForFinished(3000);
        }
        delete pythonProcess;
    }
    if (logcatProcess) {
        if (logcatProcess->state() == QProcess::Running) {
            logcatProcess->terminate();
            logcatProcess->waitForFinished(3000);
        }
        delete logcatProcess;
    }
    if (recordingProcess) {
        if (recordingProcess->state() == QProcess::Running) {
            recordingProcess->terminate();
            recordingProcess->waitForFinished(3000);
        }
        delete recordingProcess;
    }
    delete ui;
}

//--------------------------
// Shell Tab Functionality
//--------------------------
void MainWindow::runShellCommand()
{
    QString cmd = ui->shellCommandLineEdit->text().trimmed();
    if(cmd.isEmpty())
        return;

    // Append the prompt and command to the shell output.
    QString prompt = "android@device:~$ ";
    ui->shellOutput->appendPlainText(prompt + cmd);
    ui->shellCommandLineEdit->clear();

    // Use QProcess::splitCommand() to split the command properly.
    QStringList cmdArgs = QProcess::splitCommand(cmd);
    // Prepend "shell" as the first argument so the full command becomes:
    // adb shell <command and its arguments>
    cmdArgs.prepend("shell");

    QProcess *shellProcess = new QProcess(this);
    shellProcess->setProgram("adb");
    shellProcess->setArguments(cmdArgs);

    connect(shellProcess, &QProcess::readyReadStandardOutput, [this, shellProcess]() {
        QByteArray output = shellProcess->readAllStandardOutput();
        ui->shellOutput->appendPlainText(QString::fromLocal8Bit(output));
    });
    connect(shellProcess, &QProcess::readyReadStandardError, [this, shellProcess]() {
        QByteArray errOutput = shellProcess->readAllStandardError();
        ui->shellOutput->appendPlainText(QString::fromLocal8Bit(errOutput));
    });
    connect(shellProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            shellProcess, &QProcess::deleteLater);
    shellProcess->start();
    if (!shellProcess->waitForStarted(3000)) {
        ui->shellOutput->appendPlainText("Failed to start adb shell command.");
    }
}

//--------------------------
// Event Filter for Keyboard Tab
//--------------------------
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->keyboardLineEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        int qtKey = keyEvent->key();
        int androidKey = mapQtKeyToAndroidKey(qtKey);
        if (androidKey != -1) {
            QProcess *process = new QProcess(this);
            process->setProgram("adb");
            process->setArguments(QStringList() << "shell" << "input" << "keyevent" << QString::number(androidKey));
            connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    process, &QProcess::deleteLater);
            process->start();
        }
        return true;
    }
    return QMainWindow::eventFilter(obj, event);
}

// Mapping function: maps some Qt keys to Android key codes.
int MainWindow::mapQtKeyToAndroidKey(int qtKey)
{
    // Map A-Z: Android KEYCODE_A = 29, B = 30, ..., Z = 54.
    if(qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        return (qtKey - Qt::Key_A) + 29;
    // Map 0-9: Android KEYCODE_0 = 7, KEYCODE_1 = 8, ..., KEYCODE_9 = 16.
    if(qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        return (qtKey - Qt::Key_0) + 7;
    // Map space.
    if(qtKey == Qt::Key_Space)
        return 62;
    // Map Enter/Return.
    if(qtKey == Qt::Key_Return || qtKey == Qt::Key_Enter)
        return 66;
    // For unsupported keys, return -1.
    return -1;
}

//--------------------------
// Logcat related functions
//--------------------------
void MainWindow::trimLogcatOutput()
{
    QString text = ui->logcatOutput->toPlainText();
    QStringList lines = text.split('\n');
    if (lines.size() > 20000) {
        lines = lines.mid(lines.size() - 20000);
        ui->logcatOutput->setPlainText(lines.join("\n"));
    }
}

void MainWindow::appendLog(const QString &text)
{
    m_logBuffer.append(text);
}

void MainWindow::updateLogOutput()
{
    if (m_logBuffer.isEmpty())
        return;
    QScrollBar *scrollBar = ui->logcatOutput->verticalScrollBar();
    bool atBottom = (scrollBar->value() == scrollBar->maximum());
    ui->logcatOutput->moveCursor(QTextCursor::End);
    ui->logcatOutput->insertPlainText(m_logBuffer);
    m_logBuffer.clear();
    trimLogcatOutput();
    if (atBottom)
        scrollBar->setValue(scrollBar->maximum());
}

void MainWindow::runLogcat()
{
    ui->logcatOutput->clear();
    ui->runLogcatButton->setEnabled(false);
    ui->logcatArgsLineEdit->setEnabled(false);

    if (logcatProcess) {
        if (logcatProcess->state() == QProcess::Running) {
            logcatProcess->terminate();
            logcatProcess->waitForFinished(3000);
        }
        delete logcatProcess;
        logcatProcess = nullptr;
    }

    logcatProcess = new QProcess(this);
    logcatProcess->setProgram("adb");

    QStringList arguments;
    arguments << "logcat";
    QString extraArgs = ui->logcatArgsLineEdit->text().trimmed();
    if (!extraArgs.isEmpty()) {
        QStringList extraArgsList = extraArgs.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        arguments.append(extraArgsList);
    }
    logcatProcess->setArguments(arguments);

    connect(logcatProcess, &QProcess::readyReadStandardOutput, [this]() {
        QByteArray output = logcatProcess->readAllStandardOutput();
        appendLog(QString::fromLocal8Bit(output));
    });

    connect(logcatProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this](int, QProcess::ExitStatus) {
                ui->runLogcatButton->setEnabled(true);
                ui->logcatArgsLineEdit->setEnabled(true);
            });

    logcatProcess->start();
    if (!logcatProcess->waitForStarted()) {
        appendLog("Failed to start adb logcat\n");
        ui->runLogcatButton->setEnabled(true);
        ui->logcatArgsLineEdit->setEnabled(true);
    }
}

void MainWindow::stopLogcat()
{
    if (logcatProcess && logcatProcess->state() == QProcess::Running) {
        logcatProcess->terminate();
        QTimer::singleShot(3000, this, [this]() {
            if (logcatProcess && logcatProcess->state() == QProcess::Running)
                logcatProcess->kill();
        });
        appendLog("Logcat stopped.\n");
    }
    ui->runLogcatButton->setEnabled(true);
    ui->logcatArgsLineEdit->setEnabled(true);
}

//--------------------------
// Screencap function
//--------------------------
void MainWindow::runScreencap()
{
    QProcess *adbScreencapProcess = new QProcess(this);
    adbScreencapProcess->setProgram("adb");
    // Use "shell" before "screencap": adb shell screencap -p /sdcard/screen.png
    adbScreencapProcess->setArguments(QStringList() << "shell" << "screencap" << "-p" << "/sdcard/screen.png");

    connect(adbScreencapProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, adbScreencapProcess](int exitCode, QProcess::ExitStatus status) {
                if (exitCode == 0 && status == QProcess::NormalExit) {
                    QString baseFolder = "screenshots";
                    QDir baseDir;
                    if (!baseDir.exists(baseFolder))
                        baseDir.mkpath(baseFolder);

                    QString dateFolder = QDate::currentDate().toString("yyyy-MM-dd");
                    QString fullDateFolderPath = baseFolder + "/" + dateFolder;
                    if (!baseDir.exists(fullDateFolderPath))
                        baseDir.mkpath(fullDateFolderPath);

                    QString fileName = QDateTime::currentDateTime().toString("HHmmsszzz") + ".png";
                    QString localPath = fullDateFolderPath + "/" + fileName;

                    QProcess *adbPullProcess = new QProcess(this);
                    adbPullProcess->setProgram("adb");
                    adbPullProcess->setArguments(QStringList() << "pull" << "/sdcard/screen.png" << localPath);

                    connect(adbPullProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                            this, [this, localPath, adbPullProcess](int pullExitCode, QProcess::ExitStatus pullStatus) {
                                if (pullExitCode == 0 && pullStatus == QProcess::NormalExit) {
                                    QPixmap pix(localPath);
                                    if (!pix.isNull()) {
                                        ui->screencapImageLabel->setPixmap(pix.scaled(ui->screencapImageLabel->size(),
                                                                                      Qt::KeepAspectRatio,
                                                                                      Qt::SmoothTransformation));
                                    } else {
                                        qDebug() << "Failed to load image from:" << localPath;
                                    }
                                } else {
                                    qDebug() << "adb pull failed:" << adbPullProcess->readAllStandardError();
                                }
                                adbPullProcess->deleteLater();
                            });
                    adbPullProcess->start();
                } else {
                    qDebug() << "adb screencap failed:" << adbScreencapProcess->readAllStandardError();
                }
                adbScreencapProcess->deleteLater();
            });

    adbScreencapProcess->start();
    if (!adbScreencapProcess->waitForStarted()) {
        qDebug() << "Failed to start adb screencap";
    }
}

//--------------------------
// Recording functions
//--------------------------
void MainWindow::startRecording()
{
    ui->recordingStatusLabel->setText("Recording in progress...");
    ui->startRecordingButton->setEnabled(false);
    ui->stopRecordingButton->setEnabled(true);

    if (recordingProcess) {
        if (recordingProcess->state() == QProcess::Running) {
            recordingProcess->terminate();
            QTimer::singleShot(3000, this, [this]() {
                if (recordingProcess && recordingProcess->state() == QProcess::Running)
                    recordingProcess->kill();
            });
        }
        delete recordingProcess;
        recordingProcess = nullptr;
    }

    recordingProcess = new QProcess(this);
    recordingProcess->setProgram("adb");
    // Start screen recording on device: adb shell screenrecord /sdcard/demo.mp4
    recordingProcess->setArguments(QStringList() << "shell" << "screenrecord" << "/sdcard/demo.mp4");

    recordingProcess->start();
    if (!recordingProcess->waitForStarted()) {
        ui->recordingStatusLabel->setText("Failed to start screen recording");
        ui->startRecordingButton->setEnabled(true);
        ui->stopRecordingButton->setEnabled(false);
    }
}

void MainWindow::stopRecording()
{
    if (recordingProcess && recordingProcess->state() == QProcess::Running) {
        recordingProcess->terminate();
        QTimer::singleShot(3000, this, [this]() {
            if (recordingProcess && recordingProcess->state() == QProcess::Running)
                recordingProcess->kill();
        });
    }

    // Save the recording file.
    QString baseFolder = "recordings";
    QDir baseDir;
    if (!baseDir.exists(baseFolder))
        baseDir.mkpath(baseFolder);

    QString dateFolder = QDate::currentDate().toString("yyyy-MM-dd");
    QString fullDateFolderPath = baseFolder + "/" + dateFolder;
    if (!baseDir.exists(fullDateFolderPath))
        baseDir.mkpath(fullDateFolderPath);

    QString fileName = QDateTime::currentDateTime().toString("HHmmsszzz") + ".mp4";
    QString localPath = fullDateFolderPath + "/" + fileName;

    QProcess *adbPullProcess = new QProcess(this);
    adbPullProcess->setProgram("adb");
    adbPullProcess->setArguments(QStringList() << "pull" << "/sdcard/demo.mp4" << localPath);

    connect(adbPullProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, localPath, adbPullProcess](int pullExitCode, QProcess::ExitStatus pullStatus) {
                if (pullExitCode == 0 && pullStatus == QProcess::NormalExit) {
                    ui->recordingStatusLabel->setText("Recording saved to: " + localPath);
                    // Load and play the video using QMediaPlayer.
                    m_mediaPlayer->setSource(QUrl::fromLocalFile(localPath));
                    m_mediaPlayer->play();
                } else {
                    ui->recordingStatusLabel->setText("Failed to pull recording");
                }
                adbPullProcess->deleteLater();
            });
    adbPullProcess->start();
    if (!adbPullProcess->waitForStarted()) {
        ui->recordingStatusLabel->setText("Failed to start adb pull for recording");
    }

    if (recordingProcess) {
        recordingProcess->deleteLater();
        recordingProcess = nullptr;
    }
    ui->startRecordingButton->setEnabled(true);
    ui->stopRecordingButton->setEnabled(false);
}

//--------------------------
// Sideload function
//--------------------------
void MainWindow::runSideload()
{
    QString apkPath = ui->apkPathLineEdit->text().trimmed();
    if (apkPath.isEmpty()) {
        ui->sideloadOutput->appendPlainText("Please specify the APK path.");
        return;
    }

    QProcess *adbInstallProcess = new QProcess(this);
    adbInstallProcess->setProgram("adb");
    QStringList arguments;
    arguments << "install" << apkPath;
    adbInstallProcess->setArguments(arguments);

    connect(adbInstallProcess, &QProcess::readyReadStandardOutput, [this, adbInstallProcess]() {
        QByteArray output = adbInstallProcess->readAllStandardOutput();
        ui->sideloadOutput->appendPlainText(QString::fromLocal8Bit(output));
    });
    connect(adbInstallProcess, &QProcess::readyReadStandardError, [this, adbInstallProcess]() {
        QByteArray errorOutput = adbInstallProcess->readAllStandardError();
        ui->sideloadOutput->appendPlainText(QString::fromLocal8Bit(errorOutput));
    });

    adbInstallProcess->start();
    if (!adbInstallProcess->waitForStarted()) {
        ui->sideloadOutput->appendPlainText("Failed to start adb install.");
    }
}

//--------------------------
// Info function
//--------------------------
void MainWindow::runInfo()
{
    QProcess *infoProcess = new QProcess(this);
    infoProcess->setProgram("adb");
    // Use getprop to retrieve device properties.
    infoProcess->setArguments(QStringList() << "shell" << "getprop");
    connect(infoProcess, &QProcess::readyReadStandardOutput, [this, infoProcess]() {
        QByteArray output = infoProcess->readAllStandardOutput();
        ui->infoOutput->setPlainText(QString::fromLocal8Bit(output));
    });
    connect(infoProcess, &QProcess::readyReadStandardError, [this, infoProcess]() {
        QByteArray errorOutput = infoProcess->readAllStandardError();
        ui->infoOutput->appendPlainText(QString::fromLocal8Bit(errorOutput));
    });
    infoProcess->start();
    if (!infoProcess->waitForStarted()) {
        ui->infoOutput->setPlainText("Failed to start adb getprop command.");
    }
    connect(infoProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            infoProcess, &QProcess::deleteLater);
}

//--------------------------
// Python Script (if needed)
//--------------------------
void MainWindow::startPythonScript()
{
    pythonProcess = new QProcess(this);
    QString program = "python"; // or "python3"
    QStringList arguments;
    arguments << "auto_connect.py";

    pythonProcess->setProgram(program);
    pythonProcess->setArguments(arguments);

    connect(pythonProcess, &QProcess::readyReadStandardOutput, [this]() {
        appendLog("Python Script: " + QString::fromLocal8Bit(pythonProcess->readAllStandardOutput()));
    });
    connect(pythonProcess, QOverload<QProcess::ProcessError>::of(&QProcess::errorOccurred),
            [this](QProcess::ProcessError error) {
                appendLog("Python Script Error: " + QString::number(error) + "\n");
            });

    pythonProcess->start();
    if (!pythonProcess->waitForStarted())
        appendLog("Failed to start Python script\n");
}
