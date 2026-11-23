// Architecture scaffold check (target fiji-arch-test). Includes every header of the MVC + State layers so they keep
// compiling with Qt Core alone (no obs.h, no widgets), and pins the few constants the layers agree on.
// Exit code 0 means every check passed.

#include "../src/bridge/bot-link.hpp"
#include "../src/bridge/bridge-acl.hpp"
#include "../src/bridge/catalogue.hpp"
#include "../src/bridge/envelope.hpp"
#include "../src/bridge/link-transport.hpp"
#include "../src/controller/command-dispatcher.hpp"
#include "../src/controller/command.hpp"
#include "../src/controller/obs-command-handler.hpp"
#include "../src/model/deck-model.hpp"
#include "../src/model/local-store.hpp"
#include "../src/model/obs-status.hpp"
#include "../src/obs/obs-api.hpp"
#include "../src/obs/obs-data-store.hpp"
#include "../src/obs/request-obs-api.hpp"
#include "../src/state/link-state.hpp"

#include <cstdio>
#include <cstring>
#include <type_traits>

using namespace fiji;

static_assert(std::is_abstract_v<model::LocalStore>, "LocalStore is an interface");
static_assert(std::is_abstract_v<obs::ObsApi>, "ObsApi is a port");
static_assert(std::is_abstract_v<controller::CommandHandler>, "CommandHandler is an interface");
static_assert(std::is_abstract_v<state::LinkState>, "LinkState is the State pattern's base");
static_assert(std::is_base_of_v<model::LocalStore, obs::ObsDataStore>, "ObsDataStore implements LocalStore");
static_assert(std::is_base_of_v<obs::ObsApi, obs::RequestObsApi>, "RequestObsApi implements the OBS port");
static_assert(std::is_base_of_v<controller::CommandHandler, controller::ObsCommandHandler>, "a dispatcher handler");
static_assert(std::is_base_of_v<state::LinkState, state::StandaloneState> &&
		      std::is_base_of_v<state::LinkState, state::ConnectingState> &&
		      std::is_base_of_v<state::LinkState, state::LinkedState>,
	      "exactly the three link states derive from LinkState");

int main()
{
	int failures = 0;
	auto check = [&](bool ok, const char *what) {
		if (!ok) {
			std::printf("FAILED %s\n", what);
			failures++;
		}
	};

	// The states' transitions get their tests (tests/link-state-test.cpp) with the first feature; until then the
	// scaffold only declares them, so nothing here may instantiate a class whose methods are not defined yet.
	check(static_cast<int>(state::LinkStateId::LINKED) == 2, "three link states: STANDALONE, CONNECTING, LINKED");
	check(std::strcmp(bridge::kContractVersion, "1.2") == 0, "bridge contract version (BRIDGE_CONTRACT.md)");
	check(bridge::kAckTimeoutMs == 10000, "ACK timeout is 10 s");
	check(static_cast<int>(bridge::MessageType::SYNC_STATE) == 4, "five message types, in contract order");

	if (failures == 0)
		std::printf("architecture scaffold: all checks passed\n");
	return failures == 0 ? 0 : 1;
}
