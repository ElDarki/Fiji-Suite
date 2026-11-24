#pragma once

// VIEW: the dialog for a new or edited recording / backtrack output. Edits a model::RecordOutput and only lets the user
// save what model::recordOutputValid accepts.

#include "../model/record-output.hpp"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace fiji::view {

class RecordOutputDialog : public QDialog {
	Q_OBJECT

public:
	// editing: an existing output (its values, "Save"); otherwise a new one ("Create"). otherNames: the names the
	// other outputs have.
	RecordOutputDialog(QWidget *parent, const model::RecordOutput &output, const QStringList &otherNames, bool editing);

	// The values to save (the split / limit values are 0 when splitting is off).
	model::RecordOutput result() const;
	// OBS's filename patterns (the first is the default), from its "FilenameFormatting.completer" text.
	static QStringList filenamePatterns();

	// For tests.
	QLineEdit *nameField() const { return name_; }
	QLineEdit *pathField() const { return path_; }
	QLineEdit *filenameField() const { return filename_; }
	QPushButton *confirmButton() const { return confirm_; }
	QCheckBox *splitBox() const { return split_; }
	QSpinBox *maxTimeBox() const { return maxTime_; }

private:
	void validate();

	model::RecordOutput output_;
	QStringList otherNames_;
	QString pattern_;
	QLineEdit *name_ = nullptr;
	QLineEdit *path_ = nullptr;
	QLineEdit *filename_ = nullptr;
	QComboBox *format_ = nullptr;
	QSpinBox *maxTime_ = nullptr;
	QSpinBox *maxSize_ = nullptr;
	QCheckBox *split_ = nullptr; // recordings only
	QPushButton *confirm_ = nullptr;
};

} // namespace fiji::view
