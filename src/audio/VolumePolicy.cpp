#include "audio/VolumePolicy.h"

#include <QtGlobal>

#include <algorithm>
#include <limits>

namespace VolumePolicy {

Kind kindFromString(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QLatin1String("alarmonly"))
        return Kind::AlarmOnly;
    return Kind::Guarantee; // default, including unknown values
}

QString kindToString(Kind kind)
{
    return kind == Kind::AlarmOnly ? QStringLiteral("alarmOnly")
                                   : QStringLiteral("guarantee");
}

RaiseDecision decideRaise(Kind kind, const QVector<int> &currentVolumes,
                          bool currentMute, int floorPercent)
{
    RaiseDecision decision;
    if (kind == Kind::AlarmOnly || currentVolumes.isEmpty())
        return decision; // alarmOnly never touches the sink

    const int floorRaw = qBound(0, floorPercent, 100) * kFullScale / 100;

    QVector<int> target = currentVolumes;
    int minNonZero = std::numeric_limits<int>::max();

    for (int &volume : target) {
        if (volume <= 0) {
            // A silent channel cannot be reached by scaling: lift it to the
            // floor directly (it has no balance left to preserve).
            volume = floorRaw;
        } else {
            minNonZero = std::min(minNonZero, volume);
        }
    }

    // One factor for all audible channels: the quietest one ends exactly at
    // the floor, everyone else keeps their relative balance. Silent channels
    // keep the floor value from the pass above (they must not be scaled a
    // second time).
    if (floorRaw > 0 && minNonZero != std::numeric_limits<int>::max()
        && minNonZero < floorRaw) {
        const double factor =
            static_cast<double>(floorRaw) / static_cast<double>(minNonZero);
        for (int i = 0; i < target.size(); ++i) {
            if (currentVolumes.at(i) > 0)
                target[i] = qBound(0, qRound(target.at(i) * factor),
                                  kFullScale);
        }
    }

    decision.unmute = currentMute;
    decision.touchSink = currentMute || target != currentVolumes;
    decision.targetVolumes = target;
    return decision;
}

int clampStreamPercent(int percent)
{
    return qBound(0, percent, 100);
}

bool volumesMatch(const QVector<int> &a, const QVector<int> &b)
{
    if (a.isEmpty() || a.size() != b.size())
        return false; // empty or channel-count change: nothing trustworthy
    for (int i = 0; i < a.size(); ++i) {
        if (qAbs(a.at(i) - b.at(i)) > kUserOverrideTolerance)
            return false;
    }
    return true;
}

bool isUserOverride(const QVector<int> &applied, bool appliedMute,
                    const QVector<int> &observed, bool observedMute)
{
    if (!volumesMatch(applied, observed))
        return true;
    return appliedMute != observedMute;
}

} // namespace VolumePolicy
