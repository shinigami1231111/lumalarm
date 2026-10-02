# Changelog

All notable changes to Lumalarm are documented here.
Format based on Keep a Changelog (https://keepachangelog.com/).

## [Unreleased]

## [1.0.5] - 2026-10-02

### Added
- D-Bus wake helper (STG-1.1): removes the /etc/sudoers.d/lumalarm
  requirement entirely. Scheduling now goes through a privileged
  helper with strict input validation (60 <= seconds <= 86400).
- Polkit action org.lumalarm.schedule.wake for authenticated scheduling.
- systemd activation files for the helper (D-Bus system service).

### Changed
- WakeManager now uses QDBusInterface to reach the helper, falling
  back to direct rtcwake if the helper is unavailable.
- Wake verification switched from RTC readback to `systemctl list-timers`
  while the system is awake (RTC is only written at suspend entry; see
  "Known limitations").
- PKGBUILD hooks moved to lumalarm.install (inline makepkg hooks are no-ops).

### Removed
- README instructions for editing /etc/sudoers.d/lumalarm.

### Deviations from original spec (empirically required)

- **D1:** Added data/org.lumalarm.Helper.conf. The default
  /etc/dbus-1/system.conf denies own/send, so D-Bus activation of the
  helper fails without a tailored policy file.
- **D2:** Polkit authorization is enforced inside the helper
  (via pkcheck) in addition to the D-Bus policy. Without it the
  delivered polkit action is decorative and the helper is callable
  by any local process.
- **D3:** systemd invocation uses --timer-property=WakeSystem=yes
  instead of the spec-literal --property=WakeSystem=yes, which
  fails on systemd 262 with "Unknown assignment".
- **D4:** The helper class declares Q_CLASSINFO("D-Bus Interface",
  "org.lumalarm.Helper"). Without it Qt exports the interface under
  local.<something> and D-Bus clients cannot find it.
- **D5:** Cancel/replace behavior fixed after testing: a bare-name
  `systemctl stop` hit the .service instead of the .timer, and
  re-scheduling orphaned the previous timer. Both now use the
  fully-qualified unit name and stop the previous timer first.

### Known limitations

- **B1 (RTC readback):** systemd's WakeSystem= uses CLOCK_BOOTTIME
  and writes the RTC only at suspend entry. Reading
  /sys/class/rtc/rtc0/wakealarm while the system is awake therefore
  always returns 0. Verification now uses `systemctl list-timers`;
  the RTC check remains in place only for the rtcwake fallback path.
