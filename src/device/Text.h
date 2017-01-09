#pragma once

#include "../Device.h"

namespace micasa {

	class Text final : public Device {

	public:
		typedef std::string t_value;
		static const Device::Type type = Device::Type::TEXT;
		
		Text( std::shared_ptr<Hardware> hardware_, const unsigned int id_, const std::string reference_, std::string label_ ) : Device( hardware_, id_, reference_, label_ ) { };

		Device::Type getType() const override { return Text::type; };
		
		void start() override;
		void stop() override;
		bool updateValue( const unsigned int& source_, const t_value& value_ );
		t_value getValue() const { return this->m_value; };
		t_value getPreviousValue() const { return this->m_previousValue; };
		json getJson( bool full_ = false ) const override;

	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) override;

	private:
		t_value m_value = "";
		t_value m_previousValue = "";

	}; // class Text

}; // namespace micasa
