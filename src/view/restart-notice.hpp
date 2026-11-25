#pragma once

// VIEW layer: tells the user, once and without blocking, that OBS needs a restart before the newly added Fiji themes
// show up in Settings -> Appearance. The texts come from the caller (the plugin's locale).

#include <QString>

class QWidget;

namespace fiji::view {

void showRestartNotice(QWidget *parent, const QString &title, const QString &text);

} // namespace fiji::view
