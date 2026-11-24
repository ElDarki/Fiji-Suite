// The VIEW layer with Qt Widgets and no OBS (target fiji-view-test, run offscreen). Every view source is built into it,
// so a view that calls libobs or legacy code does not link. Exit code 0 means every check passed.

#include "../src/view/capture-dock.hpp"
#include "../src/view/name-dialog.hpp"
#include "fake-capture-sources.hpp"
#include "../src/view/restart-notice.hpp"

#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QMessageBox>
#include <cstdio>

static int failures = 0;
#define CHECK(cond)                                                       \
	do {                                                              \
		if (!(cond)) {                                            \
			std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond); \
			failures++;                                       \
		}                                                         \
	} while (0)

using namespace fiji;

static void test_name_dialog()
{
	CHECK(view::NameDialog::CleanName("  Scene 2\t") == "Scene 2");
	CHECK(view::NameDialog::CleanName("\t \t").empty());
	CHECK(view::NameDialog::CleanName("a  b") == "a  b"); // only the ends
}

static void test_restart_notice()
{
	QWidget parent;
	view::ShowRestartNotice(&parent, "Restart", "New themes");
	const auto boxes = parent.findChildren<QMessageBox *>();
	CHECK(boxes.size() == 1 && boxes.at(0)->text() == "New themes" && !boxes.at(0)->isModal());
}

// The Capture dock follows the controller: rows come and go, names and captures change, the menu lists the windows.
static void test_capture_dock()
{
	FakeCaptureSources fake;
	controller::CaptureController captures(fake);
	captures.start();
	fake.listener.added("u1", "Game");
	QApplication::processEvents();

	view::CaptureDock dock(captures, "Config"); // starts with what the controller has
	CHECK(dock.rows().size() == 1 && dock.rows().value("u1").name->text() == "Game");
	CHECK(dock.rows().value("u1").button->toolTip() == "Config");

	fake.listener.added("u2", "Window");
	fake.listener.hooked("u1", "game.exe", "Level 1");
	fake.listener.renamed("u1", "Main game");
	QApplication::processEvents();
	CHECK(dock.rows().size() == 2);
	CHECK(dock.rows().value("u1").name->text() == "Main game");
	CHECK(dock.rows().value("u1").capture->text() == "[game.exe]: Level 1");

	fake.windowsOf["u2"] = {{"Notepad", QString("notepad:1")}, {"Browser", qlonglong(7)}};
	QMenu menu;
	dock.fillWindowMenu("u2", menu);
	CHECK(menu.actions().size() == 2 && menu.actions().at(1)->text() == "Browser");
	menu.actions().at(1)->trigger();
	CHECK(fake.picks.size() == 1 && fake.picks.at(0).first == "u2" && fake.picks.at(0).second.toLongLong() == 7);

	fake.listener.removed("u1");
	QApplication::processEvents();
	CHECK(dock.rows().size() == 1 && !dock.rows().contains("u1"));
}

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	test_name_dialog();
	test_restart_notice();
	test_capture_dock();
	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	std::printf("view: all checks passed\n");
	return 0;
}
