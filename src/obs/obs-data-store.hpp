#pragma once

// LocalStore backed by obs_data_t JSON files in the plugin's config folder (obs_module_config_path), one file per
// key, written with obs_data_save_json_safe. The .cpp is the only place that includes obs.h for it.
// Scaffold: declaration only.

#include "../model/local-store.hpp"

namespace fiji::obs {

class ObsDataStore final : public model::LocalStore {
public:
	explicit ObsDataStore(QString folder);

	QJsonObject load(const QString &key) const override;
	bool save(const QString &key, const QJsonObject &value) override;

private:
	QString folder_;
};

} // namespace fiji::obs
