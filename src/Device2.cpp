#include "Device2.h"

namespace micasa {

	using namespace nlohmann;

	// ==========
	// DeviceBase
	// ==========

	// A destructor is *always* called an thus needs to be implemented, even if it's marked as pure virtual.
	DeviceBase::~DeviceBase() {
	};

	json DeviceBase::getJson( bool full_ ) const {
		json result = json::object();
		result["test"] = "plaap";
		return result;
	};

	// =======
	// Counter
	// =======

	Counter2::~Counter2() {
	};

	// =====
	// Level
	// =====

	Level2::~Level2 () {
	};

	// ======
	// Switch
	// ======

	Switch2::~Switch2() {
	};

	// ====
	// Text
	// ====

	Text2::~Text2() {
	};

	// ======
	// Device
	// ======

	template<class D> void Device2<D>::setValue( const DeviceBase::UpdateSource& source_, const typename D::t_value& value_ ) {
		this->m_value = value_;
	};

	template<class D> json Device2<D>::getJson( bool full_ ) const {
		json result = DeviceBase::getJson( full_ );
		result["naadje"] = "kak";
		result["value"] = this->m_value;
		return result;
	};

	template class Device2<Counter2>;
	template class Device2<Level2>;
	template class Device2<Switch2>;
	template class Device2<Text2>;

} // namespace micasa