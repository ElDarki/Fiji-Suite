// The VIEW layer with Qt Widgets and no OBS (target fiji-view-test, run offscreen). Every view source is built into it,
// so a view that calls libobs or legacy code does not link. Exit code 0 means every check passed.

#include "../src/view/name-dialog.hpp"
#include "../src/view/restart-notice.hpp"

#include <QApplication>
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

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	test_name_dialog();
	test_restart_notice();
	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	std::printf("view: all checks passed\n");
	return 0;
}
