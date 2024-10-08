#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <cmath>
#include "qt-wrappers.hpp"
#include "obs-app.hpp"
#include "adv-audio-control.hpp"
#include "window-basic-main.hpp"

#ifndef NSEC_PER_MSEC
#define NSEC_PER_MSEC 1000000
#endif

#define MIN_DB -96.0
#define MAX_DB 26.0

OBSAdvAudioCtrl::OBSAdvAudioCtrl(QGridLayout *, obs_source_t *source_)
	: source(source_)
{
	QHBoxLayout *hlayout;
	signal_handler_t *handler = obs_source_get_signal_handler(source);
	QString sourceName = QT_UTF8(obs_source_get_name(source));
	float vol = obs_source_get_volume(source);
	uint32_t flags = obs_source_get_flags(source);
	uint32_t mixers = obs_source_get_audio_mixers(source);

	activeContainer = new QWidget();
	forceMonoContainer = new QWidget();
	mixerContainer = new QWidget();
	balanceContainer = new QWidget();
	labelL = new QLabel();
	labelR = new QLabel();
	iconLabel = new QLabel();
	nameLabel = new QLabel();
	active = new QLabel();
	stackedWidget = new QStackedWidget();
	volume = new QDoubleSpinBox();
	percent = new QSpinBox();
	forceMono = new QCheckBox();
	balance = new BalanceSlider();
	if (obs_audio_monitoring_available())
		monitoringType = new QCheckBox();
	syncOffset = new QSpinBox();
	for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
		mixer[i] = new QCheckBox(QString::number(i + 1));
	}

	activateSignal.Connect(handler, "activate", OBSSourceActivated, this);
	deactivateSignal.Connect(handler, "deactivate", OBSSourceDeactivated,
				 this);
	volChangedSignal.Connect(handler, "volume", OBSSourceVolumeChanged,
				 this);
	syncOffsetSignal.Connect(handler, "audio_sync", OBSSourceSyncChanged,
				 this);
	flagsSignal.Connect(handler, "update_flags", OBSSourceFlagsChanged,
			    this);
	if (obs_audio_monitoring_available())
		monitoringTypeSignal.Connect(handler, "audio_monitoring",
					     OBSSourceMonitoringTypeChanged,
					     this);
	mixersSignal.Connect(handler, "audio_mixers", OBSSourceMixersChanged,
			     this);
	balChangedSignal.Connect(handler, "audio_balance",
				 OBSSourceBalanceChanged, this);

	hlayout = new QHBoxLayout();
	hlayout->setContentsMargins(0, 0, 0, 0);
	activeContainer->setLayout(hlayout);
	hlayout = new QHBoxLayout();
	hlayout->setContentsMargins(0, 0, 0, 0);
	forceMonoContainer->setLayout(hlayout);
	hlayout = new QHBoxLayout();
	hlayout->setContentsMargins(0, 0, 0, 0);
	mixerContainer->setLayout(hlayout);
	hlayout = new QHBoxLayout();
	hlayout->setContentsMargins(0, 0, 0, 0);
	balanceContainer->setLayout(hlayout);
	balanceContainer->setFixedWidth(150);

	labelL->setText("L");

	labelR->setText("R");

	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());

	QIcon sourceIcon = main->GetSourceIcon(obs_source_get_id(source));
	QPixmap pixmap = sourceIcon.pixmap(QSize(16, 16));
	iconLabel->setPixmap(pixmap);
	iconLabel->setFixedSize(16, 16);
	iconLabel->setStyleSheet("background: none");

	nameLabel->setText(sourceName);
	nameLabel->setAlignment(Qt::AlignVCenter);

	bool isActive = obs_source_active(source);
	active->setText(isActive ? QTStr("Basic.Stats.Status.Active")
				 : QTStr("Basic.Stats.Status.Inactive"));
	if (isActive)
		setThemeID(active, "error");
	activeContainer->layout()->addWidget(active);
	activeContainer->layout()->setAlignment(active, Qt::AlignVCenter);
	activeContainer->setFixedWidth(120);

	volume->setMinimum(MIN_DB - 0.1);
	volume->setMaximum(MAX_DB);
	volume->setSingleStep(0.1);
	volume->setDecimals(1);
	volume->setSuffix(" dB");
	volume->setValue(obs_mul_to_db(vol));
	volume->setFixedWidth(100);
	volume->setAccessibleName(
		QTStr("Basic.AdvAudio.VolumeSource").arg(sourceName));

	if (volume->value() < MIN_DB) {
		volume->setSpecialValueText("-inf dB");
		volume->setAccessibleDescription("-inf dB");
	}

	percent->setMinimum(0);
	percent->setMaximum(2000);
	percent->setSuffix("%");
	percent->setValue((int)(obs_source_get_volume(source) * 100.0f));
	percent->setFixedWidth(100);
	percent->setAccessibleName(
		QTStr("Basic.AdvAudio.VolumeSource").arg(sourceName));

	stackedWidget->addWidget(volume);
	stackedWidget->addWidget(percent);

	VolumeType volType = (VolumeType)config_get_int(
		GetGlobalConfig(), "BasicWindow", "AdvAudioVolumeType");

	SetVolumeWidget(volType);

	forceMono->setChecked((flags & OBS_SOURCE_FLAG_FORCE_MONO) != 0);
	forceMono->setAccessibleName(
		QTStr("Basic.AdvAudio.MonoSource").arg(sourceName));

	forceMonoContainer->layout()->addWidget(forceMono);
	forceMonoContainer->layout()->setAlignment(forceMono, Qt::AlignVCenter);
	forceMonoContainer->setFixedWidth(50);

	balance->setOrientation(Qt::Horizontal);
	balance->setMinimum(0);
	balance->setMaximum(100);
	balance->setTickPosition(QSlider::TicksAbove);
	balance->setTickInterval(50);
	balance->setAccessibleName(
		QTStr("Basic.AdvAudio.BalanceSource").arg(sourceName));

	const char *speakers =
		config_get_string(main->Config(), "Audio", "ChannelSetup");

	if (strcmp(speakers, "Mono") == 0)
		balance->setEnabled(false);
	else
		balance->setEnabled(true);

	float bal = obs_source_get_balance_value(source) * 100.0f;
	balance->setValue((int)bal);

	int64_t cur_sync = obs_source_get_sync_offset(source);
	syncOffset->setMinimum(-950);
	syncOffset->setMaximum(20000);
	syncOffset->setSuffix(" ms");
	syncOffset->setValue(int(cur_sync / NSEC_PER_MSEC));
	syncOffset->setFixedWidth(100);
	syncOffset->setAccessibleName(
		QTStr("Basic.AdvAudio.SyncOffsetSource").arg(sourceName));

	if (obs_audio_monitoring_available()) {
		int mt = (int)obs_source_get_monitoring_type(source);
		monitoringType->setChecked(mt != 0);
		monitoringType->setAccessibleName(
			QTStr("Basic.AdvAudio.MonitoringSource")
				.arg(sourceName));
	}

	char *configName = new char[16];
	for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
		QPointer<QCheckBox> mixer1 = mixer[i];
		snprintf(configName, 16, "Track%iName", i + 1);
		const char *name1 =
			config_get_string(main->Config(), "AdvOut", configName);
		if (name1) {
			mixer1->setText(name1);
		}
		mixer1->setChecked(mixers & (1 << i));
		mixer1->setStyleSheet(mixer1->isChecked() ? "color: #00a000;"
							  : "");
		mixer1->setAccessibleName(
			QTStr("Basic.Settings.Output.Adv.Audio.Track1"));
	}
	delete[] configName;

	speaker_layout sl = obs_source_get_speaker_layout(source);

	if (sl == SPEAKERS_STEREO) {
		balanceContainer->layout()->addWidget(labelL);
		balanceContainer->layout()->addWidget(balance);
		balanceContainer->layout()->addWidget(labelR);
		balanceContainer->setMaximumWidth(170);
	}

	for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
		mixerContainer->layout()->addWidget(mixer[i]);
	}

	QWidget::connect(volume, SIGNAL(valueChanged(double)), this,
			 SLOT(volumeChanged(double)));
	QWidget::connect(percent, SIGNAL(valueChanged(int)), this,
			 SLOT(percentChanged(int)));
	QWidget::connect(forceMono, SIGNAL(clicked(bool)), this,
			 SLOT(downmixMonoChanged(bool)));
	QWidget::connect(balance, SIGNAL(valueChanged(int)), this,
			 SLOT(balanceChanged(int)));
	QWidget::connect(balance, SIGNAL(doubleClicked()), this,
			 SLOT(ResetBalance()));
	QWidget::connect(syncOffset, SIGNAL(valueChanged(int)), this,
			 SLOT(syncOffsetChanged(int)));
	if (obs_audio_monitoring_available())
		QWidget::connect(monitoringType, SIGNAL(clicked(bool)), this,
				 SLOT(monitoringTypeChanged(bool)));
	for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
		QWidget::connect(mixer[i], SIGNAL(clicked(bool)), this,
				 SLOT(mixerChanged(bool)));
	}

	setObjectName(sourceName);
}

OBSAdvAudioCtrl::~OBSAdvAudioCtrl()
{
	iconLabel->deleteLater();
	nameLabel->deleteLater();
	activeContainer->deleteLater();
	stackedWidget->deleteLater();
	forceMonoContainer->deleteLater();
	balanceContainer->deleteLater();
	syncOffset->deleteLater();
	if (obs_audio_monitoring_available())
		monitoringType->deleteLater();
	mixerContainer->deleteLater();
}

void OBSAdvAudioCtrl::ShowAudioControl(QGridLayout *layout)
{
	int lastRow = layout->rowCount();
	int idx = 0;

	layout->addWidget(iconLabel, lastRow, idx++);
	layout->addWidget(nameLabel, lastRow, idx++);
	layout->addWidget(activeContainer, lastRow, idx++);
	layout->addWidget(stackedWidget, lastRow, idx++);
	layout->addWidget(forceMonoContainer, lastRow, idx++);
	layout->addWidget(balanceContainer, lastRow, idx++);
	layout->addWidget(syncOffset, lastRow, idx++);
	if (obs_audio_monitoring_available())
		layout->addWidget(monitoringType, lastRow, idx++);
	layout->addWidget(mixerContainer, lastRow, idx++);
	layout->layout()->setAlignment(mixerContainer, Qt::AlignVCenter);
	layout->setHorizontalSpacing(15);
}

/* ------------------------------------------------------------------------- */
/* OBS source callbacks */

void OBSAdvAudioCtrl::OBSSourceActivated(void *param, calldata_t *calldata)
{
	QMetaObject::invokeMethod(reinterpret_cast<OBSAdvAudioCtrl *>(param),
				  "SourceActiveChanged", Q_ARG(bool, true));
	UNUSED_PARAMETER(calldata);
}

void OBSAdvAudioCtrl::OBSSourceDeactivated(void *param, calldata_t *calldata)
{
	QMetaObject::invokeMethod(reinterpret_cast<OBSAdvAudioCtrl *>(param),
				  "SourceActiveChanged", Q_ARG(bool, false));
	UNUSED_PARAMETER(calldata);
}

void OBSAdvAudioCtrl::OBSSourceFlagsChanged(void *param, calldata_t *calldata)
{
	uint32_t flags = (uint32_t)calldata_int(calldata, "flags");
	QMetaObject::invokeMethod(reinterpret_cast<OBSAdvAudioCtrl *>(param),
				  "SourceFlagsChanged", Q_ARG(uint32_t, flags));
}

void OBSAdvAudioCtrl::OBSSourceVolumeChanged(void *param, calldata_t *calldata)
{
	float volume = (float)calldata_float(calldata, "volume");
	QMetaObject::invokeMethod(reinterpret_cast<OBSAdvAudioCtrl *>(param),
				  "SourceVolumeChanged", Q_ARG(float, volume));
}

void OBSAdvAudioCtrl::OBSSourceSyncChanged(void *param, calldata_t *calldata)
{
	int64_t offset = calldata_int(calldata, "offset");
	QMetaObject::invokeMethod(reinterpret_cast<OBSAdvAudioCtrl *>(param),
				  "SourceSyncChanged", Q_ARG(int64_t, offset));
}

void OBSAdvAudioCtrl::OBSSourceMonitoringTypeChanged(void *param,
						     calldata_t *calldata)
{
	int type = calldata_int(calldata, "type");
	QMetaObject::invokeMethod(reinterpret_cast<OBSAdvAudioCtrl *>(param),
				  "SourceMonitoringTypeChanged",
				  Q_ARG(int, type));
}

void OBSAdvAudioCtrl::OBSSourceMixersChanged(void *param, calldata_t *calldata)
{
	uint32_t mixers = (uint32_t)calldata_int(calldata, "mixers");
	QMetaObject::invokeMethod(reinterpret_cast<OBSAdvAudioCtrl *>(param),
				  "SourceMixersChanged",
				  Q_ARG(uint32_t, mixers));
}

void OBSAdvAudioCtrl::OBSSourceBalanceChanged(void *param, calldata_t *calldata)
{
	int balance = (float)calldata_float(calldata, "balance") * 100.0f;
	QMetaObject::invokeMethod(reinterpret_cast<OBSAdvAudioCtrl *>(param),
				  "SourceBalanceChanged", Q_ARG(int, balance));
}

/* ------------------------------------------------------------------------- */
/* Qt event queue source callbacks */

static inline void setCheckboxState(QCheckBox *checkbox, bool checked)
{
	checkbox->blockSignals(true);
	checkbox->setChecked(checked);
	checkbox->blockSignals(false);
}

void OBSAdvAudioCtrl::SourceActiveChanged(bool isActive)
{
	if (isActive) {
		active->setText(QTStr("Basic.Stats.Status.Active"));
		setThemeID(active, "error");
	} else {
		active->setText(QTStr("Basic.Stats.Status.Inactive"));
		setThemeID(active, "");
	}
}

void OBSAdvAudioCtrl::SourceFlagsChanged(uint32_t flags)
{
	bool forceMonoVal = (flags & OBS_SOURCE_FLAG_FORCE_MONO) != 0;
	setCheckboxState(forceMono, forceMonoVal);
}

void OBSAdvAudioCtrl::SourceVolumeChanged(float value)
{
	volume->blockSignals(true);
	percent->blockSignals(true);
	volume->setValue(obs_mul_to_db(value));
	percent->setValue((int)std::round(value * 100.0f));
	percent->blockSignals(false);
	volume->blockSignals(false);
}

void OBSAdvAudioCtrl::SourceBalanceChanged(int value)
{
	balance->blockSignals(true);
	balance->setValue(value);
	balance->blockSignals(false);
}

void OBSAdvAudioCtrl::SourceSyncChanged(int64_t offset)
{
	syncOffset->blockSignals(true);
	syncOffset->setValue(offset / NSEC_PER_MSEC);
	syncOffset->blockSignals(false);
}

void OBSAdvAudioCtrl::SourceMonitoringTypeChanged(int type)
{
	monitoringType->blockSignals(true);
	monitoringType->setChecked(type != 0);
	monitoringType->blockSignals(false);
}

void OBSAdvAudioCtrl::SourceMixersChanged(uint32_t mixers)
{
	for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
		setCheckboxState(mixer[i], mixers & (1 << i));
		mixer[i]->setStyleSheet(
			mixer[i]->isChecked() ? "color: #00a000;" : "");
	}
}

/* ------------------------------------------------------------------------- */
/* Qt control callbacks */

void OBSAdvAudioCtrl::volumeChanged(double db)
{
	float prev = obs_source_get_volume(source);

	if (db < MIN_DB) {
		volume->setSpecialValueText("-inf dB");
		db = -INFINITY;
	}

	float val = obs_db_to_mul(db);
	obs_source_set_volume(source, val);

	auto undo_redo = [](const std::string &name, float val) {
		OBSSourceAutoRelease source =
			obs_get_source_by_name(name.c_str());
		obs_source_set_volume(source, val);
	};

	const char *name = obs_source_get_name(source);

	OBSBasic *main = OBSBasic::Get();
	main->undo_s.add_action(
		QTStr("Undo.Volume.Change").arg(name),
		std::bind(undo_redo, std::placeholders::_1, prev),
		std::bind(undo_redo, std::placeholders::_1, val), name, name,
		true);
}

void OBSAdvAudioCtrl::percentChanged(int percent)
{
	float prev = obs_source_get_volume(source);
	float val = (float)percent / 100.0f;

	obs_source_set_volume(source, val);

	auto undo_redo = [](const std::string &name, float val) {
		OBSSourceAutoRelease source =
			obs_get_source_by_name(name.c_str());
		obs_source_set_volume(source, val);
	};

	const char *name = obs_source_get_name(source);
	OBSBasic::Get()->undo_s.add_action(
		QTStr("Undo.Volume.Change").arg(name),
		std::bind(undo_redo, std::placeholders::_1, prev),
		std::bind(undo_redo, std::placeholders::_1, val), name, name,
		true);
}

static inline void set_mono(obs_source_t *source, bool mono)
{
	uint32_t flags = obs_source_get_flags(source);
	if (mono)
		flags |= OBS_SOURCE_FLAG_FORCE_MONO;
	else
		flags &= ~OBS_SOURCE_FLAG_FORCE_MONO;
	obs_source_set_flags(source, flags);
}

void OBSAdvAudioCtrl::downmixMonoChanged(bool val)
{
	uint32_t flags = obs_source_get_flags(source);
	bool forceMonoActive = (flags & OBS_SOURCE_FLAG_FORCE_MONO) != 0;

	if (forceMonoActive == val)
		return;

	if (val)
		flags |= OBS_SOURCE_FLAG_FORCE_MONO;
	else
		flags &= ~OBS_SOURCE_FLAG_FORCE_MONO;

	obs_source_set_flags(source, flags);

	auto undo_redo = [](const std::string &name, bool val) {
		OBSSourceAutoRelease source =
			obs_get_source_by_name(name.c_str());
		set_mono(source, val);
	};

	QString text = QTStr(val ? "Undo.ForceMono.On" : "Undo.ForceMono.Off");

	const char *name = obs_source_get_name(source);
	OBSBasic::Get()->undo_s.add_action(
		text.arg(name),
		std::bind(undo_redo, std::placeholders::_1, !val),
		std::bind(undo_redo, std::placeholders::_1, val), name, name);
}

void OBSAdvAudioCtrl::balanceChanged(int val)
{
	float prev = obs_source_get_balance_value(source);
	float bal = (float)val / 100.0f;

	if (abs(50 - val) < 10) {
		balance->blockSignals(true);
		balance->setValue(50);
		bal = 0.5f;
		balance->blockSignals(false);
	}

	obs_source_set_balance_value(source, bal);

	auto undo_redo = [](const std::string &name, float val) {
		OBSSourceAutoRelease source =
			obs_get_source_by_name(name.c_str());
		obs_source_set_balance_value(source, val);
	};

	const char *name = obs_source_get_name(source);
	OBSBasic::Get()->undo_s.add_action(
		QTStr("Undo.Balance.Change").arg(name),
		std::bind(undo_redo, std::placeholders::_1, prev),
		std::bind(undo_redo, std::placeholders::_1, bal), name, name,
		true);
}

void OBSAdvAudioCtrl::ResetBalance()
{
	balance->setValue(50);
}

void OBSAdvAudioCtrl::syncOffsetChanged(int milliseconds)
{
	int64_t prev = obs_source_get_sync_offset(source);
	int64_t val = int64_t(milliseconds) * NSEC_PER_MSEC;

	if (prev / NSEC_PER_MSEC == milliseconds)
		return;

	obs_source_set_sync_offset(source, val);

	auto undo_redo = [](const std::string &name, int64_t val) {
		OBSSourceAutoRelease source =
			obs_get_source_by_name(name.c_str());
		obs_source_set_sync_offset(source, val);
	};

	const char *name = obs_source_get_name(source);
	OBSBasic::Get()->undo_s.add_action(
		QTStr("Undo.SyncOffset.Change").arg(name),
		std::bind(undo_redo, std::placeholders::_1, prev),
		std::bind(undo_redo, std::placeholders::_1, val), name, name,
		true);
}

void OBSAdvAudioCtrl::monitoringTypeChanged(bool checked)
{
	obs_monitoring_type prev = obs_source_get_monitoring_type(source);

	obs_monitoring_type mt = (obs_monitoring_type)(checked ? 2 : 0);
	obs_source_set_monitoring_type(source, mt);

	const char *type = nullptr;

	switch (mt) {
	case OBS_MONITORING_TYPE_NONE:
		type = "none";
		break;
	case OBS_MONITORING_TYPE_MONITOR_ONLY:
		type = "monitor only";
		break;
	case OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT:
		type = "monitor and output";
		break;
	}

	const char *name = obs_source_get_name(source);
	blog(LOG_INFO, "User changed audio monitoring for source '%s' to: %s",
	     name ? name : "(null)", type);

	auto undo_redo = [](const std::string &name, obs_monitoring_type val) {
		OBSSourceAutoRelease source =
			obs_get_source_by_name(name.c_str());
		obs_source_set_monitoring_type(source, val);
	};

	OBSBasic::Get()->undo_s.add_action(
		QTStr("Undo.MonitoringType.Change").arg(name),
		std::bind(undo_redo, std::placeholders::_1, prev),
		std::bind(undo_redo, std::placeholders::_1, mt), name, name);
}

static inline void setMixer(obs_source_t *source, const int mixerIdx,
			    const bool checked)
{
	uint32_t mixers = obs_source_get_audio_mixers(source);
	uint32_t new_mixers = mixers;

	if (checked)
		new_mixers |= (1 << mixerIdx);
	else
		new_mixers &= ~(1 << mixerIdx);

	obs_source_set_audio_mixers(source, new_mixers);

	auto undo_redo = [](const std::string &name, uint32_t mixers) {
		OBSSourceAutoRelease source =
			obs_get_source_by_name(name.c_str());
		obs_source_set_audio_mixers(source, mixers);
	};

	const char *name = obs_source_get_name(source);
	OBSBasic::Get()->undo_s.add_action(
		QTStr("Undo.Mixers.Change").arg(name),
		std::bind(undo_redo, std::placeholders::_1, mixers),
		std::bind(undo_redo, std::placeholders::_1, new_mixers), name,
		name);
}

void OBSAdvAudioCtrl::mixerChanged(bool checked)
{
	int i = 0;
	QPointer box = sender();
	while (i < MAX_AUDIO_MIXES) {
		mixer[i]->setStyleSheet(
			mixer[i]->isChecked() ? "color: #00a000;" : "");
		if (mixer[i] == box)
			break;
		++i;
	}
	setMixer(source, i, checked);
}

void OBSAdvAudioCtrl::SetVolumeWidget(VolumeType type)
{
	switch (type) {
	case VolumeType::Percent:
		stackedWidget->setCurrentWidget(percent);
		break;
	case VolumeType::dB:
		stackedWidget->setCurrentWidget(volume);
		break;
	}
}

void OBSAdvAudioCtrl::SetIconVisible(bool visible)
{
	visible ? iconLabel->show() : iconLabel->hide();
}
