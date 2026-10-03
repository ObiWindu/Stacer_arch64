#ifndef CPUINFO_H
#define CPUINFO_H

#include <QDebug>
#include <QSet>
#include <QStringList>
#include <QVector>

#include "Utils/file_util.h"

#define PROC_CPUINFO "/proc/cpuinfo"
#define LSCPU_COMMAND "LC_ALL=C lscpu"
#define PROC_LOADAVG "/proc/loadavg"
#define PROC_STAT    "/proc/stat"
#define CPUFREQ_PATH "/sys/devices/system/cpu/cpu%1/cpufreq/"
#define CPUFREQ_POLICIES_PATH "/sys/devices/system/cpu/cpufreq/"

#include "stacer-core_global.h"

class STACERCORESHARED_EXPORT CpuInfo
{
public:
    int getCpuPhysicalCoreCount() const;
    int getCpuCoreCount() const;
    QList<int> getCpuPercents() const;
    QList<double> getLoadAvgs() const;
    double getAvgClock() const;
    QList<double> getClocks() const;

    /*
     * Architecture independent helpers.
     * `lscpu` reports different fields depending on the architecture: x86 exposes
     * `Model name` and `CPU MHz`, while aarch64 only reports `Model` and the min/max
     * MHz of the cpu policy, so callers have to be able to ask for optional fields.
     */
    static QStringList lscpuLines();
    static QString lscpuValue(const QStringList &lines, const QString &key);
    // Resolves the cpu model across architectures: x86 reports `Model name`, aarch64
    // only reports the numeric `Model` and ARM kernels may expose `model name`,
    // `Hardware` or `CPU part` in /proc/cpuinfo.
    static QString cpuModelName(const QStringList &lines);
    // Converts a frequency to MHz, tolerating locale decimal separators (2400,000)
    // and units (Hz, kHz, MHz, GHz).
    static double parseFrequency(const QString &value);

private:
    int getCpuPercent(const QList<double> &cpuTimes, const int &processor = 0) const;
    int getPhysicalCoreCountFromCpuinfo(const QStringList &cpuinfo) const;
    // Reads the current frequencies from the cpufreq sysfs interface, which is the
    // only source of per cpu clocks on aarch64.
    QList<double> getCpufreqClocks() const;
};

#endif // CPUINFO_H
