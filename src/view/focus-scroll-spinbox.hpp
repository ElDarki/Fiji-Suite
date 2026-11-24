#pragma once

// VIEW: spin boxes that only take the mouse wheel when they have focus (so scrolling a form scrolls the form).

#include <QSpinBox>
#include <QDoubleSpinBox>

class FocusScrollSpinBox : public QSpinBox {
	Q_OBJECT
protected:
	virtual void wheelEvent(QWheelEvent *event);

public:
	FocusScrollSpinBox(QWidget *parent = nullptr);
};

class FocusScrollDoubleSpinBox : public QDoubleSpinBox {
	Q_OBJECT
protected:
	virtual void wheelEvent(QWheelEvent *event);

public:
	FocusScrollDoubleSpinBox(QWidget *parent = nullptr);
};

