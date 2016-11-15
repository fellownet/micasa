#pragma once

#include "Device.h"

namespace micasa {

	class Event {

	public:
		typedef enum {
			DEVICE = 1,
			SCRIPT,
			TIMER,
		} Origin;

	private:
		unsigned long int m_sequence;
		Origin m_origin;
		Device* m_device;

	}; // class Event

}; // namespace micasa
