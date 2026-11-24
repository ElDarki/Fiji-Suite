#pragma once

// LocalStore in the plugin's config folder (obs_module_config_path, which the composition passes in): one JSON file
// per key ("decks" -> decks.json). Each save keeps the previous file as <key>.json.bak and swaps the new one in
// atomically, so a crash cannot truncate it. A file that cannot be read is left untouched and loads as empty.

#include "../model/local-store.hpp"

#include <QString>
#include <functional>

namespace fiji::obs {

class ObsDataStore final : public model::LocalStore {
public:
	explicit ObsDataStore(QString folder, std::function<void(const QString &)> log = {});

	QJsonObject load(const QString &key) const override;
	bool save(const QString &key, const QJsonObject &value) override;

private:
	QString path(const QString &key) const;

	QString folder_;
	std::function<void(const QString &)> log_;
};

} // namespace fiji::obs
