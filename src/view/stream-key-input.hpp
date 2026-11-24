#pragma once

#include <QLineEdit>

// VIEW: a stream key field; it tells when it gains or loses focus (the dialog shows the key only while it has focus).
class StreamKeyInput : public QLineEdit
{
	Q_OBJECT
	
public:
	explicit StreamKeyInput(QWidget *parent = nullptr);
	
signals:
	void focusGained();
	void focusLost();
	
protected:
	void focusInEvent(QFocusEvent *event) override;
	void focusOutEvent(QFocusEvent *event) override;
};
