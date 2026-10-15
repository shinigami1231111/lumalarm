#include "wake/WakeCaps.h"

#include <QDir>
#include <QFile>

#include <algorithm>

namespace WakePolicy {
namespace {

QString readTrimmed(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll()).trimmed();
}

QStringList splitTokens(const QString &text)
{
    return text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

// rtc10 sorts before rtc2 as text; the kernel names them by number, so order
// them the same way the kernel does.
int rtcNumber(const QString &entry)
{
    return entry.mid(3).toInt();
}

} // namespace

Capabilities readCapabilities(const QString &sysRoot)
{
    Capabilities caps;

    caps.sleepStates = splitTokens(readTrimmed(sysRoot + QLatin1String("/power/state")));

    // mem_sleep looks like "s2idle shallow [deep]": the bracketed entry is the
    // kernel default, and `deep` (S3) may well be absent — every consumer must
    // offer only what is really here.
    const QStringList rawTypes = splitTokens(readTrimmed(sysRoot + QLatin1String("/power/mem_sleep")));
    for (const QString &raw : rawTypes) {
        QString type = raw;
        if (type.startsWith(QLatin1Char('[')) && type.endsWith(QLatin1Char(']'))) {
            caps.memSleepDefault = type.mid(1, type.size() - 2);
            type = caps.memSleepDefault;
        }
        caps.memSleepTypes.append(type);
    }

    // Enumerate /sys/class/rtc/rtcX instead of trusting rtc0: the wake-capable
    // device is the one whose power/wakeup is enabled, and on some machines
    // rtc0 is not it.
    const QDir rtcs(sysRoot + QLatin1String("/class/rtc"));
    QStringList entries = rtcs.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    entries.removeIf([](const QString &e) { return !e.startsWith(QLatin1String("rtc")); });
    std::sort(entries.begin(), entries.end(),
              [](const QString &a, const QString &b) { return rtcNumber(a) < rtcNumber(b); });
    for (const QString &entry : entries) {
        const QString base = rtcs.absoluteFilePath(entry);
        caps.rtcNames.append(readTrimmed(base + QLatin1Char('/') + QStringLiteral("name")));
        if (caps.wakeCapableRtc.isEmpty()
            && readTrimmed(base + QLatin1String("/device/power/wakeup")) == QLatin1String("enabled"))
            caps.wakeCapableRtc = entry;
    }

    return caps;
}

QVariantMap capabilitiesToVariantMap(const Capabilities &caps)
{
    QVariantMap map;
    map.insert(QStringLiteral("sleepStates"), caps.sleepStates);
    map.insert(QStringLiteral("memSleepTypes"), caps.memSleepTypes);
    map.insert(QStringLiteral("memSleepDefault"), caps.memSleepDefault);
    map.insert(QStringLiteral("rtcNames"), caps.rtcNames);
    map.insert(QStringLiteral("wakeCapableRtc"), caps.wakeCapableRtc);
    return map;
}

} // namespace WakePolicy
