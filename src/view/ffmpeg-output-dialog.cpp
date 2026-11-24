#include "ffmpeg-output-dialog.hpp"

#include "focus-scroll-spinbox.hpp"
#include "record-output-dialog.hpp"
#include "text.hpp"
#include "../model/record-output.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>

namespace fiji::view {

namespace {
QString platformName()
{
#ifdef __APPLE__
	return QStringLiteral("mac");
#elif defined(_WIN32)
	return QStringLiteral("windows");
#else
	return QString();
#endif
}

// An encoder in a combo box: its name and id.
QVariant encoderData(const model::FfmpegCodec &codec)
{
	return QVariantList{codec.name, codec.id};
}
} // namespace

FfmpegOutputDialog::FfmpegOutputDialog(QWidget *parent, const model::FfmpegOutput &output, const QStringList &otherNames,
				       bool editing, model::FfmpegCatalogue &catalogue)
	: QDialog(parent),
	  output_(output),
	  otherNames_(otherNames),
	  catalogue_(catalogue)
{
	setWindowTitle(pluginText(editing ? "EditFfmpegOutputWindowTitle" : "NewFfmpegOutputWindowTitle"));
	setModal(true);
	setContentsMargins(0, 0, 0, 0);
	setMinimumSize(400, 120);

	const QStringList patterns = RecordOutputDialog::filenamePatterns();
	pattern_ = patterns.first();
	if (output_.filename.isEmpty()) {
		output_.filename = pattern_ + QStringLiteral(" ") + output_.name;
	}

	confirm_ = new QPushButton(pluginText(editing ? "SaveFfmpegOutput" : "CreateFfmpegOutput"));
	connect(confirm_, &QPushButton::clicked, this, &QDialog::accept);

	form_ = new QFormLayout;
	form_->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	form_->setLabelAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	form_->setSpacing(12);

	// the name; the filename follows it while it is still the pattern
	auto name = new QLineEdit(output_.name);
	filename_ = new QLineEdit(output_.filename);
	connect(name, &QLineEdit::textEdited, this, [this](const QString &text) {
		const QString old = output_.name;
		output_.name = text;
		const QString followed = model::filenameFollowingName(output_.filename, pattern_, old, text);
		if (followed != output_.filename) {
			output_.filename = followed;
			filename_->setText(followed);
		}
		validate();
	});
	form_->addRow(pluginText("OutputName"), name);

	// to a file or to a URL
	type_ = new QComboBox;
	type_->addItem(obsText("Basic.Settings.Output.Adv.FFmpeg.Type.RecordToFile"));
	type_->addItem(obsText("Basic.Settings.Output.Adv.FFmpeg.Type.URL"));
	type_->setCurrentIndex(output_.saveFile ? 0 : 1);
	connect(type_, &QComboBox::currentIndexChanged, this, [this](int index) {
		output_.saveFile = index == 0;
		showType();
		validate();
	});
	form_->addRow(obsText("Basic.Settings.Output.Adv.FFmpeg.Type"), type_);

	url_ = new QLineEdit(output_.url);
	connect(url_, &QLineEdit::textEdited, this, [this](const QString &text) {
		output_.url = text;
		validate();
	});
	form_->addRow(obsText("Basic.Settings.Output.Adv.FFmpeg.SavePathURL"), url_);

	auto pathRow = new QHBoxLayout;
	auto path = new QLineEdit(output_.directory);
	path->setReadOnly(true);
	auto browse = new QPushButton(obsText("Browse"));
	browse->setProperty("themeID", "settingsButtons");
	connect(browse, &QPushButton::clicked, this, [this, path] {
		const QString dir = QFileDialog::getExistingDirectory(this, obsText("Basic.Settings.Output.Simple.SavePath"),
								      path->text(),
								      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
		if (dir.isEmpty()) {
			return;
		}
		path->setText(dir);
		output_.directory = dir;
		validate();
	});
	pathRow->addWidget(path);
	pathRow->addWidget(browse);
	pathRow_ = pathRow;
	form_->addRow(obsText("Basic.Settings.Output.Adv.FFmpeg.SavePathURL"), pathRow);

	auto completer = new QCompleter(patterns, filename_);
	completer->setCaseSensitivity(Qt::CaseSensitive);
	completer->setFilterMode(Qt::MatchContains);
	filename_->setCompleter(completer);
	filename_->setToolTip(obsText("FilenameFormatting.TT"));
	connect(filename_, &QLineEdit::textEdited, this, [this](const QString &text) {
		const QString safe = model::safeFilename(text, platformName());
		if (safe != text) {
			filename_->setText(safe);
		}
		output_.filename = safe;
		validate();
	});
	form_->addRow(obsText("Basic.Settings.Output.Adv.Recording.Filename"), filename_);

	// "generate file names without spaces" is the opposite of allowing them
	noSpace_ = new QCheckBox(obsText("Basic.Settings.Output.NoSpaceFileName"));
	noSpace_->setChecked(!output_.allowSpaces);
	connect(noSpace_, &QCheckBox::toggled, this, [this](bool checked) { output_.allowSpaces = !checked; });
	form_->addRow(QString(), noSpace_);

	// the format: FFmpeg's muxers, sorted, after "default"
	format_ = new QComboBox;
	formats_ = catalogue_.formats();
	std::sort(formats_.begin(), formats_.end(),
		  [](const model::FfmpegFormat &a, const model::FfmpegFormat &b) { return a.name < b.name; });
	format_->addItem(obsText("Basic.Settings.Output.Adv.FFmpeg.FormatDefault"));
	int current = 0;
	for (int i = 0; i < formats_.size(); ++i) {
		const auto &f = formats_.at(i);
		QString label = f.name;
		const bool audio = f.audioCodec != 0, video = f.videoCodec != 0;
		if (audio != video) {
			label += QStringLiteral(" (%1)").arg(obsText(audio ? "Basic.Settings.Output.Adv.FFmpeg.FormatAudio"
									  : "Basic.Settings.Output.Adv.FFmpeg.FormatVideo"));
		}
		format_->addItem(label, i);
		if (current == 0 && f.name == output_.formatName && f.mimeType == output_.mimeType) {
			current = i + 1;
		}
	}
	form_->addRow(obsText("Basic.Settings.Output.Adv.FFmpeg.Format"), format_);
	formatDescription_ = new QLabel;
	formatDescription_->setObjectName("advOutFFFormatDesc");
	form_->addRow(obsText("Basic.Settings.Output.Adv.FFmpeg.FormatDesc"), formatDescription_);

	auto muxer = new QLineEdit(output_.muxerSettings);
	connect(muxer, &QLineEdit::textEdited, this, [this](const QString &text) { output_.muxerSettings = text; });
	form_->addRow(obsText("Basic.Settings.Output.Adv.FFmpeg.MuxerSettings"), muxer);

	auto videoBitrate = new FocusScrollSpinBox;
	videoBitrate->setSingleStep(50);
	videoBitrate->setSuffix(" Kbps");
	videoBitrate->setRange(0, 1000000000);
	videoBitrate->setValue(output_.videoBitrate);
	connect(videoBitrate, &QSpinBox::valueChanged, this, [this](int value) { output_.videoBitrate = value; });
	form_->addRow(obsText("Basic.Settings.Output.VideoBitrate"), videoBitrate);

	auto gop = new FocusScrollSpinBox;
	gop->setRange(0, 1000000000);
	gop->setValue(output_.gopSize);
	connect(gop, &QSpinBox::valueChanged, this, [this](int value) { output_.gopSize = value; });
	form_->addRow(obsText("Basic.Settings.Output.Adv.FFmpeg.GOPSize"), gop);

	auto useRescale = new QCheckBox(obsText("Basic.Settings.Output.Adv.Rescale"));
	useRescale->setLayoutDirection(Qt::RightToLeft);
	useRescale->setChecked(output_.rescale);
	auto rescale = new QComboBox;
	rescale->setEditable(true);
	rescale->lineEdit()->setValidator(
		new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d{1,5}x\\d{1,5}")), rescale));
	rescale->addItems({"1280x720", "1920x1080", "2560x1440", "720x1280", "1080x1920", "1440x2560"});
	if (output_.scaleWidth > 0 && output_.scaleHeight > 0) {
		rescale->setCurrentText(QStringLiteral("%1x%2").arg(output_.scaleWidth).arg(output_.scaleHeight));
	}
	rescale->setEnabled(output_.rescale);
	connect(useRescale, &QCheckBox::toggled, this, [this, rescale](bool checked) {
		output_.rescale = checked;
		rescale->setEnabled(checked);
	});
	connect(rescale, &QComboBox::currentTextChanged, this, [this](const QString &text) {
		model::parseScale(text, output_.scaleWidth, output_.scaleHeight);
	});
	form_->addRow(useRescale, rescale);

	auto ignore = new QCheckBox(obsText("Basic.Settings.Output.Adv.FFmpeg.IgnoreCodecCompat"));
	connect(ignore, &QCheckBox::toggled, this, [this](bool checked) {
		ignoreCompatibility_ = checked;
		formatChanged();
	});
	form_->addRow(QString(), ignore);

	videoEncoder_ = new QComboBox;
	connect(videoEncoder_, &QComboBox::currentIndexChanged, this, [this] {
		const auto data = videoEncoder_->currentData().toList();
		output_.videoEncoder = data.isEmpty() ? QString() : data.at(0).toString();
		output_.videoEncoderId = data.isEmpty() ? 0 : data.at(1).toInt();
	});
	form_->addRow(obsText("Basic.Settings.Output.Adv.FFmpeg.VEncoder"), videoEncoder_);
	auto videoSettings = new QLineEdit(output_.videoSettings);
	connect(videoSettings, &QLineEdit::textEdited, this, [this](const QString &text) { output_.videoSettings = text; });
	form_->addRow(obsText("Basic.Settings.Output.Adv.FFmpeg.VEncoderSettings"), videoSettings);

	auto audioBitrate = new FocusScrollSpinBox;
	audioBitrate->setRange(32, 4096);
	audioBitrate->setSingleStep(16);
	audioBitrate->setSuffix(" Kbps");
	audioBitrate->setValue(output_.audioBitrate);
	connect(audioBitrate, &QSpinBox::valueChanged, this, [this](int value) { output_.audioBitrate = value; });
	form_->addRow(obsText("Basic.Settings.Output.AudioBitrate"), audioBitrate);

	auto tracks = new QHBoxLayout;
	for (int i = 0; i < 6; i++) {
		auto track = new QCheckBox(QString::number(i + 1));
		track->setChecked((output_.audioMixes & (1u << i)) != 0);
		connect(track, &QCheckBox::toggled, this, [this, i](bool checked) {
			if (checked) {
				output_.audioMixes |= (1u << i);
			} else {
				output_.audioMixes &= ~(1u << i);
			}
		});
		tracks->addWidget(track);
	}
	form_->addRow(obsText("Basic.Settings.Output.Adv.AudioTrack"), tracks);

	audioEncoder_ = new QComboBox;
	connect(audioEncoder_, &QComboBox::currentIndexChanged, this, [this] {
		const auto data = audioEncoder_->currentData().toList();
		output_.audioEncoder = data.isEmpty() ? QString() : data.at(0).toString();
		output_.audioEncoderId = data.isEmpty() ? 0 : data.at(1).toInt();
	});
	form_->addRow(obsText("Basic.Settings.Output.Adv.FFmpeg.AEncoder"), audioEncoder_);
	auto audioSettings = new QLineEdit(output_.audioSettings);
	connect(audioSettings, &QLineEdit::textEdited, this, [this](const QString &text) { output_.audioSettings = text; });
	form_->addRow(obsText("Basic.Settings.Output.Adv.FFmpeg.AEncoderSettings"), audioSettings);

	connect(format_, &QComboBox::currentIndexChanged, this, &FfmpegOutputDialog::formatChanged);
	{
		const QSignalBlocker block(format_);
		format_->setCurrentIndex(current);
	}
	formatChanged();
	showType();

	auto controls = new QHBoxLayout;
	controls->setSpacing(0);
	controls->setContentsMargins(0, 0, 0, 0);
	controls->addWidget(confirm_, 0, Qt::AlignRight);

	auto scroll = new QScrollArea;
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	auto content = new QWidget;
	content->setLayout(form_);
	scroll->setWidget(content);

	auto page = new QVBoxLayout(this);
	page->addWidget(scroll);
	page->addLayout(controls);
	validate();
}

void FfmpegOutputDialog::showType()
{
	form_->setRowVisible(url_, !output_.saveFile);
	form_->setRowVisible(pathRow_, output_.saveFile);
	form_->setRowVisible(filename_, output_.saveFile);
	form_->setRowVisible(noSpace_, output_.saveFile);
}

void FfmpegOutputDialog::formatChanged()
{
	const QSignalBlocker blockVideo(videoEncoder_);
	const QSignalBlocker blockAudio(audioEncoder_);
	videoEncoder_->clear();
	audioEncoder_->clear();
	const QVariant data = format_->currentData();
	if (!data.isValid()) {
		// "default": no format chosen
		formatDescription_->clear();
		output_.formatName.clear();
		output_.mimeType.clear();
		validate();
		return;
	}
	const auto &format = formats_.at(data.toInt());
	output_.formatName = format.name;
	output_.mimeType = format.mimeType;
	output_.extension = model::firstExtension(format.extensions);
	formatDescription_->setText(format.longName);

	// the encoders, after "disable"; the saved one, else the format's default, is chosen
	const auto choices = model::ffmpegEncodersFor(catalogue_, format, ignoreCompatibility_);
	auto fill = [](QComboBox *box, const QList<model::FfmpegEncoderChoice> &list, const QString &saved, QString &name,
		       int &id) {
		box->addItem(obsText("Basic.Settings.Output.Adv.FFmpeg.AVEncoderDisable"));
		int chosen = -1, fallback = -1;
		for (const auto &choice : list) {
			QString label = choice.label();
			if (choice.isDefault) {
				label += QStringLiteral(" (%1)").arg(obsText("Basic.Settings.Output.Adv.FFmpeg.AVEncoderDefault"));
				fallback = box->count();
			}
			if (chosen < 0 && !saved.isEmpty() && choice.codec.name == saved) {
				chosen = box->count();
			}
			box->addItem(label, encoderData(choice.codec));
		}
		const int index = chosen >= 0 ? chosen : fallback;
		if (index >= 0) {
			box->setCurrentIndex(index);
			const auto data = box->itemData(index).toList();
			name = data.at(0).toString();
			id = data.at(1).toInt();
		}
	};
	fill(videoEncoder_, choices.video, output_.videoEncoder, output_.videoEncoder, output_.videoEncoderId);
	fill(audioEncoder_, choices.audio, output_.audioEncoder, output_.audioEncoder, output_.audioEncoderId);
	validate();
}

void FfmpegOutputDialog::validate()
{
	confirm_->setEnabled(model::ffmpegOutputValid(output_, otherNames_));
}

} // namespace fiji::view
