// The VIEW layer with Qt Widgets and no OBS (target fiji-view-test, run offscreen). Every view source is built into it,
// so a view that calls libobs or legacy code does not link. Exit code 0 means every check passed.

#include "../src/view/capture-dock.hpp"
#include "../src/view/name-dialog.hpp"
#include "fake-capture-sources.hpp"
#include "fake-live-scenes.hpp"
#include "../src/view/live-scenes-dock.hpp"
#include "../src/view/record-output-dialog.hpp"
#include "../src/view/stats-dock.hpp"
#include "../src/view/stream-output-dialog.hpp"
#include "../src/view/ffmpeg-output-dialog.hpp"
#include "fake-ffmpeg.hpp"
#include <QComboBox>
#include <QStackedWidget>
#include "../src/model/profile-model.hpp"
#include "fake-stats-source.hpp"
#include <QHeaderView>
#include <QPixmap>
#include <QTableView>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QListWidget>
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

// The Live Scenes dock shows the model, selects the program entry, and picking an entry puts it on program.
static void test_live_scenes_dock()
{
	FakeLiveScenes fake;
	model::LiveScenesModel model;
	controller::LiveScenesController scenes(model, fake);
	scenes.start();
	scenes.addScene("Intro");
	view::LiveScenesDock dock(scenes, {"Add", "Remove", "Up", "Down"});
	CHECK(dock.list()->count() == 1 && dock.list()->currentRow() == -1);

	QMenu menu;
	dock.fillAddMenu(menu);
	CHECK(menu.actions().size() == 3);
	menu.actions().at(1)->trigger(); // Game
	CHECK(dock.list()->count() == 2 && dock.list()->item(1)->text() == "Game");

	dock.list()->setCurrentRow(1); // the user picks Game
	CHECK(fake.shown == QStringList({"u-game"}));
	scenes.programChanged(); // OBS reports it
	CHECK(dock.list()->currentRow() == 1 && dock.list()->item(1)->isSelected());
	CHECK(fake.shown.size() == 1); // showing the model did not pick again

	dock.moveCurrent(-1);
	CHECK(dock.list()->item(0)->text() == "Game" && dock.list()->currentRow() == 0);
	dock.removeCurrent();
	CHECK(dock.list()->count() == 1 && dock.list()->item(0)->text() == "Intro");
}

// The recording output dialog: create needs a folder, the filename follows the name, splitting off saves no limits.
static void test_record_output_dialog()
{
	const QString pattern = view::RecordOutputDialog::filenamePatterns().first();
	view::RecordOutputDialog create(nullptr, model::newRecordOutput(false, "Recording", pattern), {"Taken"}, false);
	CHECK(create.windowTitle() == "NewRecordOutputWindowTitle" && create.confirmButton()->text() == "CreateRecordOutput");
	CHECK(create.filenameField()->text() == pattern + " Recording");
	CHECK(!create.confirmButton()->isEnabled()); // no folder
	create.nameField()->setText("Game");
	emit create.nameField()->textEdited("Game");
	CHECK(create.filenameField()->text() == pattern + " Game" && create.result().filename == pattern + " Game");
	create.nameField()->setText("Taken");
	emit create.nameField()->textEdited("Taken");
	CHECK(!create.confirmButton()->isEnabled());

	model::RecordOutput existing;
	existing.name = "Rec";
	existing.path = "/videos";
	existing.filename = "my file";
	existing.format = "mkv";
	existing.maxTimeSec = 600;
	view::RecordOutputDialog edit(nullptr, existing, {}, true);
	CHECK(edit.windowTitle() == "EditRecordOutputWindowTitle" && edit.confirmButton()->isEnabled());
	CHECK(edit.splitBox()->isChecked() && edit.maxTimeBox()->isEnabled() && edit.result().format == "mkv");
	edit.splitBox()->setChecked(false);
	CHECK(!edit.maxTimeBox()->isEnabled() && edit.result().maxTimeSec == 0);
	emit edit.nameField()->textEdited("Rec 2");
	CHECK(edit.filenameField()->text() == "my file"); // the user's own filename stays

	model::RecordOutput back = model::newRecordOutput(true, "Backtrack", pattern);
	back.path = "/videos";
	view::RecordOutputDialog backtrack(nullptr, back, {}, false);
	CHECK(!backtrack.splitBox() && backtrack.confirmButton()->isEnabled() && backtrack.maxTimeBox()->value() == 30);
	backtrack.maxTimeBox()->setValue(0);
	CHECK(!backtrack.confirmButton()->isEnabled());
}

// The Stats dock: shows the table, hides the columns that start hidden, keeps its layout in the profile, paints.
static void test_stats_dock()
{
	FakeStatsSource source;
	model::StatsEncoder video{1, "x264", 100, true, true, 10, 1280, 720};
	model::StatsOutput out;
	out.id = 10;
	out.name = "Fiji Suite Output Main";
	out.active = true;
	out.videoEncoders = {1};
	source.next.encoders = {video};
	source.next.outputs = {out};
	source.next.canvases = {{"Main", 100, 0, 10, 1280, 720}};
	model::OutputStatsTable table({}, [](const QString &) { return QVariant(qint64(0xff00ff00)); });
	controller::StatsController stats(table, source);
	model::ProfileModel profile;
	profile.load({});
	view::StatsDock dock(stats, &profile);
	dock.resize(900, 300);
	dock.show();
	stats.tick();
	source.next.encoders[0].encodedFrames = 40;
	stats.tick();
	CHECK(dock.table()->model()->rowCount() == 1);
	CHECK(dock.table()->isColumnHidden(model::OutputStatsTable::EncoderName) &&
	      !dock.table()->isColumnHidden(model::OutputStatsTable::EncoderFps));

	QMenu menu;
	dock.fillHeaderMenu(menu, 0);
	CHECK(menu.actions().size() == 2 + model::OutputStatsTable::ColumnCount); // Sort, separator, one per column

	dock.table()->setColumnHidden(model::OutputStatsTable::EncoderName, false);
	dock.SaveSettings(false, "Live");
	CHECK(!profile.view("stats_state_Live").toString().isEmpty());
	dock.table()->setColumnHidden(model::OutputStatsTable::EncoderName, true);
	dock.LoadMode("live"); // falls back to the mode's other spelling
	dock.LoadMode("Live");
	CHECK(!dock.table()->isColumnHidden(model::OutputStatsTable::EncoderName));

	QPixmap canvas(dock.size()); // the graphs, colour lines and grouped header paint without trouble
	dock.render(&canvas);

	dock.hide();
	stats.tick();
	CHECK(table.rows().isEmpty()); // hidden: the table empties
}

// The stream output dialog: platforms first, each page's server kind, the key rule, and editing an existing output.
static void test_stream_output_dialog()
{
	auto servers = [](const QString &service) {
		return service == "Twitch" ? QList<model::StreamServer>{{"Default", "rtmp://live.twitch.tv/app"}, {"EU", "rtmp://eu.twitch.tv/app"}}
					   : QList<model::StreamServer>{};
	};
	view::StreamOutputDialog create(nullptr, {"Taken"}, servers);
	CHECK(create.pages()->count() == 9 && create.pages()->currentIndex() == 0);
	create.choosePlatform("twitch");
	auto twitch = create.page("twitch");
	CHECK(create.pages()->currentIndex() == 1 && twitch.serverList && twitch.serverList->count() == 2);
	CHECK(create.result().name == "TwitchOutput" && create.result().server == "rtmp://live.twitch.tv/app");
	CHECK(!twitch.confirm->isEnabled()); // no key yet
	twitch.key->setText("live_123");
	emit twitch.key->textEdited("live_123");
	CHECK(twitch.confirm->isEnabled());
	twitch.serverList->setCurrentIndex(1);
	CHECK(create.result().server == "rtmp://eu.twitch.tv/app");

	create.choosePlatform("kick"); // a fixed server
	auto kick = create.page("kick");
	CHECK(kick.serverField && !kick.serverField->isEnabled() && create.result().server.contains("live-video.net"));
	CHECK(create.result().key.isEmpty()); // each page has its own fields

	create.choosePlatform("custom");
	auto custom = create.page("custom");
	custom.serverField->setText("srt://example:9000");
	emit custom.serverField->textEdited("srt://example:9000");
	CHECK(custom.confirm->isEnabled()); // SRT needs no key
	custom.name->setText("Taken");
	emit custom.name->textEdited("Taken");
	CHECK(!custom.confirm->isEnabled());

	view::StreamOutputDialog edit(nullptr, {"Main", "rtmp://eu.twitch.tv/app", "secret"}, {}, servers);
	auto page = edit.page("twitch");
	CHECK(edit.windowTitle() == "EditStreamOutputWindowTitle" && page.serverList && page.serverList->currentIndex() == 1);
	CHECK(page.key->text() == "secret" && page.confirm->isEnabled() && edit.result().server == "rtmp://eu.twitch.tv/app");
}

// The FFmpeg output dialog: formats from the catalogue, their encoders, file or URL, and the "no spaces" box.
static void test_ffmpeg_output_dialog()
{
	FakeFfmpeg ffmpeg;
	model::FfmpegOutput fresh;
	fresh.name = "Archive";
	view::FfmpegOutputDialog create(nullptr, fresh, {}, false, ffmpeg);
	CHECK(create.formatBox()->count() == 3 && create.formatBox()->currentIndex() == 0); // "default", mp3, mp4
	CHECK(!create.confirmButton()->isEnabled());
	create.formatBox()->setCurrentIndex(2); // mp4
	CHECK(create.result().formatName == "mp4" && create.result().extension == "mp4");
	CHECK(create.videoEncoderBox()->count() == 2 && create.result().videoEncoder == "libx264"); // "disable" + x264
	CHECK(create.result().audioEncoder == "aac" && create.result().audioEncoderId == 86018);
	create.noSpaceBox()->setChecked(true);
	CHECK(!create.result().allowSpaces);
	create.typeBox()->setCurrentIndex(1); // to a URL
	CHECK(!create.result().saveFile && !create.confirmButton()->isEnabled());
	create.urlField()->setText("srt://x");
	emit create.urlField()->textEdited("srt://x");
	CHECK(create.confirmButton()->isEnabled());

	model::FfmpegOutput saved = create.result();
	saved.audioEncoder = "libmp3lame";
	view::FfmpegOutputDialog edit(nullptr, saved, {}, true, ffmpeg);
	CHECK(edit.formatBox()->currentIndex() == 2 && edit.result().audioEncoder == "libmp3lame"); // the saved one stays
	CHECK(edit.windowTitle() == "EditFfmpegOutputWindowTitle" && edit.confirmButton()->isEnabled());
}

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	test_name_dialog();
	test_restart_notice();
	test_capture_dock();
	test_live_scenes_dock();
	test_record_output_dialog();
	test_stats_dock();
	test_stream_output_dialog();
	test_ffmpeg_output_dialog();
	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	std::printf("view: all checks passed\n");
	return 0;
}
