#pragma once

// VIEW layer: tells the streamer, once and without blocking, to restart OBS so new themes show up in
// Settings -> Appearance. The words come from the caller (views do not call OBS).

#include <QString>

class QWidget;

namespace fiji::view {

void ShowRestartNotice(QWidget *parent, const QString &title, const QString &text);

} // namespace fiji::view
