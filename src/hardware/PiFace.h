#pragma once

#include "../Hardware.h"

namespace micasa {

	class PiFace final : public Hardware {

		friend class PiFaceBoard;

	public:
		static const constexpr char* label = "PiFace";
	
		PiFace( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~PiFace() { };

		void start() override;
		void stop() override;
		
		std::string getLabel() const throw() override { return PiFace::label; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) throw() override { return true; };

	private:
		int m_fd;

		int _Read_Write_SPI_Byte( unsigned char *data, int len );
		int _Read_MCP23S17_Register( unsigned char devId, unsigned char reg );
		int _Write_MCP23S17_Register( unsigned char devId, unsigned char reg, unsigned char data );

	}; // class PiFace

}; // namespace micasa
