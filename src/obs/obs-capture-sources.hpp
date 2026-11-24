#pragma once

// libobs implementation of CaptureSources: the global source_create / source_remove signals, and each capture
// source's rename / hooked / unhooked signals.

#include "capture-ports.hpp"

#include <QHash>
#include <memory>
#include <mutex>

struct calldata;
typedef struct calldata calldata_t;
struct obs_weak_source;
typedef struct obs_weak_source obs_weak_source_t;

namespace fiji::obs {

class ObsCaptureSources final : public CaptureSources {
public:
	ObsCaptureSources() = default;
	~ObsCaptureSources() override;

	void listen(Listener listener) override;
	QList<CaptureWindow> windows(const QString &uuid) override;
	void pickWindow(const QString &uuid, const QVariant &value) override;

private:
	struct Tracked {
		ObsCaptureSources *self;
		QString uuid;
		obs_weak_source_t *weak;
	};

	static void sourceCreated(void *param, calldata_t *cd);
	static void sourceRemoved(void *param, calldata_t *cd);
	static void renamed(void *param, calldata_t *cd);
	static void hooked(void *param, calldata_t *cd);
	static void unhooked(void *param, calldata_t *cd);
	void untrack(Tracked &tracked);

	Listener listener_;
	bool listening_ = false;
	std::mutex mutex_;
	QHash<QString, std::shared_ptr<Tracked>> tracked_;
};

} // namespace fiji::obs
