#include "command_util.h"

#include <QProcess>
#include <QTextStream>
#include <QStandardPaths>
#include <QDebug>

#include <memory>

QString CommandUtil::sudoExec(const QString &cmd, QStringList args, QByteArray data)
{
    args.push_front(cmd);

    QString result("");

    try {
        result = CommandUtil::exec("pkexec", args, data);
    } catch (QString &ex) {
        qCritical() << ex;
    }

    return result;
}

QString CommandUtil::exec(const QString &cmd, QStringList args, QByteArray data)
{
    std::unique_ptr<QProcess> process(new QProcess());
    process->start(cmd, args);

    if (! data.isEmpty()) {
        process->write(data);
        process->waitForBytesWritten();
        process->closeWriteChannel();
    }

    // 10 minutes
    process->waitForFinished(600*1000);

    const QByteArray out = process->readAllStandardOutput();

    // Read the result state before tearing the process down, otherwise the
    // error is lost once the channels are closed.
    const QProcess::ProcessError error = process->error();
    const QString err = process->errorString();

    process->kill();
    process->close();

    if (error != QProcess::UnknownError)
        throw err;

    return QString::fromUtf8(out).trimmed();
}

bool CommandUtil::isExecutable(const QString &cmd)
{
    return !QStandardPaths::findExecutable(cmd).isEmpty();
}
