// libserial example
// https://github.com/crayzeewulf/libserial

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>
#include <string.h>
#include <iostream>
#include <thread>
#include <sys/types.h>

#ifdef _DEBUG
	#include <cassert>
#endif // _DEBUG

#include "Serial.h"

#define SERIAL_READ_BUFFER_SIZE 65 * 1024

void micasa_serial_signal_handler( int signal_ ) {
	if ( signal_ == SIGIO ) {
		// Iterate through all the serial instances that are listening for data.
		std::lock_guard<std::mutex> lock( micasa::Serial::g_serialInstancesMutex );
		for ( auto serialIt = micasa::Serial::g_serialInstances.begin(); serialIt != micasa::Serial::g_serialInstances.end(); serialIt++ ) {
			(*serialIt)->_signalReceived();
		}
	}
}

namespace micasa {

	std::vector<Serial*> Serial::g_serialInstances;
	std::mutex Serial::g_serialInstancesMutex;

	Serial::Serial( const std::string& port_, const unsigned int baudRate_, const CharacterSize charSize_, const Parity parity_, const StopBits stopBits_, const FlowControl flowControl_, std::shared_ptr<t_callback> callback_ )
		: m_port( port_ ), m_baudRate( baudRate_ ), m_charSize( charSize_ ), m_parity( parity_ ), m_stopBits( stopBits_ ), m_flowControl( flowControl_ ), m_callback( callback_ ) {
	};
	
	Serial::~Serial() {
		// Close the serial port if it is open.
		if ( this->m_fd > -1 ) {
			this->close();
		}
	};

	void Serial::open() {
		std::lock_guard<std::mutex> lock( this->m_fdMutex );

		if ( this->m_fd > -1 ) {
			throw Serial::SerialException( "Port already open." );
		}
		this->m_fd = ::open( this->m_port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK );
		if ( this->m_fd < 0 ) {
			throw Serial::SerialException( "Open port failed." );
		}

		// Add ourselves to the list of serial instances that receive io signals. Install the signal handler
		// if we're the first.
		{
			std::lock_guard<std::mutex> lock( g_serialInstancesMutex );
			g_serialInstances.push_back( this );
			if ( g_serialInstances.size() == 1 ) {
				signal( SIGIO, micasa_serial_signal_handler );
			}
		}
	
		// Direct all SIGIO and SIGURG signals for the port to the current process and enable
		// asynchronous I/O with the serial port.
		if (
			fcntl( this->m_fd, F_SETOWN, getpid() ) < 0
			|| fcntl( this->m_fd, F_SETFL, FASYNC ) < 0
		) {
			this->close();
			throw Serial::SerialException( "Unable to configure port." );
		}
		
		// Save the current settings of the serial port so they can be  restored when the serial port
		// is closed.
		if ( tcgetattr( this->m_fd, &this->m_oldSettings ) < 0 ) {
			this->close();
			throw Serial::SerialException( "Unable to retrieve port settings." );
		}

		// Flush the input buffer associated with the port.
		if ( tcflush( this->m_fd, TCIFLUSH ) < 0 ) {
			this->close();
			throw Serial::SerialException( "Unable to flush port settings." );
		}
		
		// Start assembling the new port settings.
		termios settings;
		bzero( &settings, sizeof( settings ) );
		
		// Enable the receiver (CREAD) and ignore modem control lines (CLOCAL).
		settings.c_cflag |= CREAD | CLOCAL;
		
		// Set the VMIN and VTIME parameters to zero by default. VMIN is the minimum number of characters
		// for non-canonical read and VTIME is the timeout in deciseconds for non-canonical read. Setting
		// both of these parameters to zero implies that a read will return immediately only giving the
		// currently available characters.
		settings.c_cc[VMIN] = 0;
		settings.c_cc[VTIME] = 0;

		// Set the baud rate for both input and output.
		speed_t baudrate = B57600;
		switch( this->m_baudRate ) {
			case 9600: baudrate = B9600; break;
			case 19200: baudrate = B19200; break;
			case 38400: baudrate = B38400; break;
			case 57600: baudrate = B57600; break;
			case 115200: baudrate = B115200; break;
			case 230400: baudrate = B230400; break;
			default:
				throw Serial::SerialException( "Unsupported baudrate." );
				break;
		}
		if (
			cfsetispeed( &settings, baudrate ) < 0
			|| cfsetospeed( &settings, baudrate ) < 0
		) {
			throw Serial::SerialException( "Unsupported baudrate." );
		}
		
		// Set the character size.
		settings.c_cflag &= ~CSIZE;
		settings.c_cflag |= this->m_charSize;
		
		// Set the parity.
		switch( this->m_parity ) {
			case Parity::PARITY_EVEN:
				settings.c_cflag |= PARENB;
				settings.c_cflag &= ~PARODD;
				settings.c_iflag |= INPCK;
				break;
			case Parity::PARITY_ODD:
				settings.c_cflag |= ( PARENB | PARODD );
				settings.c_iflag |= INPCK;
				break;
			case Parity::PARITY_NONE:
				settings.c_cflag &= ~(PARENB);
				settings.c_iflag |= IGNPAR;
				break;
			default:
				throw Serial::SerialException( "Invalid parity." );
				break;
		}
		
		// Set the number of stop bits.
		switch( this->m_stopBits ) {
			case StopBits::STOP_BITS_1:
				settings.c_cflag &= ~(CSTOPB);
				break;
			case StopBits::STOP_BITS_2:
				settings.c_cflag |= CSTOPB;
				break;
			default:
				throw Serial::SerialException( "Invalid stopbits." );
		}
		
		// Set the flow control.
		switch( this->m_flowControl ) {
			case FlowControl::FLOW_CONTROL_HARD:
				settings.c_cflag |= CRTSCTS;
				break;
			case FlowControl::FLOW_CONTROL_NONE:
				settings.c_cflag &= ~(CRTSCTS);
				break;
			default:
				throw Serial::SerialException( "Invalid flow control." );
		}

		// Write the assembled settings to the port.
		if ( tcsetattr( this->m_fd, TCSANOW, &settings ) < 0 ) {
			this->close();
			throw Serial::SerialException( "Unable to set port settings." );
		}
	}

	void Serial::close() {
		std::lock_guard<std::mutex> lock( this->m_fdMutex );
	
		if ( this->m_fd < 0 ) {
			throw Serial::SerialException( "Port not open." );
		}

		// Remove ourselves from the list of serial instances that receive io signals. Remove the signal
		// handler if this was the last.
		{
			std::lock_guard<std::mutex> lock( g_serialInstancesMutex );
			auto serialIt = g_serialInstances.begin();
			for ( ; serialIt != g_serialInstances.end(); serialIt++ ) {
				if ( *serialIt == this ) {
					g_serialInstances.erase( serialIt );
					break;
				}
			}
			if ( g_serialInstances.size() == 0 ) {
				signal( SIGIO, SIG_DFL );
			}
		}

		// Restore old settings on the port.
		tcsetattr( this->m_fd, TCSANOW, &this->m_oldSettings );
	
		::close( this->m_fd );
		this->m_fd = -1;
	};

	void Serial::write( const unsigned char* data_, const size_t length_ ) {
		std::lock_guard<std::mutex> lock( this->m_fdMutex );
	
		if ( this->m_fd < 0 ) {
			throw Serial::SerialException( "Port not open." );
		}
	
		int written = -1;
		do {
			written = ::write( this->m_fd, data_, length_ );
		} while (
			written < 0
			&& EAGAIN == errno
		);

		if ( written < 0 ) {
			throw Serial::SerialException( strerror( errno ) );
		}
	};

	void Serial::setModemControlLine( const int modemLine_, const bool lineState_ ) {
		std::lock_guard<std::mutex> lock( this->m_fdMutex );
	
		if ( this->m_fd < 0 ) {
			throw Serial::SerialException( "Port not open." );
		}
		
		// Set or unset the specified bit according to the value of lineState_.
		int ioctlResult = -1;
		if ( true == lineState_ ) {
			int lineMask = modemLine_;
			ioctlResult = ioctl( this->m_fd, TIOCMBIS, &lineMask );
		} else {
			int resetLineMask = modemLine_;
			ioctlResult = ioctl( this->m_fd, TIOCMBIC, &resetLineMask );
		}

		if ( -1 == ioctlResult ) {
			throw Serial::SerialException( strerror( errno ) );
		}
	};

	bool Serial::getModemControlLine( const int modemLine_ ) const {
		std::lock_guard<std::mutex> lock( this->m_fdMutex );
	
		if ( this->m_fd < 0 ) {
			throw Serial::SerialException( "Port not open." );
		}

		int state = 0;
		if ( -1 == ioctl( this->m_fd, TIOCMGET, &state ) ) {
			throw Serial::SerialException( strerror( errno ) );
		}

		return ( state & modemLine_ );
	};

	void Serial::_signalReceived() {
		// Read all the data into the buffer. This is done in a separate thread to prevent stalling
		// other serial ports which also might have data available.
		std::thread( [this]{
			std::lock_guard<std::mutex> fdLock( this->m_fdMutex );

			// Determine if it was our serial port that received data.
			int bytesAvailable = 0 ;
			if ( ioctl( this->m_fd, FIONREAD, &bytesAvailable ) < 0 ) {
				return;
			}
			
			unsigned char data[bytesAvailable];
			int length;
			for ( length = 0; length < bytesAvailable; length++ ) {
				unsigned char byte;
				if ( read( this->m_fd, &byte, 1 ) > 0 ) {
					data[length] = byte;
				} else {
					break;
				}
			}

			// Call the associated callback to inform listeners of new data.
			if ( this->m_callback != nullptr ) {
				(*this->m_callback)( data, length );
			}

		} ).detach();
	};

} // namespace micasa
