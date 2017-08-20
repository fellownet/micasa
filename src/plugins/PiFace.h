#pragma once

#include "../Plugin.h"

namespace micasa {

	class PiFace final : public Plugin {

		friend class PiFaceBoard;

	public:
		static const char* label;

		PiFace( const unsigned int id_, const Plugin::Type type_, const std::string reference_, const std::shared_ptr<Plugin> parent_ ) : Plugin( id_, type_, reference_, parent_ ) { };
		~PiFace() { };

		void start() override;
		void stop() override;
		std::string getLabel() const override { return PiFace::label; };

	private:
		int m_fd;

		int _Read_Write_SPI_Byte( unsigned char *data, int len );
		int _Read_MCP23S17_Register( unsigned char devId, unsigned char reg );
		int _Write_MCP23S17_Register( unsigned char devId, unsigned char reg, unsigned char data );

	}; // class PiFace

}; // namespace micasa
