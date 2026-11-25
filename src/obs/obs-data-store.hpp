#pragma once

// LocalStore in the plugin's config folder (obs_module_config_path): one <key>.json file per key. Each save keeps
// the previous file as <key>.json.bak and swaps the new one in, so a crash cannot leave a half-written file.

#include "../model/local-store.hpp"

namespace fiji::obs {

class ObsDataStore final : public model::LocalStore {
public:
	explicit ObsDataStore(QString folder);

	QJsonObject load(const QString &key) const override;
	bool save(const QString &key, const QJsonObject &value) override;

	// The plugin's config folder.
	static QString configFolder();

private:
	QString path(const QString &key) const;
	QString folder_;
};

} // namespace fiji::obs
