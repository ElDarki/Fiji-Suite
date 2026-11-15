#pragma once

#include "output-dock.hpp"
#include <list>
#include <map>
#include <obs.hpp>
#include <QElapsedTimer>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPointer>
#include <QTimer>
#include <src/utils/widgets/qt-display.hpp>

class CanvasDock;

// One canvas in the multiview: its live program picture and the outputs it feeds.
class CanvasTile : public QFrame {
	Q_OBJECT
private:
	struct Chip {
		QWidget *row = nullptr;
		QLabel *icon = nullptr;
		QLabel *text = nullptr;
		uint64_t lastBytes = 0;
		qint64 lastTime = 0;
		int kbps = 0;
	};

	// weak, so an open multiview never keeps a removed canvas alive
	OBSWeakCanvas canvas;
	QString canvasName;
	QString dockName;
	QPointer<CanvasDock> canvasDock;
	bool mainCanvas;
	bool live = false;

	OBSQTDisplay *display;
	QLabel *title;
	QVBoxLayout *chipsLayout;
	std::map<QString, Chip> chips;
	QElapsedTimer clock;

	static void Draw(void *data, uint32_t cx, uint32_t cy);
	void UpdateTitle();

protected:
	void mouseDoubleClickEvent(QMouseEvent *event) override;

public:
	CanvasTile(obs_canvas_t *canvas, const QString &dockName, CanvasDock *canvasDock, bool mainCanvas,
		   QWidget *parent = nullptr);
	~CanvasTile();

	void UpdateOutputs(const std::vector<OutputState> &states);
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
