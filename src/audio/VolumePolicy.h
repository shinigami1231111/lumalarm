#ifndef VOLUMEPOLICY_H
#define VOLUMEPOLICY_H

#include <QString>
#include <QVector>

// Pure decision logic for the alarm volume system: no I/O, no QObject, no
// mixer — everything here is deterministic and unit-tested.
//
// Two policies (guide):
//  - "guarantee" (default): the alarm must be audible — raise the sink to
//    the configured floor and unmute it; the original state is restored
//    exactly afterwards (via RestoreJournal).
//  - "alarmOnly": never touch the system sink; only our own stream matters.
namespace VolumePolicy {

// Raw PulseAudio-style full scale (65536 == 100 %).
constexpr int kFullScale = 65536;

// Guide: a sink change beyond 3 % of full scale during a ring means the
// user touched it — stop managing and do not restore. 65536 * 3 / 100 = 1966.
constexpr int kUserOverrideTolerance = kFullScale * 3 / 100;

enum class Kind { Guarantee, AlarmOnly };

// Config parsing: unknown/absent values fall back to Guarantee (default).
Kind kindFromString(const QString &value);
QString kindToString(Kind kind);

struct RaiseDecision {
    // false => never call the mixer (alarmOnly, nothing to do, or no data).
    bool touchSink = false;
    // Final desired sink state, raw per-channel in channel order. Always
    // populated when touchSink is true; every value is <= kFullScale.
    QVector<int> targetVolumes;
    bool unmute = false; // true when the sink is muted and we manage it
};

// Guarantee: scale by ONE factor so the quietest non-silent channel reaches
// the floor (a silent channel cannot be scaled, so it is lifted to the floor
// directly). One factor preserves channel balance; factor is >= 1 so the
// raise never lowers anything, and every target is clamped at kFullScale
// (never exceed 100 % — no software amplification).
RaiseDecision decideRaise(Kind kind, const QVector<int> &currentVolumes,
                          bool currentMute, int floorPercent);

// Our own alarm stream: 0..100, never above 100 (guide: never > 1.0).
int clampStreamPercent(int percent);

// Compare-and-set helpers on raw volumes.
// Same size, non-empty, and every channel within kUserOverrideTolerance.
bool volumesMatch(const QVector<int> &a, const QVector<int> &b);

// True when the observed sink cannot be ours anymore: volumes beyond the
// tolerance, a mute flip, a channel-count change, or nothing to compare.
bool isUserOverride(const QVector<int> &applied, bool appliedMute,
                    const QVector<int> &observed, bool observedMute);

} // namespace VolumePolicy

#endif // VOLUMEPOLICY_H
