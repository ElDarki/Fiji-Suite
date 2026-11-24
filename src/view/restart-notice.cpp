#include "restart-notice.hpp"

#include <QMessageBox>

namespace fiji::view {

void ShowRestartNotice(QWidget *parent, const QString &title, const QString &text)
{
	auto box = new QMessageBox(QMessageBox::Information, title, text, QMessageBox::Ok, parent);
	box->setAttribute(Qt::WA_DeleteOnClose);
	box->setModal(false);
	box->show();
}

} // namespace fiji::view
