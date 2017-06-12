// https://github.com/piface/libmcp23s17/blob/master/src/mcp23s17.c
// http://wiringpi.com/
// http://wiringpi.com/reference/priority-interrupts-and-threads/

#include <fcntl.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "PiFace.h"
#include "PiFaceBoard.h"
#include "PiFace/MCP23x17.h"

#include "../Logger.h"
#include "../Controller.h"

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

namespace micasa {

	extern std::unique_ptr<Controller> g_controller;
	
	const char* PiFace::label = "PiFace";

	void PiFace::start() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		Hardware::start();

		// First the SPI device is openend.
		if ( ( this->m_fd = open( "/dev/spidev0.0", O_RDWR ) ) >= 0 ) {
			unsigned char spiMode = 0;
			unsigned char spiBPW = 8;
			int speed = 4000000;

			if (
				ioctl( this->m_fd, SPI_IOC_WR_MODE, &spiMode ) >= 0
				&& ioctl( this->m_fd, SPI_IOC_WR_BITS_PER_WORD, &spiBPW ) >= 0
				&& ioctl( this->m_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed ) >= 0
			) {
				Logger::log( Logger::LogLevel::VERBOSE, this, "SPI device opened successfully." );
				int detected = 0;

				// Once the SPI device is opened we need to detect the number of boards present. This is done by writing
				// to all possible addresses and read them back to determine if they're present.
				int devId;
				for ( devId = 0; devId < 4; devId++ ) {
					this->_Write_MCP23S17_Register( devId, MCP23x17_IOCON, IOCON_INIT | IOCON_HAEN );
					this->_Write_MCP23S17_Register( devId, MCP23x17_IOCONB, IOCON_INIT | IOCON_HAEN );
				}
				for ( devId = 0; devId < 4; devId++ ) {
					unsigned char read_iocon = this->_Read_MCP23S17_Register( devId, MCP23x17_IOCON );
					unsigned char read_ioconb = this->_Read_MCP23S17_Register( devId, MCP23x17_IOCONB );
					if (
						read_iocon == ( IOCON_INIT | IOCON_HAEN )
						&& read_ioconb == ( IOCON_INIT | IOCON_HAEN )
					) {
						g_controller->declareHardware(
							Hardware::Type::PIFACE_BOARD,
							std::to_string( devId ),
							this->shared_from_this(),
							{ },
							true
						)->start();
						detected++;
					}
				}

				if ( detected > 0 ) {
					this->setState( Hardware::State::READY );
				} else {
					Logger::log( Logger::LogLevel::ERROR, this, "No PiFaces were found." );
					this->setState( Hardware::State::FAILED );
					close( this->m_fd );
				}
			} else {
				Logger::log( Logger::LogLevel::ERROR, this, "SPI device configuration failure." );
				this->setState( Hardware::State::FAILED );
				close( this->m_fd );
			}
		} else {
			Logger::log( Logger::LogLevel::ERROR, this, "Unable to open SPI device." );
			this->setState( Hardware::State::FAILED );
		}
	};
	
	void PiFace::stop() {
		Logger::log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		if ( this->getState() == Hardware::State::READY ) {
			close( this->m_fd );
		}
		Hardware::stop();
	};

	int PiFace::_Read_Write_SPI_Byte( unsigned char *data, int len ) {
		struct spi_ioc_transfer spi;
		memset( &spi, 0, sizeof(spi) );

		spi.tx_buf        = (unsigned long)data;
		spi.rx_buf        = (unsigned long)data;
		spi.len           = len;
		spi.delay_usecs   = 0;
		spi.speed_hz      = 4000000;
		spi.bits_per_word = 8;

		return ioctl( this->m_fd, SPI_IOC_MESSAGE( 1 ), &spi );
	};

	int PiFace::_Read_MCP23S17_Register( unsigned char devId, unsigned char reg ) {
		unsigned char spiData[4];
		spiData[0] = CMD_READ | ( ( devId & 7 ) << 1 );
		spiData[1] = reg;
		this->_Read_Write_SPI_Byte( spiData, 3 );
		return spiData[2];
	};

	int PiFace::_Write_MCP23S17_Register( unsigned char devId, unsigned char reg, unsigned char data ) {
		unsigned char spiData[4];
		spiData[0] = CMD_WRITE | ( ( devId & 7 ) << 1 );
		spiData[1] = reg;
		spiData[2] = data;
		return this->_Read_Write_SPI_Byte( spiData, 3 );
	};

}; // namespace micasa
