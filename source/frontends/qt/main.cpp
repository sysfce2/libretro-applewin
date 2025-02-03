#include "StdAfx.h"

#include "qapple.h"
#include <QApplication>
#include <QTimer>
#include <QCommandLineParser>
#include <QMediaDevices>
#include <QAudioDevice>

#include "linux/version.h"
#include "applicationname.h"

#include <iostream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setOrganizationName(ORGANIZATION_NAME);
    QApplication::setApplicationName(APPLICATION_NAME);
    const QString qversion = QString::fromStdString(getVersion());
    QApplication::setApplicationVersion(qversion);

    QCommandLineParser parser;
    parser.setApplicationDescription("Qt Apple Emulator");
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption runOption("r", "start immeditaely");
    parser.addOption(runOption);

    const QCommandLineOption logStateOption("load-state", "load state file", "file");
    parser.addOption(logStateOption);

    const QCommandLineOption audioDeviceOption("a", "audio device", "id");
    parser.addOption(audioDeviceOption);

    parser.process(app);

    int audioDeviceID = -1;
    if (parser.isSet(audioDeviceOption))
    {
        audioDeviceID = parser.value(audioDeviceOption).toInt();
    }

    const QList<QAudioDevice> devices = QMediaDevices::audioOutputs();
    // default
    QAudioDevice audioDevice = QMediaDevices::defaultAudioOutput();

    std::cout << "Audio devices:" << std::endl;
    for (size_t i = 0; i < devices.size(); ++i)
    {
        const QAudioDevice &dev = devices[i];
        std::cout << i;
        if (i == audioDeviceID)
        {
            audioDevice = dev;
            std::cout << " (*): ";
        }
        else
        {
            std::cout << "    : ";
        }
        std::cout << dev.description().toStdString();
        if (dev.isDefault())
        {
            std::cout << " <default>";
        }
        std::cout << std::endl;
    }

    QApple w(audioDevice);

    const bool run = parser.isSet(runOption);
    const QString stateFile = parser.value(logStateOption);
    if (!stateFile.isEmpty())
    {
        w.loadStateFile(stateFile);
    }

    w.show();

    if (run)
    {
        QTimer::singleShot(0, &w, SLOT(startEmulator()));
    }

    return app.exec();
}
