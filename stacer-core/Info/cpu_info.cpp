#include "cpu_info.h"

#include <QDir>
#include <QRegularExpression>

#include "Utils/command_util.h"

int CpuInfo::getCpuPhysicalCoreCount() const
{
    static int count = 0;

    if (! count) {
        const QStringList cpuinfo = FileUtil::readListFromFile(PROC_CPUINFO);

        if (!cpuinfo.isEmpty()) {
            count = getPhysicalCoreCountFromCpuinfo(cpuinfo);

            if (! count) {
                // the architecture doesn't expose any topology information,
                // fall back to one core per logical cpu.
                count = getCpuCoreCount();
            }
        }
    }

    return count;
}

int CpuInfo::getPhysicalCoreCountFromCpuinfo(const QStringList &cpuinfo) const
{
    QSet<QPair<int, int> > physicalCoreSet;
    int physical = 0;
    int core = 0;

    for (const QString &line : cpuinfo) {
        if (line.startsWith("physical id")) {
            QStringList fields = line.split(": ");
            if (fields.size() > 1)
                physical = fields[1].toInt();
        }
        if (line.startsWith("core id")) {
            QStringList fields = line.split(": ");
            if (fields.size() > 1)
                core = fields[1].toInt();
            // We assume core id appears after physical id.
            physicalCoreSet.insert(qMakePair(physical, core));
        }
    }

    if (! physicalCoreSet.isEmpty())
        return physicalCoreSet.size();

    // aarch64 and some other architectures don't report the physical/core ids,
    // ask lscpu for the topology instead.
    const QStringList lines = lscpuLines();
    const int coresPerSocket = lscpuValue(lines, "Core(s) per socket").toInt();
    const int sockets = lscpuValue(lines, "Socket(s)").toInt();

    if (coresPerSocket > 0 && sockets > 0)
        return coresPerSocket * sockets;

    return 0;
}

int CpuInfo::getCpuCoreCount() const
{
    static quint8 count = 0;

    if (! count) {
        QStringList cpuinfo = FileUtil::readListFromFile(PROC_CPUINFO);

        if (! cpuinfo.isEmpty())
            for (const QString &line : cpuinfo) {
                if (line.startsWith("processor"))
                    count++;
            }
    }

    return count;
}

QList<double> CpuInfo::getLoadAvgs() const
{
    QList<double> avgs = {0, 0, 0};

    QStringList strListAvgs = FileUtil::readStringFromFile(PROC_LOADAVG).split(QRegularExpression("\\s+"));

    if (strListAvgs.count() > 2) {
        avgs.clear();
        avgs << strListAvgs.takeFirst().toDouble();
        avgs << strListAvgs.takeFirst().toDouble();
        avgs << strListAvgs.takeFirst().toDouble();
    }

    return avgs;
}

QStringList CpuInfo::lscpuLines()
{
    static const QStringList lines = []() {
        try {
            return CommandUtil::exec("bash", {"-c", LSCPU_COMMAND}).split('\n');
        } catch (QString &ex) {
            qCritical() << ex;
            return QStringList();
        }
    }();

    return lines;
}

QString CpuInfo::lscpuValue(const QStringList &lines, const QString &key)
{
    for (const QString &line : lines) {
        const int separator = line.indexOf(':');

        if (separator < 0)
            continue;

        if (line.left(separator).trimmed() == key)
            return line.mid(separator + 1).trimmed();
    }

    return QString();
}

QString CpuInfo::cpuModelName(const QStringList &lines)
{
    QString model = lscpuValue(lines, "Model name");

    if (! model.isEmpty())
        return model;

    // aarch64 lscpu only has "Model" (e.g. 0), which is useless as a name
    model = lscpuValue(lines, "Model");
    if (! model.isEmpty() && ! model.contains(QRegularExpression("^\\d+$")))
        return model;

    // the ARM kernel can name the cpu in /proc/cpuinfo instead
    for (const QString &line : FileUtil::readListFromFile(PROC_CPUINFO)) {
        for (const QString &key : QStringList{"model name", "Hardware", "CPU part"}) {
            if (line.startsWith(key)) {
                const QString value = line.split(':').last().trimmed();
                if (! value.isEmpty())
                    return value;
            }
        }
    }

    return QString();
}

double CpuInfo::parseFrequency(const QString &value)
{
    QString frequency = value.trimmed();

    if (frequency.isEmpty())
        return 0.0;

    // lscpu is localized, some locales print the decimal comma (2424,0000)
    frequency.replace(',', '.');

    // the same value can be reported in Hz, kHz, MHz or GHz
    double multiplier = 1.0;
    const QRegularExpression unit("([kMG]?Hz)", QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = unit.match(frequency);
    const QString suffix = match.hasMatch() ? match.captured(1).toLower() : QString("mhz");

    if (suffix == "ghz")
        multiplier = 1000.0;
    else if (suffix == "khz")
        multiplier = 0.001;
    else if (suffix == "hz")
        multiplier = 0.000001;

    // drop the unit
    frequency.remove(QRegularExpression("[^0-9.]"));

    bool ok = false;
    const double number = frequency.toDouble(&ok);

    return ok ? number * multiplier : 0.0;
}

double CpuInfo::getAvgClock() const
{
    // the average of the per core clocks is the most accurate value (x86)
    const QList<double> clocks = getClocks();

    if (!clocks.isEmpty()) {
        double total = 0.0;
        for (const double &clock : clocks) total += clock;
        return total / clocks.size();
    }

    // aarch64 doesn't report the current clock of the cores, only the range the
    // cpu policy is allowed to scale between.
    const QStringList lines = lscpuLines();

    const double scalingClock = parseFrequency(lscpuValue(lines, "CPU(s) scaling MHz"));
    if (scalingClock > 0) return scalingClock;

    const double clockMHz = parseFrequency(lscpuValue(lines, "CPU MHz"));
    if (clockMHz > 0) return clockMHz;

    const double minClock = parseFrequency(lscpuValue(lines, "CPU min MHz"));
    const double maxClock = parseFrequency(lscpuValue(lines, "CPU max MHz"));

    if (minClock > 0 && maxClock > 0)
        return (minClock + maxClock) / 2;

    if (maxClock > 0)
        return maxClock;

    return minClock;
}

QList<double> CpuInfo::getClocks() const
{
    QList<double> clocks;

    // x86 reports the current clock of every core
    QStringList lines;
    const QStringList cpuinfo = FileUtil::readListFromFile(PROC_CPUINFO);
    for (const QString &line : cpuinfo) {
        if (line.startsWith("cpu MHz"))
            lines.append(line);
    }

    for (const QString &line : lines) {
        const double clock = parseFrequency(line.split(":").last());

        if (clock > 0)
            clocks.push_back(clock);
    }

    if (!clocks.isEmpty())
        return clocks;

    // aarch64: the current clock of every core lives in the cpufreq sysfs
    return getCpufreqClocks();
}

QList<double> CpuInfo::getCpufreqClocks() const
{
    // every file holds a plain number in kHz
    const auto readFrequency = [](const QString &path) {
        for (const QString &file : QStringList{"scaling_cur_freq", "cpuinfo_cur_freq", "cpuinfo_max_freq"}) {
            const double frequency = FileUtil::readStringFromFile(path + file).toDouble() / 1000.0;

            // the current frequency is missing while a core is offline
            if (frequency > 0)
                return frequency;
        }

        return 0.0;
    };

    QList<double> clocks;

    for (int i = 0; i < getCpuCoreCount(); ++i) {
        const double frequency = readFrequency(QString(CPUFREQ_PATH).arg(i));

        if (frequency > 0)
            clocks.push_back(frequency);
    }

    if (!clocks.isEmpty())
        return clocks;

    // a big.LITTLE system has one policy per cluster, /sys/devices/system/cpu/cpu%1/cpufreq
    // only points at the policy of the cluster the core belongs to
    const QDir policies(CPUFREQ_POLICIES_PATH);

    for (const QString &policy : policies.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const double frequency = readFrequency(policies.filePath(policy) + "/");

        if (frequency > 0)
            clocks.push_back(frequency);
    }

    return clocks;
}

QList<int> CpuInfo::getCpuPercents() const
{
    QList<double> cpuTimes;

    QList<int> cpuPercents;

    QStringList times = FileUtil::readListFromFile(PROC_STAT);

    if (! times.isEmpty())
    {
     /*  user nice system idle iowait  irq  softirq steal guest guest_nice
        cpu  4705 356  584    3699   23    23     0       0     0      0
         .
        cpuN 4705 356  584    3699   23    23     0       0     0      0

          The meanings of the columns are as follows, from left to right:
             - user: normal processes executing in user mode
             - nice: niced processes executing in user mode
             - system: processes executing in kernel mode
             - idle: twiddling thumbs
             - iowait: waiting for I/O to complete
             - irq: servicing interrupts
             - softirq: servicing softirqs
             - steal: involuntary wait
             - guest: running a normal guest
             - guest_nice: running a niced guest
        */

        const QRegularExpression sep("\\s+");
        int count = CpuInfo::getCpuCoreCount() + 1;
        for (int i = 0; i < count; ++i)
        {
            QStringList n_times = times.at(i).split(sep);
            n_times.removeFirst();
            for (const QString &t : n_times)
                cpuTimes << t.toDouble();

            cpuPercents << getCpuPercent(cpuTimes, i);

            cpuTimes.clear();
        }
    }

    return cpuPercents;
}

int CpuInfo::getCpuPercent(const QList<double> &cpuTimes, const int &processor) const
{
    const int N = getCpuCoreCount()+1;

    static QVector<double> l_idles(N);
    static QVector<double> l_totals(N);

    int utilisation = 0;

    if (cpuTimes.count() > 0) {

        double idle = cpuTimes.at(3) + cpuTimes.at(4); // get (idle + iowait)
        double total = 0.0;
        for (const double &t : cpuTimes) total += t; // get total time

        double idle_delta  = idle  - l_idles[processor];
        double total_delta = total - l_totals[processor];

        if (total_delta)
            utilisation = 100 * ((total_delta - idle_delta) / total_delta);

        l_idles[processor] = idle;
        l_totals[processor] = total;
    }

    if (utilisation > 100) utilisation = 100;
    else if (utilisation < 0) utilisation = 0;

    return utilisation;
}
