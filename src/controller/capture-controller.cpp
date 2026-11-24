#include "capture-controller.hpp"

#include <QMetaObject>
#include <QPointer>

namespace fiji::controller {

QString captureText(const QString &executable, const QString &title)
{
	QString text;
	if (!executable.isEmpty()) {
		text += QStringLiteral("[") + executable + QStringLiteral("]: ");
	}
	return text + title;
}

CaptureController::CaptureController(obs::CaptureSources &sources, QObject *parent) : QObject(parent), sources_(sources) {}

void CaptureController::start()
{
	if (started_) {
		return;
	}
	started_ = true;
	// libobs reports from its own threads: each report is queued to this object's thread
	QPointer<CaptureController> self(this);
	auto queue = [self](auto call) {
		if (self) {
			QMetaObject::invokeMethod(self.data(), call, Qt::QueuedConnection);
		}
	};
	sources_.listen({
		[self, queue](const QString &uuid, const QString &name) {
			queue([self, uuid, name] {
				if (self)
					self->onAdded(uuid, name);
			});
		},
		[self, queue](const QString &uuid) {
			queue([self, uuid] {
				if (self)
					self->onRemoved(uuid);
			});
		},
		[self, queue](const QString &uuid, const QString &name) {
			queue([self, uuid, name] {
				if (self)
					self->onRenamed(uuid, name);
			});
		},
		[self, queue](const QString &uuid, const QString &executable, const QString &title) {
			const QString capture = captureText(executable, title);
			queue([self, uuid, capture] {
				if (self)
					self->onHooked(uuid, capture);
			});
		},
	});
}

int CaptureController::indexOf(const QString &uuid) const
{
	for (int i = 0; i < entries_.size(); i++) {
		if (entries_.at(i).uuid == uuid) {
			return i;
		}
	}
	return -1;
}

void CaptureController::onAdded(const QString &uuid, const QString &name)
{
	if (indexOf(uuid) >= 0) {
		return;
	}
	entries_.append({uuid, name, QString()});
	emit added(entries_.last());
}

void CaptureController::onRemoved(const QString &uuid)
{
	const int i = indexOf(uuid);
	if (i < 0) {
		return;
	}
	entries_.removeAt(i);
	emit removed(uuid);
}

void CaptureController::onRenamed(const QString &uuid, const QString &name)
{
	const int i = indexOf(uuid);
	if (i < 0 || entries_.at(i).name == name) {
		return;
	}
	entries_[i].name = name;
	emit changed(entries_.at(i));
}

void CaptureController::onHooked(const QString &uuid, const QString &capture)
{
	const int i = indexOf(uuid);
	if (i < 0 || entries_.at(i).capture == capture) {
		return;
	}
	entries_[i].capture = capture;
	emit changed(entries_.at(i));
}

} // namespace fiji::controller
