#pragma once

// VIEW: the dialog for a new or edited stream output. A new one starts on a page of platforms; each platform's page
// (built from model::streamPlatforms) asks for the name, the server (fixed, from a list, or typed) and the key. Only
// what model::streamOutputValid accepts can be saved.

#include "../model/stream-output.hpp"

#include <QDialog>
#include <functional>

class QComboBox;
class QLineEdit;
class QPushButton;
class QStackedWidget;

namespace fiji::view {

class StreamOutputDialog : public QDialog {
	Q_OBJECT

public:
	// OBS's servers for a service name (obs::StreamServersFor in the plugin).
	using Servers = std::function<QList<model::StreamServer>(const QString &service)>;

	// A new output: the platforms first.
	StreamOutputDialog(QWidget *parent, const QStringList &otherNames, Servers servers);
	// An existing output: the page of the platform its server belongs to.
	StreamOutputDialog(QWidget *parent, const model::StreamOutput &output, const QStringList &otherNames, Servers servers);

	model::StreamOutput result() const { return output_; }

	// For tests.
	QStackedWidget *pages() const { return pages_; }
	void choosePlatform(const QString &id);
	struct Page {
		QLineEdit *name = nullptr;
		QLineEdit *serverField = nullptr; // fixed and typed servers
		QComboBox *serverList = nullptr;  // servers from a list
		QLineEdit *key = nullptr;
		QPushButton *confirm = nullptr;
	};
	Page page(const QString &id) const;

private:
	QWidget *platformsPage();
	QWidget *platformPage(const model::StreamPlatform &platform, bool editing);
	void showPage(int index); // takes the page's values (a new output)
	void validate();

	model::StreamOutput output_;
	QStringList otherNames_;
	Servers servers_;
	QStackedWidget *pages_ = nullptr; // a new output: 0 = the platforms, then one page per platform
	QList<QString> pageIds_;
	QList<Page> pageWidgets_;
	QPushButton *currentConfirm_ = nullptr;
};

} // namespace fiji::view
