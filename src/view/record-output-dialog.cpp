#include "record-output-dialog.hpp"

#include "focus-scroll-spinbox.hpp"
#include "text.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

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
} // namespace

QStringList RecordOutputDialog::filenamePatterns()
{
	return obsText("FilenameFormatting.completer").split(QLatin1Char('\n'));
}

RecordOutputDialog::RecordOutputDialog(QWidget *parent, const model::RecordOutput &output, const QStringList &otherNames,
				       bool editing)
	: QDialog(parent),
	  output_(output),
	  otherNames_(otherNames)
{
	const bool backtrack = output_.backtrack;
	const QStringList patterns = filenamePatterns();
	pattern_ = patterns.first();
	if (output_.filename.isEmpty()) {
		output_.filename = pattern_ + QStringLiteral(" ") + output_.name;
	}
	if (output_.format.isEmpty()) {
		output_.format = QStringLiteral("hybrid_mp4");
	}

	setWindowTitle(pluginText(editing ? (backtrack ? "EditBacktrackOutputWindowTitle" : "EditRecordOutputWindowTitle")
					  : (backtrack ? "NewBacktrackOutputWindowTitle" : "NewRecordOutputWindowTitle")));
	setModal(true);
	setContentsMargins(0, 0, 0, 0);
	setMinimumSize(650, 400);

	confirm_ = new QPushButton(pluginText(editing ? (backtrack ? "SaveBacktrackOutput" : "SaveRecordOutput")
						     : (backtrack ? "CreateBacktrackOutput" : "CreateRecordOutput")));
	connect(confirm_, &QPushButton::clicked, this, &QDialog::accept);

	auto form = new QFormLayout;
	form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	form->setLabelAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	form->setSpacing(12);

	// the name; the filename follows it while it is still the pattern
	name_ = new QLineEdit(output_.name);
	filename_ = new QLineEdit(output_.filename);
	connect(name_, &QLineEdit::textEdited, this, [this](const QString &text) {
		const QString old = output_.name;
		output_.name = text;
		const QString followed = model::filenameFollowingName(output_.filename, pattern_, old, text);
		if (followed != output_.filename) {
			output_.filename = followed;
			filename_->setText(followed);
		}
		validate();
	});
	form->addRow(pluginText("OutputName"), name_);

	// the folder
	auto pathRow = new QHBoxLayout;
	path_ = new QLineEdit(output_.path);
	path_->setReadOnly(true);
	auto browse = new QPushButton(obsText("Browse"));
	browse->setProperty("themeID", "settingsButtons");
	connect(browse, &QPushButton::clicked, this, [this] {
		const QString dir = QFileDialog::getExistingDirectory(this, obsText("Basic.Settings.Output.Simple.SavePath"),
								      path_->text(),
								      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
		if (dir.isEmpty()) {
			return;
		}
		path_->setText(dir);
		output_.path = dir;
		validate();
	});
	pathRow->addWidget(path_);
	pathRow->addWidget(browse);
	form->addRow(obsText("Basic.Settings.Output.Simple.SavePath"), pathRow);

	// the filename, without characters the platform does not allow
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
	form->addRow(obsText("Basic.Settings.Output.Adv.Recording.Filename"), filename_);

	format_ = new QComboBox;
	format_->addItems(model::recordFormats());
	format_->setCurrentText(output_.format);
	connect(format_, &QComboBox::currentTextChanged, this, [this](const QString &text) {
		output_.format = text;
		validate();
	});
	form->addRow(obsText("Basic.Settings.Output.Format"), format_);

	maxTime_ = new FocusScrollSpinBox;
	maxTime_->setRange(0, 31536000);
	maxTime_->setSuffix(" s");
	maxTime_->setValue(int(output_.maxTimeSec));
	connect(maxTime_, &QSpinBox::valueChanged, this, [this](int value) {
		output_.maxTimeSec = value;
		validate();
	});
	maxSize_ = new FocusScrollSpinBox;
	maxSize_->setRange(0, 1073741824);
	maxSize_->setSuffix(" MB");
	maxSize_->setValue(int(output_.maxSizeMb));
	connect(maxSize_, &QSpinBox::valueChanged, this, [this](int value) {
		output_.maxSizeMb = value;
		validate();
	});

	// a recording splits its file only when asked; a backtrack always has its limits
	if (!backtrack) {
		split_ = new QCheckBox(obsText("Basic.Settings.Output.EnableSplitFile"));
		split_->setChecked(output_.maxSizeMb > 0 || output_.maxTimeSec > 0);
		auto splitChanged = [this] {
			maxTime_->setEnabled(split_->isChecked());
			maxSize_->setEnabled(split_->isChecked());
			validate();
		};
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
		connect(split_, &QCheckBox::checkStateChanged, this, splitChanged);
#else
		connect(split_, &QCheckBox::stateChanged, this, splitChanged);
#endif
		maxTime_->setEnabled(split_->isChecked());
		maxSize_->setEnabled(split_->isChecked());
		form->addRow(split_);
	}
	form->addRow(obsText(backtrack ? "Basic.Settings.Output.ReplayBuffer.SecondsMax" : "Basic.Settings.Output.SplitFile.Time"),
		     maxTime_);
	form->addRow(obsText(backtrack ? "Basic.Settings.Output.ReplayBuffer.MegabytesMax" : "Basic.Settings.Output.SplitFile.Size"),
		     maxSize_);

	auto controls = new QHBoxLayout;
	controls->setSpacing(0);
	controls->setContentsMargins(0, 0, 0, 0);
	controls->addWidget(confirm_, 0, Qt::AlignRight);

	auto page = new QVBoxLayout(this);
	page->addLayout(form);
	page->addLayout(controls);

	validate();
}

model::RecordOutput RecordOutputDialog::result() const
{
	model::RecordOutput out = output_;
	if (split_ && !split_->isChecked()) {
		out.maxSizeMb = 0;
		out.maxTimeSec = 0;
	}
	return out;
}

void RecordOutputDialog::validate()
{
	confirm_->setEnabled(model::recordOutputValid(result(), otherNames_));
}

} // namespace fiji::view
