#include "system_info.h"

#include <QObject>
#include <QRegularExpression>
#include <iostream>

SystemInfo::SystemInfo()
{
    QString unknown(QObject::tr("Unknown"));

    try{
        // run the command in English language (guarantee same behaviour across languages)
        const QStringList lines = CpuInfo::lscpuLines();

        const QRegularExpression regexp("\\s+");
        const QString space(" ");

        // x86 reports "Model name", aarch64 only reports "Model"
        QString model = CpuInfo::cpuModelName(lines);

        double clock = 0.0;
        // aarch64 only reports the range the cpu policy scales between,
        // on x86 "CPU MHz" holds the current clock of every core.
        for (const QString &key : QStringList{"CPU max MHz", "CPU MHz"}) {
            clock = CpuInfo::parseFrequency(CpuInfo::lscpuValue(lines, key));
            if (clock > 0) break;
        }

        model = model.contains('@') ? model.split("@").first() : model; // intel : AMD

        this->cpuModel = model.isEmpty() ? unknown : model.trimmed().replace(regexp, space);
        this->cpuSpeed = clock > 0
                ? QString::number(clock/1000.0) + "GHz"
                : unknown;
    } catch(QString &ex) {
        this->cpuModel = unknown;
        this->cpuSpeed = unknown;
    }

    CpuInfo ci;
    this->cpuCore = QString::number(ci.getCpuPhysicalCoreCount());

    // get username
    QString name = qgetenv("USER");

    if (name.isEmpty())
        name = qgetenv("USERNAME");

    try {
        if (name.isEmpty())
            name = CommandUtil::exec("whoami").trimmed();
    } catch (const QString &ex) {
        qCritical() << ex;
    }

   this->username = name;
}

QString SystemInfo::getUsername() const
{
    return username;
}

QString SystemInfo::getHostname() const
{
    return QSysInfo::machineHostName();
}

QStringList SystemInfo::getUserList() const
{
    QStringList passwdUsers = FileUtil::readListFromFile("/etc/passwd");
    QStringList users;

    for(QString &row: passwdUsers) {
        users.append(row.split(":").at(0));
    }

    return users;
}

QStringList SystemInfo::getGroupList() const
{
    QStringList groupFile = FileUtil::readListFromFile("/etc/group");
    QStringList groups;

    for(QString &row: groupFile) {
        groups.append(row.split(":").at(0));
    }

    return groups;
}

QString SystemInfo::getPlatform() const
{
    return QString("%1 %2")
            .arg(QSysInfo::kernelType())
            .arg(QSysInfo::currentCpuArchitecture());
}

QString SystemInfo::getDistribution() const
{
    return QSysInfo::prettyProductName();
}

QString SystemInfo::getKernel() const
{
    return QSysInfo::kernelVersion();
}

QString SystemInfo::getCpuModel() const
{
    return this->cpuModel;
}

QString SystemInfo::getCpuSpeed() const
{
    return this->cpuSpeed;
}

QString SystemInfo::getCpuCore() const
{
    return this->cpuCore;
}

QFileInfoList SystemInfo::getCrashReports() const
{
    QDir reports("/var/crash");

    return reports.entryInfoList(QDir::Files);
}

QFileInfoList SystemInfo::getAppLogs() const
{
    QDir logs("/var/log");

    //remove only files not directory ex. apache2 (log directory)
    return logs.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
}

QFileInfoList SystemInfo::getAppCaches() const
{
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

    QDir caches(homePath + "/.cache");

    return caches.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
}
