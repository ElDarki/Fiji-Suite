#pragma once

#include "output-dock.hpp"
#include "../model/multiview.hpp"
#include <list>
#include <map>
#include <obs.hpp>
#include <QElapsedTimer>
#include <QColor>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPointer>
#include <QTimer>
#include <src/utils/widgets/qt-display.hpp>

class CanvasDock;

// One canvas in the multiview: its live program picture and the outputs it feeds.
// The live border and the warning text colours can be set by a theme, the way OBS themes colour the volume meter:
//   CanvasTile { qproperty-liveColor: #c8243f; qproperty-warningColor: #8f5000; }
class CanvasTile : public QFrame {
	Q_OBJECT
	Q_PROPERTY(QColor liveColor READ LiveColor WRITE SetLiveColor DESIGNABLE true)
	Q_PROPERTY(QColor warningColor READ WarningColor WRITE SetWarningColor DESIGNABLE true)
private:
	struct Chip {
		QWidget *row = nullptr;
		QLabel *icon = nullptr;
		QLabel *text = nullptr;
	};

	// weak, so an open multiview never keeps a removed canvas alive
	OBSWeakCanvas canvas;
	QString canvasName;
	QString dockName;
	QPointer<CanvasDock> canvasDock;
	bool mainCanvas;
	bool live = false;
	QColor liveColor{0xd9, 0x2c, 0x2c};
	QColor warningColor{0xe0, 0xa0, 0x00};

	void ApplyBorderStyle();

	OBSQTDisplay *display;
	QLabel *title;
	QVBoxLayout *chipsLayout;
	std::map<QString, Chip> chips;
	fiji::model::TileOutputs outputs; // which outputs are on this canvas, and their status
	QElapsedTimer clock;

	static void Draw(void *data, uint32_t cx, uint32_t cy);
	void UpdateTitle();

protected:
	void mouseDoubleClickEvent(QMouseEvent *event) override;

public:
	CanvasTile(obs_canvas_t *canvas, const QString &dockName, CanvasDock *canvasDock, bool mainCanvas,
		   QWidget *parent = nullptr);
	~CanvasTile();

	void UpdateOutputs(const QList<fiji::model::MultiviewOutput> &states);

	QColor LiveColor() const { return liveColor; }
	void SetLiveColor(const QColor &color);
	QColor WarningColor() const { return warningColor; }
	void SetWarningColor(const QColor &color);
};

// Shows every canvas side by side. Used as a dock, and as a borderless window for a spare monitor.
class MultiviewDock : public QFrame {
	Q_OBJECT
private:
	QGridLayout *grid;
	std::vector<CanvasTile *> tiles;
	QTimer refreshTimer;
	bool window;

	void Rebuild();
	void Clear();
	void Refresh();

protected:
	void contextMenuEvent(QContextMenuEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

public:
	MultiviewDock(QWidget *parent = nullptr, bool window = false);
	~MultiviewDock();

	// Canvases changed: rebuild every open multiview.
	static void RebuildAll();
	// Canvases are about to go away: drop the tiles so nothing renders a dead canvas.
	static void ClearAll();
	// OBS is exiting: close the extra windows.
	static void CloseWindows();
	// Shows the multiview in OBS's centre area in place of the main preview, or puts the preview back.
	static void ToggleOverMainPreview();
};
