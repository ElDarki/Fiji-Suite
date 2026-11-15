#include "multiview-dock.hpp"

#include "canvas-clone-dock.hpp"
#include "canvas-dock.hpp"
#include <cmath>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QContextMenuEvent>
#include <QDockWidget>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenu>
#include <QScreen>
#include <QStyle>
#include <src/utils/icon.hpp>
#include <src/utils/widgets/display-helpers.hpp>

extern std::list<CanvasDock *> canvas_docks;
extern std::list<CanvasCloneDock *> canvas_clone_docks;
extern OutputDock *output_dock;

static std::list<MultiviewDock *> multiviews;

// An output is flagged once it drops more than this share of its frames.
static const double droppedWarningPercent = 1.0;

CanvasTile::CanvasTile(obs_canvas_t *canvas_, const QString &dockName_, CanvasDock *canvasDock_, bool mainCanvas_,
		       QWidget *parent)
	: QFrame(parent),
	  canvas(OBSGetWeakRef(canvas_)),
	  canvasName(QString::fromUtf8(obs_canvas_get_name(canvas_))),
	  dockName(dockName_),
	  canvasDock(canvasDock_),
	  mainCanvas(mainCanvas_),
	  display(new OBSQTDisplay(this)),
	  title(new QLabel(this)),
	  chipsLayout(new QVBoxLayout)
{
	setObjectName(QStringLiteral("canvasTile"));
	// the border is the on-air light: red while any output on this canvas is live
	setStyleSheet(QStringLiteral("#canvasTile{border:2px solid transparent;border-radius:4px;}"
				     "#canvasTile[live=\"true\"]{border-color:#d92c2c;}"));
	setProperty("live", false);

	auto layout = new QVBoxLayout;
	layout->setContentsMargins(4, 4, 4, 4);
	layout->setSpacing(4);

	title->setAlignment(Qt::AlignCenter);
	layout->addWidget(title);

	display->setMinimumSize(QSize(120, 68));
	display->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	connect(display, &OBSQTDisplay::DisplayCreated,
		[this]() { obs_display_add_draw_callback(display->GetDisplay(), Draw, this); });
	layout->addWidget(display, 1);

	chipsLayout->setContentsMargins(0, 0, 0, 0);
	chipsLayout->setSpacing(2);
	layout->addLayout(chipsLayout);

	setLayout(layout);
	clock.start();
	UpdateTitle();
}

CanvasTile::~CanvasTile()
{
	obs_display_remove_draw_callback(display->GetDisplay(), Draw, this);
}

void CanvasTile::Draw(void *data, uint32_t cx, uint32_t cy)
{
	auto tile = static_cast<CanvasTile *>(data);
	OBSCanvas canvas = OBSGetStrongRef(tile->canvas);
	if (!canvas || obs_canvas_removed(canvas)) {
		return;
	}
	obs_video_info ovi = {};
	if (!obs_canvas_get_video_info(canvas, &ovi) || !ovi.base_width || !ovi.base_height) {
		return;
	}

	int x, y;
	float scale;
	GetScaleAndCenterPos(ovi.base_width, ovi.base_height, cx, cy, x, y, scale);

	gs_viewport_push();
	gs_projection_push();
	const bool previous = gs_set_linear_srgb(true);

	gs_ortho(0.0f, float(ovi.base_width), 0.0f, float(ovi.base_height), -100.0f, 100.0f);
	gs_set_viewport(x, y, int(scale * float(ovi.base_width)), int(scale * float(ovi.base_height)));
	obs_canvas_render(canvas);

	gs_set_linear_srgb(previous);
	gs_projection_pop();
	gs_viewport_pop();
}

void CanvasTile::UpdateTitle()
{
	auto text = mainCanvas ? QString::fromUtf8(obs_module_text("MainCanvas")) : canvasName;
	OBSCanvas c = OBSGetStrongRef(canvas);
	obs_video_info ovi = {};
	if (c && obs_canvas_get_video_info(c, &ovi)) {
		text += QStringLiteral("  %1x%2").arg(ovi.base_width).arg(ovi.base_height);
	}
	if (canvasDock && canvasDock->IsStudioMode()) {
		text += QStringLiteral("  ·  ") + QString::fromUtf8(obs_module_text("StudioMode"));
	}
	if (title->text() != text) {
		title->setText(text);
	}
}

static QIcon output_icon(const OutputState &s)
{
	if (s.type == QStringLiteral("record")) {
		return QIcon(QStringLiteral(":/fiji/media/record.svg"));
	} else if (s.type == QStringLiteral("backtrack")) {
		return QIcon(QStringLiteral(":/fiji/media/backtrack_off.svg"));
	} else if (s.type == QStringLiteral("virtual_cam")) {
		return QIcon(QStringLiteral(":/fiji/media/virtual_cam_off.svg"));
	} else if (s.type == QStringLiteral("ffmpeg")) {
		return QIcon(QStringLiteral(":/fiji/media/ffmpeg_off.svg"));
	} else if (s.server.isEmpty()) {
		return QIcon(QStringLiteral(":/fiji/media/stream.svg"));
	}
	return getPlatformIconFromEndpoint(s.server);
}

void CanvasTile::UpdateOutputs(const std::vector<OutputState> &states)
{
	UpdateTitle();

	// outputs with no canvas set, and OBS's own outputs, belong to the main canvas
	std::vector<const OutputState *> mine;
	for (const auto &s : states) {
		if ((s.main || s.canvas.isEmpty()) ? mainCanvas : (!mainCanvas && s.canvas == canvasName)) {
			mine.push_back(&s);
		}
	}

	// drop chips of outputs that are gone
	for (auto it = chips.begin(); it != chips.end();) {
		const bool present =
			std::any_of(mine.begin(), mine.end(), [&](const OutputState *s) { return s->name == it->first; });
		if (present) {
			++it;
			continue;
		}
		chipsLayout->removeWidget(it->second.row);
		delete it->second.row;
		it = chips.erase(it);
	}

	bool anyLive = false;
	const qint64 now = clock.elapsed();
	for (const auto s : mine) {
		auto found = chips.find(s->name);
		if (found == chips.end()) {
			Chip chip;
			chip.row = new QWidget(this);
			auto rowLayout = new QHBoxLayout;
			rowLayout->setContentsMargins(0, 0, 0, 0);
			rowLayout->setSpacing(6);
			chip.icon = new QLabel(chip.row);
			chip.icon->setPixmap(output_icon(*s).pixmap(16, 16));
			rowLayout->addWidget(chip.icon);
			chip.text = new QLabel(chip.row);
			rowLayout->addWidget(chip.text, 1);
			chip.row->setLayout(rowLayout);
			chipsLayout->addWidget(chip.row);
			chip.lastBytes = s->totalBytes;
			chip.lastTime = now;
			found = chips.emplace(s->name, chip).first;
		}
		auto &chip = found->second;

		if (s->active && now > chip.lastTime) {
			if (s->totalBytes >= chip.lastBytes) {
				chip.kbps = int((s->totalBytes - chip.lastBytes) * 8 / (now - chip.lastTime));
			}
		} else if (!s->active) {
			chip.kbps = 0;
		}
		chip.lastBytes = s->totalBytes;
		chip.lastTime = now;

		QString text = s->name;
		bool warning = false;
		if (s->reconnecting) {
			text += QStringLiteral("  ·  ") + QString::fromUtf8(obs_module_text("OutputReconnecting"));
			warning = true;
		} else if (s->active) {
			anyLive = true;
			const auto secs = s->startTime.isValid() ? s->startTime.secsTo(QDateTime::currentDateTime()) : 0;
			const auto t = QTime(0, 0).addSecs(int(secs % 86400));
			text += QStringLiteral("  ·  ") + QString::fromUtf8(obs_module_text("OutputLive")) + QStringLiteral(" ") +
				t.toString(t.hour() ? QStringLiteral("hh:mm:ss") : QStringLiteral("mm:ss"));
			if (s->type == QStringLiteral("stream") || s->type == QStringLiteral("ffmpeg")) {
				text += QStringLiteral("  ·  %1 kb/s").arg(chip.kbps);
			}
			if (s->totalFrames > 0 && s->droppedFrames > 0) {
				const double dropped = 100.0 * s->droppedFrames / s->totalFrames;
				text += QStringLiteral("  ·  ") +
					QString::fromUtf8(obs_module_text("OutputDropped")).arg(dropped, 0, 'f', 1);
				warning = dropped >= droppedWarningPercent;
			}
		} else {
			text += QStringLiteral("  ·  ") + QString::fromUtf8(obs_module_text("OutputIdle"));
		}
		if (chip.text->text() != text) {
			chip.text->setText(text);
		}
		const auto style = warning ? QStringLiteral("color:#e0a000;") : QString();
		if (chip.text->styleSheet() != style) {
			chip.text->setStyleSheet(style);
		}
		chip.text->setEnabled(s->active || s->reconnecting);
	}

	if (anyLive != live) {
		live = anyLive;
		setProperty("live", live);
		style()->unpolish(this);
		style()->polish(this);
	}
}

void CanvasTile::mouseDoubleClickEvent(QMouseEvent *event)
{
	QFrame::mouseDoubleClickEvent(event);
	if (dockName.isEmpty()) {
		return;
	}
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto dock = main_window ? main_window->findChild<QDockWidget *>(dockName) : nullptr;
	if (dock) {
		dock->show();
		dock->raise();
	}
}

MultiviewDock::MultiviewDock(QWidget *parent, bool window_) : QFrame(parent), grid(new QGridLayout), window(window_)
{
	grid->setContentsMargins(4, 4, 4, 4);
	grid->setSpacing(4);
	setLayout(grid);
	setFocusPolicy(Qt::StrongFocus);

	multiviews.push_back(this);
	Rebuild();

	connect(&refreshTimer, &QTimer::timeout, this, [this] { Refresh(); });
	refreshTimer.start(1000);
}

MultiviewDock::~MultiviewDock()
{
	multiviews.remove(this);
}

void MultiviewDock::Clear()
{
	for (auto tile : tiles) {
		grid->removeWidget(tile);
		delete tile;
	}
	tiles.clear();
}

void MultiviewDock::Rebuild()
{
	Clear();

	OBSCanvasAutoRelease main = obs_get_main_canvas();
	if (main) {
		tiles.push_back(new CanvasTile(main, QString(), nullptr, true, this));
	}
	for (const auto &dock : canvas_docks) {
		if (dock->GetCanvas() && dock->parentWidget()) {
			tiles.push_back(new CanvasTile(dock->GetCanvas(), dock->parentWidget()->objectName(), dock, false, this));
		}
	}
	for (const auto &dock : canvas_clone_docks) {
		if (dock->GetCanvas() && dock->parentWidget()) {
			tiles.push_back(
				new CanvasTile(dock->GetCanvas(), dock->parentWidget()->objectName(), nullptr, false, this));
		}
	}

	// as square a grid as the number of canvases allows
	const int columns = std::max(1, int(std::ceil(std::sqrt(double(tiles.size())))));
	for (size_t i = 0; i < tiles.size(); i++) {
		grid->addWidget(tiles[i], int(i) / columns, int(i) % columns);
	}
	Refresh();
}

void MultiviewDock::Refresh()
{
	if (!output_dock || tiles.empty()) {
		return;
	}
	const auto states = output_dock->GetOutputStates();
	for (auto tile : tiles) {
		tile->UpdateOutputs(states);
	}
}

void MultiviewDock::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu menu(this);
	if (window) {
		menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Close")), this, [this] { close(); });
	} else {
		auto fullscreen = menu.addMenu(QString::fromUtf8(obs_module_text("MultiviewFullscreen")));
		const auto screens = QGuiApplication::screens();
		for (int i = 0; i < screens.size(); i++) {
			auto screen = screens[i];
			const auto geometry = screen->geometry();
			const auto text = QStringLiteral("%1: %2x%3").arg(i + 1).arg(geometry.width()).arg(geometry.height());
			fullscreen->addAction(text, this, [screen] {
				// its own top-level window; Esc or the right-click menu closes it
				auto view = new MultiviewDock(nullptr, true);
				view->setAttribute(Qt::WA_DeleteOnClose);
				view->setWindowTitle(QString::fromUtf8(obs_module_text("FijiSuiteMultiview")));
				view->setGeometry(screen->geometry());
				view->showFullScreen();
			});
		}
	}
	menu.exec(event->globalPos());
}

void MultiviewDock::keyPressEvent(QKeyEvent *event)
{
	if (window && event->key() == Qt::Key_Escape) {
		close();
		return;
	}
	QFrame::keyPressEvent(event);
}

void MultiviewDock::RebuildAll()
{
	for (auto view : multiviews) {
		view->Rebuild();
	}
}

void MultiviewDock::ClearAll()
{
	for (auto view : multiviews) {
		view->Clear();
	}
}

void MultiviewDock::CloseWindows()
{
	// closing deletes the window, which removes it from the list
	const auto views = multiviews;
	for (auto view : views) {
		if (view->window) {
			view->close();
		}
	}
}
