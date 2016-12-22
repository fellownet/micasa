#pragma once

#include "../Device.h"

namespace micasa {

	class Text final : public Device {

	public:
		typedef std::string t_value;
		static const Device::Type type = Device::Type::TEXT;
		
		Text( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_ ) : Device( hardware_, id_, reference_, label_ ) { };
		const Device::Type getType() const { return Text::type; };
		
		void start() override;
		void stop() override;
		bool updateValue( const unsigned int& source_, const t_value& value_ );
		const t_value& getValue() const { return this->m_value; };
		json getJson() const override;
		const std::chrono::milliseconds _work( const unsigned long int& iteration_ );

	private:
		t_value m_value = "";

	}; // class Text

}; // namespace micasa
