#include "bot-link.hpp"

#include "../state/link-state.hpp"

#include <QMetaObject>

namespace fiji::bridge {

namespace {
BotLink *current = nullptr;
}

BotLink *BotLink::instance()
{
	return current;
}

void BotLink::setInstance(BotLink *link)
{
	current = link;
}

BotLink::BotLink(controller::CommandDispatcher &dispatcher, state::LinkStateMachine &link, const QString &appVersion,
		 std::function<void(const QString &)> log, QObject *parent)
	: QObject(parent),
	  link_(link),
	  acl_(dispatcher, link, appVersion, this)
{
	LinkTransport::Callbacks callbacks;
	callbacks.log = log;
	callbacks.opened = [this] { acl_.onOpen(); };
	callbacks.frame = [this](const QByteArray &frame) { acl_.onFrame(frame); };
	callbacks.closed = [this] {
		const bool wasLinked = acl_.linked();
		acl_.onClosed(); // a linked connection: LINKED -> CONNECTING (dropped)
		if (!wasLinked) {
			QMetaObject::invokeMethod(
				this,
				[this] {
					link_.failed(state::LinkReason::Refused,
						     QStringLiteral("The bot closed the connection before the handshake"));
				},
				Qt::QueuedConnection);
		}
	};
	callbacks.attemptFailed = [this](const QString &error) {
		QMetaObject::invokeMethod(
			this, [this, error] { link_.failed(state::LinkReason::Refused, error); }, Qt::QueuedConnection);
	};
	transport_ = std::make_unique<LinkTransport>(std::move(callbacks),
						     QStringLiteral("fiji-suite/%1").arg(appVersion).toStdString());
	acl_.send = [this](const QByteArray &frame) { transport_->send(frame); };
	acl_.refuse = [this] { transport_->stopRetrying(); };
	connect(&link_, &state::LinkStateMachine::stateChanged, this,
		[this](state::LinkStateId id) { linkedNow_ = id == state::LinkStateId::LINKED; });
}

BotLink::~BotLink()
{
	shutdown();
	if (current == this) {
		current = nullptr;
	}
}

void BotLink::setAddress(const QString &address)
{
	transport_->stop();
	acl_.onClosed();
	const auto trimmed = address.trimmed();
	if (trimmed.isEmpty()) {
		link_.addressCleared();
		return;
	}
	link_.addressSet(trimmed);
	if (!LinkTransport::validAddress(trimmed)) {
		link_.failed(state::LinkReason::BadAddress, QStringLiteral("The address starts with ws:// or wss://"));
		return;
	}
	transport_->start(trimmed.toStdString());
}

void BotLink::shutdown()
{
	if (transport_) {
		transport_->stop();
	}
}

} // namespace fiji::bridge
