#pragma once

// In-memory CaptureSources for the tests: the test plays libobs by calling the listener, and sees the picks.

#include "../src/obs/capture-ports.hpp"

#include <QHash>
#include <QPair>

struct FakeCaptureSources final : fiji::obs::CaptureSources {
	Listener listener;
	int listens = 0;
	QHash<QString, QList<fiji::obs::CaptureWindow>> windowsOf;
	QList<QPair<QString, QVariant>> picks;

	void listen(Listener l) override
	{
		listener = std::move(l);
		listens++;
	}
	QList<fiji::obs::CaptureWindow> windows(const QString &uuid) override { return windowsOf.value(uuid); }
	void pickWindow(const QString &uuid, const QVariant &value) override { picks.append({uuid, value}); }
};
