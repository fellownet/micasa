#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <queue>

#include <sys/ioctl.h>
#include <termios.h>

#include "Scheduler.h"

void micasa_serial_signal_handler( int signal_ );

namespace micasa {

	class Serial {

		friend void (::micasa_serial_signal_handler)( int signal_ );

	public:
		typedef std::function<void( const unsigned char* data_, const size_t length_ )> t_callback;

		class SerialException: public std::logic_error {
		public:
			using logic_error::logic_error;
		}; // class SerialException

		enum CharacterSize {
			CHAR_SIZE_5 = CS5, // 5 bit characters
			CHAR_SIZE_6 = CS6, // 6 bit characters
			CHAR_SIZE_7 = CS7, // 7 bit characters
			CHAR_SIZE_8 = CS8, // 8 bit characters
			CHAR_SIZE_DEFAULT = CHAR_SIZE_8
		}; // enum CharacterSize

		enum StopBits {
			STOP_BITS_1, // 1 stop bit
			STOP_BITS_2, // 2 stop bits
			STOP_BITS_DEFAULT = STOP_BITS_1
		}; // enum StopBits

		enum Parity {
			PARITY_EVEN, // even parity
			PARITY_ODD,  // odd parity
			PARITY_NONE, // no parity i.e. parity checking disabled
			PARITY_DEFAULT = PARITY_NONE
		}; // enum Parity

		enum FlowControl {
			FLOW_CONTROL_HARD,
			FLOW_CONTROL_SOFT,
			FLOW_CONTROL_NONE,
			FLOW_CONTROL_DEFAULT = FLOW_CONTROL_NONE
		}; // enum FlowControl

		Serial( const std::string& port_, const unsigned int baudRate_, const CharacterSize charSize_, const Parity parity_, const StopBits stopBits_, const FlowControl flowControl_, std::shared_ptr<t_callback> callback_ );
		~Serial();

		void open();
		void close();
		void write( const unsigned char* data_, const size_t length_ );

		void setModemControlLine( const int modemLine_, const bool lineState_ );
		bool getModemControlLine( const int modemLine_ ) const;

		inline void setDtr( const bool dtr_ ) { this->setModemControlLine( TIOCM_DTR, dtr_ ); };
		inline bool getDtr() const { return this->getModemControlLine( TIOCM_DTR ); };
		inline void setRts( const bool rts_ ) { this->setModemControlLine( TIOCM_RTS, rts_ ); };
		inline bool getRts() const { return this->getModemControlLine( TIOCM_RTS ); };
		inline void setCts( const bool cts_ ) { this->setModemControlLine( TIOCM_CTS, cts_ ); };
		inline bool getCts() const { return this->getModemControlLine( TIOCM_CTS ); };

	protected:

	private:
		static Scheduler g_scheduler;
		static std::vector<Serial*> g_serialInstances;
		static std::mutex g_serialInstancesMutex;

		const std::string m_port;
		const unsigned int m_baudRate;
		const CharacterSize m_charSize;
		const Parity m_parity;
		const StopBits m_stopBits;
		const FlowControl m_flowControl;
		const std::shared_ptr<t_callback> m_callback;

		int m_fd;
		mutable std::mutex m_fdMutex;
		termios m_oldSettings;

		void _signalReceived();

	}; // class Serial

}; // namespace micasa
