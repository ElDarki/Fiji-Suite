#include "link-state.hpp"

// Scaffold: only what Qt's meta-object code needs. The transitions are implemented (with tests) as the first feature
// after the architecture review.

namespace fiji::state {

LinkStateMachine::LinkStateMachine(QObject *parent) : QObject(parent) {}

} // namespace fiji::state
