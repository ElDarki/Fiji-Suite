#pragma once

// VIEW: the dialog for a new or edited FFmpeg output (to a file or a URL): the format from FFmpeg's muxers, its video
// and audio encoders (model::ffmpegEncodersFor), bitrates, rescaling and tracks. Only what model::ffmpegOutputValid
// accepts can be saved.

#include "../model/ffmpeg-output.hpp"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;

namespace fiji::view {

class FfmpegOutputDialog : public QDialog {
	Q_OBJECT

public:
	FfmpegOutputDialog(QWidget *parent, const model::FfmpegOutput &output, const QStringList &otherNames, bool editing,
			   model::FfmpegCatalogue &catalogue);

	model::FfmpegOutput result() const { return output_; }

	// For tests.
	QComboBox *typeBox() const { return type_; }
	QComboBox *formatBox() const { return format_; }
	QComboBox *videoEncoderBox() const { return videoEncoder_; }
	QComboBox *audioEncoderBox() const { return audioEncoder_; }
	QCheckBox *noSpaceBox() const { return noSpace_; }
	QLineEdit *urlField() const { return url_; }
	QPushButton *confirmButton() const { return confirm_; }

private:
	void formatChanged();
	void showType();
	void validate();

	model::FfmpegOutput output_;
	QStringList otherNames_;
	model::FfmpegCatalogue &catalogue_;
	QList<model::FfmpegFormat> formats_;
	bool ignoreCompatibility_ = false;
	QString pattern_;

	QFormLayout *form_ = nullptr;
	QComboBox *type_ = nullptr;
	QLineEdit *url_ = nullptr;
	QLayout *pathRow_ = nullptr;
	QLineEdit *filename_ = nullptr;
	QCheckBox *noSpace_ = nullptr;
	QComboBox *format_ = nullptr;
	QLabel *formatDescription_ = nullptr;
	QComboBox *videoEncoder_ = nullptr;
	QComboBox *audioEncoder_ = nullptr;
	QPushButton *confirm_ = nullptr;
};

} // namespace fiji::view
