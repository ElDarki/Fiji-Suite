#pragma once

// BRIDGE: the optional link to a Fiji-chan bot, put together. The transport moves frames, the BridgeAcl translates
// them, the LinkStateMachine says where the link stands. Built once by the plugin's composition (fiji-suite.cpp) on
// the UI thread. With no address set nothing runs, and the plugin works on its own (STANDALONE).

#include "bridge-acl.hpp"
#include "link-transport.hpp"

#include <QObject>
#include <atomic>
#include <functional>
#include <memory>

namespace fiji::controller {
class CommandDispatcher;
}
namespace fiji::state {
class LinkStateMachine;
}

namespace fiji::bridge {

class BotLink : public QObject {
	Q_OBJECT

public:
	BotLink(controller::CommandDispatcher &dispatcher, state::LinkStateMachine &link, const QString &appVersion,
		std::function<void(const QString &)> log, QObject *parent = nullptr);
	~BotLink() override;

	// UI thread. An empty address turns the link off; a new one restarts it.
	void setAddress(const QString &address);
	void shutdown();

	BridgeAcl &acl() { return acl_; }
	state::LinkStateMachine &state() { return link_; }

	// Thread-safe: whether the link is LINKED, for code on other threads (the local deck server).
	bool linkedNow() const { return linkedNow_; }

	// The one link of this plugin (set by the composition), for the legacy code that reports events.
	static BotLink *instance();
	static void setInstance(BotLink *link);

private:
	state::LinkStateMachine &link_;
	BridgeAcl acl_;
	std::unique_ptr<LinkTransport> transport_;
	std::atomic<bool> linkedNow_{false};
};

} // namespace fiji::bridge
