#ifndef WAKECAPS_H
#define WAKECAPS_H

#include <QString>
#include <QStringList>
#include <QVariantMap>

// ── WAKE-1 §A: capability detection — the machine answers, nothing is assumed
//
// Reads the kernel's own files under a sys root (the real /sys in production,
// a fixture directory in tests) and reports what this machine can actually do:
// which sleep states exist, which memory-sleep types back `mem` and which of
// them is the kernel default (so `mem` is never assumed to mean S3), and which
// RTC devices exist with wake enabled (so rtc0 is never assumed to be the
// wake-capable one).
//
// Unlike wake/WakeSecurity.h this unit is NOT inert — the file reading lives
// in WakeCaps.cpp. Keep the two apart: the helper's security posture depends
// on WakeSecurity.h staying free of I/O.

namespace WakePolicy {

struct Capabilities {
    QStringList sleepStates; // /sys/power/state tokens, e.g. freeze standby mem disk
    QStringList memSleepTypes; // /sys/power/mem_sleep tokens, brackets stripped
    QString memSleepDefault; // the bracketed default entry, "" when absent
    QStringList rtcNames; // RTC names in numeric order, e.g. rtc0 rtc1
    QString wakeCapableRtc; // first RTC whose device/power/wakeup says "enabled"
};

// Never throws, never assumes: a missing file or directory yields an empty
// field. sysRoot exists so tests point at fixtures instead of the machine.
Capabilities readCapabilities(const QString &sysRoot);

// The D-Bus shape of the same data (GetCapabilities returns a{sv}).
QVariantMap capabilitiesToVariantMap(const Capabilities &caps);

} // namespace WakePolicy

#endif
