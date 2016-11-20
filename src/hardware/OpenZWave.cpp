#ifdef WITH_OPENZWAVE

#include "OpenZWave.h"

// OpenZWave includes
#include "Options.h"
#include "Manager.h"

namespace micasa {

	std::string OpenZWave::toString() const {
		return this->m_name;
	};

}; // namespace micasa

#endif // WITH_OPENZWAVE
