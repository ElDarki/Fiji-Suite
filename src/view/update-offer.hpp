#pragma once

// VIEW layer: offers a newer Fiji Suite in a small, non-blocking window: Download (opens the releases page), Skip
// this version, or Later. Texts come from the caller (the plugin's locale).

#include <QString>
#include <functional>

class QWidget;

namespace fiji::view {

struct UpdateOfferTexts {
	QString title, text, info, download, skip, later;
};

void showUpdateOffer(QWidget *parent, const UpdateOfferTexts &texts, const QString &releasesPage,
		     std::function<void()> onSkip);

} // namespace fiji::view
