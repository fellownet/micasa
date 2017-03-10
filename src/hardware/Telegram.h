#pragma once

#include "../Hardware.h"

namespace micasa {

	class Telegram final : public Hardware {

	public:
		static const constexpr char* label = "Telegram Bot";
	
		Telegram( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~Telegram() { };

		void start() override;
		void stop() override;
		
		std::string getLabel() const throw() override;
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) override;
		nlohmann::json getJson( bool full_ = false ) const override;
		nlohmann::json getSettingsJson() const override;

	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) override;
		
	private:
		std::string m_username;
		int m_lastUpdateId = -1;
		volatile bool m_acceptMode = false;

		void _processIncomingMessage( const nlohmann::json& message_ );

	}; // class PiFace

}; // namespace micasa
