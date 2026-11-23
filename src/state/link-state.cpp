#include "link-state.hpp"

namespace fiji::state {

bool isFinal(LinkReason reason)
{
	return reason == LinkReason::BadAddress || reason == LinkReason::VersionMismatch;
}

// ---- STANDALONE ------------------------------------------------------------------------------------
void StandaloneState::onAddressSet(LinkStateMachine &machine, const QString &)
{
	machine.transitionTo(std::make_unique<ConnectingState>());
}

// ---- CONNECTING ------------------------------------------------------------------------------------
void ConnectingState::onAddressSet(LinkStateMachine &machine, const QString &)
{
	machine.setReason(LinkReason::None, QString()); // a new address: earlier failures no longer apply
}

void ConnectingState::onAddressCleared(LinkStateMachine &machine)
{
	machine.transitionTo(std::make_unique<StandaloneState>(), LinkReason::AddressCleared);
}

void ConnectingState::onHelloCompleted(LinkStateMachine &machine, const QStringList &accepted)
{
	machine.setAccepted(accepted);
	machine.transitionTo(std::make_unique<LinkedState>());
}

void ConnectingState::onFailure(LinkStateMachine &machine, LinkReason reason, const QString &detail)
{
	if (isFinal(reason)) {
		machine.transitionTo(std::make_unique<StandaloneState>(), reason, detail);
	} else {
		machine.setReason(reason, detail);
	}
}

// ---- LINKED ----------------------------------------------------------------------------------------
void LinkedState::exit(LinkStateMachine &machine)
{
	machine.setAccepted({});
}

void LinkedState::onAddressSet(LinkStateMachine &machine, const QString &)
{
	machine.transitionTo(std::make_unique<ConnectingState>());
}

void LinkedState::onAddressCleared(LinkStateMachine &machine)
{
	machine.transitionTo(std::make_unique<StandaloneState>(), LinkReason::AddressCleared);
}

void LinkedState::onHelloCompleted(LinkStateMachine &machine, const QStringList &accepted)
{
	machine.setAccepted(accepted); // the bot restarted the negotiation
}

void LinkedState::onFailure(LinkStateMachine &machine, LinkReason reason, const QString &detail)
{
	if (isFinal(reason)) {
		machine.transitionTo(std::make_unique<StandaloneState>(), reason, detail);
	} else {
		machine.transitionTo(std::make_unique<ConnectingState>(), reason, detail);
	}
}

void LinkedState::onDropped(LinkStateMachine &machine)
{
	machine.transitionTo(std::make_unique<ConnectingState>(), LinkReason::Dropped);
}

// ---- the machine -----------------------------------------------------------------------------------
LinkStateMachine::LinkStateMachine(QObject *parent) : QObject(parent), current_(std::make_unique<StandaloneState>()) {}

LinkStateMachine::~LinkStateMachine() = default;

LinkStateId LinkStateMachine::state() const
{
	return current_->id();
}

LinkReason LinkStateMachine::lastReason() const
{
	return reason_;
}

QString LinkStateMachine::lastDetail() const
{
	return detail_;
}

bool LinkStateMachine::twitchFeaturesAvailable() const
{
	return state() == LinkStateId::LINKED;
}

QStringList LinkStateMachine::acceptedCapabilities() const
{
	return accepted_;
}

bool LinkStateMachine::accepts(const QString &capability) const
{
	return accepted_.contains(capability);
}

void LinkStateMachine::addressSet(const QString &address)
{
	current_->onAddressSet(*this, address);
}

void LinkStateMachine::addressCleared()
{
	current_->onAddressCleared(*this);
}

void LinkStateMachine::helloCompleted(const QStringList &accepted)
{
	current_->onHelloCompleted(*this, accepted);
}

void LinkStateMachine::failed(LinkReason reason, const QString &detail)
{
	current_->onFailure(*this, reason, detail);
}

void LinkStateMachine::dropped()
{
	current_->onDropped(*this);
}

void LinkStateMachine::transitionTo(std::unique_ptr<LinkState> next, LinkReason reason, const QString &detail)
{
	const bool wasAvailable = twitchFeaturesAvailable();
	const LinkStateId before = state();
	current_->exit(*this);
	current_ = std::move(next);
	current_->enter(*this);
	if (state() != before) {
		emit stateChanged(state());
	}
	setReason(reason, detail);
	if (twitchFeaturesAvailable() != wasAvailable) {
		emit twitchFeaturesAvailableChanged(twitchFeaturesAvailable());
	}
}

void LinkStateMachine::setAccepted(const QStringList &accepted)
{
	if (accepted_ != accepted) {
		accepted_ = accepted;
		emit capabilitiesChanged(accepted_);
	}
}

void LinkStateMachine::setReason(LinkReason reason, const QString &detail)
{
	if (reason_ != reason || detail_ != detail) {
		reason_ = reason;
		detail_ = detail;
		emit reasonChanged(reason_, detail_);
	}
}

} // namespace fiji::state
