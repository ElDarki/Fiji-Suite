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
// The multiview currently covering OBS's centre area, and whether the main preview was on before it.
static MultiviewDock *overPreview = nullptr;
static bool previewWasEnabled = true;

CanvasTile::CanvasTile(obs_canvas_t *canvas_, const QString &dockName_, CanvasDock *canvasDock_, bool mainCanvas_,
		       QWidget *parent)
	: QFrame(parent),
	  canvas(OBSGetWeakRef(canvas_)),
	  canvasName(QString::fromUtf8(obs_canvas_get_name(canvas_))),
	  dockName(dockName_),
	  canvasDock(canvasDock_),
	  mainCanvas(mainCanvas_),
	  outputs(QString::fromUtf8(obs_canvas_get_name(canvas_)), mainCanvas_),
	  display(new OBSQTDisplay(this)),
	  title(new QLabel(this)),
	  chipsLayout(new QVBoxLayout)
{
	setObjectName(QStringLiteral("canvasTile"));
	// the border is the on-air light: liveColor (red unless the theme says otherwise) while any output on this
	// canvas is live
	ApplyBorderStyle();
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

static QIcon output_icon(const fiji::model::Chip &s)
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

void CanvasTile::ApplyBorderStyle()
{
	setStyleSheet(QStringLiteral("#canvasTile{border:2px solid transparent;border-radius:4px;}"
				     "#canvasTile[live=\"true\"]{border-color:%1;}")
			      .arg(liveColor.name()));
}

void CanvasTile::SetLiveColor(const QColor &color)
{
	if (!color.isValid() || color == liveColor)
		return;
	liveColor = color;
	ApplyBorderStyle();
}

void CanvasTile::SetWarningColor(const QColor &color)
{
	if (!color.isValid() || color == warningColor)
		return;
	warningColor = color;
	for (auto &entry : chips) {
		if (!entry.second.text->styleSheet().isEmpty())
			entry.second.text->setStyleSheet(QStringLiteral("color:%1;").arg(warningColor.name()));
	}
}

void CanvasTile::UpdateOutputs(const QList<fiji::model::MultiviewOutput> &states)
{
	UpdateTitle();

	const auto update = outputs.update(states, clock.elapsed());
	for (const auto &name : update.gone) {
		auto it = chips.find(name);
		if (it != chips.end()) {
			chipsLayout->removeWidget(it->second.row);
			delete it->second.row;
			chips.erase(it);
		}
	}

	for (const auto &s : update.chips) {
		auto found = chips.find(s.name);
		if (found == chips.end()) {
			Chip chip;
			chip.row = new QWidget(this);
			auto rowLayout = new QHBoxLayout;
			rowLayout->setContentsMargins(0, 0, 0, 0);
			rowLayout->setSpacing(6);
			chip.icon = new QLabel(chip.row);
			chip.icon->setPixmap(output_icon(s).pixmap(16, 16));
			rowLayout->addWidget(chip.icon);
			chip.text = new QLabel(chip.row);
			rowLayout->addWidget(chip.text, 1);
			chip.row->setLayout(rowLayout);
			chipsLayout->addWidget(chip.row);
			found = chips.emplace(s.name, chip).first;
		}
		auto &chip = found->second;

		QString text = s.name;
		const auto sep = QStringLiteral("  ·  ");
		switch (s.state) {
		case fiji::model::ChipState::Reconnecting:
			text += sep + QString::fromUtf8(obs_module_text("OutputReconnecting"));
			break;
		case fiji::model::ChipState::Live:
			text += sep + QString::fromUtf8(obs_module_text("OutputLive")) + QStringLiteral(" ") +
				fiji::model::liveTime(s.liveSeconds);
			if (s.showRate) {
				text += sep + QStringLiteral("%1 kb/s").arg(s.kbps);
			}
			if (s.droppedPercent >= 0) {
				text += sep + QString::fromUtf8(obs_module_text("OutputDropped")).arg(s.droppedPercent, 0, 'f', 1);
			}
			break;
		case fiji::model::ChipState::Idle:
			text += sep + QString::fromUtf8(obs_module_text("OutputIdle"));
			break;
		}
		if (chip.text->text() != text) {
			chip.text->setText(text);
		}
		const auto style = s.warning ? QStringLiteral("color:%1;").arg(warningColor.name()) : QString();
		if (chip.text->styleSheet() != style) {
			chip.text->setStyleSheet(style);
		}
		chip.text->setEnabled(s.state != fiji::model::ChipState::Idle);
	}

	if (update.live != live) {
		live = update.live;
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
	// The dock is created while OBS is still loading; its tiles are built by RebuildAll() once the
	// canvases exist. A window is opened later, when everything is there.
	if (window) {
		Rebuild();
	}

	connect(&refreshTimer, &QTimer::timeout, this, [this] { Refresh(); });
	refreshTimer.start(1000);
}

MultiviewDock::~MultiviewDock()
{
	multiviews.remove(this);
	if (overPreview == this) {
		overPreview = nullptr;
		obs_frontend_set_preview_enabled(previewWasEnabled);
	}
}

void MultiviewDock::ToggleOverMainPreview()
{
	if (overPreview) {
		overPreview->close(); // deletes it, which turns the main preview back on
		return;
	}
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto central = main_window ? main_window->centralWidget() : nullptr;
	if (!central) {
		return;
	}
	// The main preview is switched off while it is covered, so it neither renders underneath nor
	// shows through; it goes back to how it was when the multiview is closed.
	previewWasEnabled = obs_frontend_preview_enabled();
	obs_frontend_set_preview_enabled(false);
	auto view = new MultiviewDock(central, true);
	overPreview = view;
	view->setAttribute(Qt::WA_DeleteOnClose);
	view->setAutoFillBackground(true);
	view->setGeometry(central->rect());
	central->installEventFilter(view);
	view->show();
	view->raise();
	view->setFocus();
}

// Keeps the multiview the size of the centre area it covers.
bool MultiviewDock::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::Resize && watched == parentWidget()) {
		setGeometry(parentWidget()->rect());
	}
	return QFrame::eventFilter(watched, event);
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
	const int columns = fiji::model::multiviewColumns(int(tiles.size()));
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
	QList<fiji::model::MultiviewOutput> states;
	const auto now = QDateTime::currentDateTime();
	for (const auto &s : output_dock->GetOutputStates()) {
		states.append({s.name, s.type, s.server, s.canvas, s.main, s.active, s.reconnecting,
			       s.startTime.isValid() ? s.startTime.secsTo(now) : 0, s.totalBytes, s.totalFrames,
			       s.droppedFrames});
	}
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
		auto over = menu.addAction(QString::fromUtf8(obs_module_text("MultiviewOverPreview")), this,
					    [] { ToggleOverMainPreview(); });
		over->setCheckable(true);
		over->setChecked(overPreview != nullptr);
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
