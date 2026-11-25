#include "update-offer.hpp"

#include <QAbstractButton>
#include <QDesktopServices>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>

namespace fiji::view {

void showUpdateOffer(QWidget *parent, const UpdateOfferTexts &texts, const QString &releasesPage,
		     std::function<void()> onSkip)
{
	auto box = new QMessageBox(parent);
	box->setAttribute(Qt::WA_DeleteOnClose);
	box->setWindowModality(Qt::NonModal);
	box->setIcon(QMessageBox::Information);
	box->setWindowTitle(texts.title);
	box->setText(texts.text);
	box->setInformativeText(texts.info);
	auto download = box->addButton(texts.download, QMessageBox::AcceptRole);
	auto skip = box->addButton(texts.skip, QMessageBox::DestructiveRole);
	auto later = box->addButton(texts.later, QMessageBox::RejectRole);
	box->setDefaultButton(download);
	box->setEscapeButton(later);
	QObject::connect(box, &QMessageBox::buttonClicked, box, [download, skip, releasesPage, onSkip](QAbstractButton *button) {
		if (button == download) {
			QDesktopServices::openUrl(QUrl(releasesPage)); // a fixed address: nothing from the answer is opened
		} else if (button == skip && onSkip) {
			onSkip();
		}
	});
	box->show();
}

} // namespace fiji::view
