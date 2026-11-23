#pragma once

// STATE layer (CLAUDE.md, "MVC + State"): the bot link as an explicit State pattern. Qt Core only.
//
//   STANDALONE --address set--> CONNECTING --HELLO done--> LINKED
//        ^                         |   ^                     |
//        +--final failure----------+   +------dropped--------+
//   any state --address cleared--> STANDALONE
//
// A final failure (a bad address, a bot speaking another contract version) ends in STANDALONE with its reason. A
// passing one (refused, timed out) keeps CONNECTING, with the reason shown, while the transport tries again.
// STANDALONE is a normal, fully working mode: nothing throws or logs errors there. Twitch-dependent controls bind to
// twitchFeaturesAvailable() and are disabled with an explanation (never hidden).

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

namespace fiji::state {

enum class LinkStateId { STANDALONE, CONNECTING, LINKED };

// Why the machine is in STANDALONE, or why the last connection attempt ended.
enum class LinkReason { None, NoAddress, AddressCleared, BadAddress, Refused, VersionMismatch, Dropped };

// Final failures end in STANDALONE; the others keep CONNECTING while the transport retries.
bool isFinal(LinkReason reason);

class LinkStateMachine;

class LinkState {
public:
	virtual ~LinkState() = default;
	virtual LinkStateId id() const = 0;

	virtual void enter(LinkStateMachine &) {}
	virtual void exit(LinkStateMachine &) {}

	// Inputs. A state ignores the ones that are not transitions from it.
	virtual void onAddressSet(LinkStateMachine &, const QString & /*address*/) {}
	virtual void onAddressCleared(LinkStateMachine &) {}
	virtual void onHelloCompleted(LinkStateMachine &, const QStringList & /*accepted*/) {}
	virtual void onFailure(LinkStateMachine &, LinkReason, const QString & /*detail*/) {}
	virtual void onDropped(LinkStateMachine &) {}
};

class StandaloneState final : public LinkState {
public:
	LinkStateId id() const override { return LinkStateId::STANDALONE; }
	void onAddressSet(LinkStateMachine &machine, const QString &address) override;
};

class ConnectingState final : public LinkState {
public:
	LinkStateId id() const override { return LinkStateId::CONNECTING; }
	void onAddressSet(LinkStateMachine &machine, const QString &address) override;
	void onAddressCleared(LinkStateMachine &machine) override;
	void onHelloCompleted(LinkStateMachine &machine, const QStringList &accepted) override;
	void onFailure(LinkStateMachine &machine, LinkReason reason, const QString &detail) override;
};

class LinkedState final : public LinkState {
public:
	LinkStateId id() const override { return LinkStateId::LINKED; }
	void exit(LinkStateMachine &machine) override;
	void onAddressSet(LinkStateMachine &machine, const QString &address) override;
	void onAddressCleared(LinkStateMachine &machine) override;
	void onHelloCompleted(LinkStateMachine &machine, const QStringList &accepted) override;
	void onFailure(LinkStateMachine &machine, LinkReason reason, const QString &detail) override;
	void onDropped(LinkStateMachine &machine) override;
};

class LinkStateMachine : public QObject {
	Q_OBJECT

public:
	explicit LinkStateMachine(QObject *parent = nullptr);
	~LinkStateMachine() override;

	LinkStateId state() const;
	LinkReason lastReason() const;
	QString lastDetail() const;
	bool twitchFeaturesAvailable() const;     // true only in LINKED
	QStringList acceptedCapabilities() const; // empty unless LINKED
	bool accepts(const QString &capability) const;

	// Inputs, called on the UI thread (the transport and the bridge post them there).
	void addressSet(const QString &address);
	void addressCleared();
	void helloCompleted(const QStringList &accepted);
	void failed(LinkReason reason, const QString &detail = QString());
	void dropped();

	// Used by the states only.
	void transitionTo(std::unique_ptr<LinkState> next, LinkReason reason = LinkReason::None,
			  const QString &detail = QString());
	void setAccepted(const QStringList &accepted);
	void setReason(LinkReason reason, const QString &detail);

signals:
	void stateChanged(fiji::state::LinkStateId state);
	void reasonChanged(fiji::state::LinkReason reason, const QString &detail);
	void twitchFeaturesAvailableChanged(bool available);
	void capabilitiesChanged(const QStringList &accepted);

private:
	std::unique_ptr<LinkState> current_;
	LinkReason reason_ = LinkReason::NoAddress;
	QString detail_;
	QStringList accepted_;
};

} // namespace fiji::state
