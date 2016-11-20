#ifdef WITH_OPENZWAVE

#include "OpenZWaveNode.h"

namespace micasa {

	std::string OpenZWaveNode::toString() const {
		return this->m_name;
	}

}; // namespace micasa

#endif // WITH_OPENZWAVE
