#pragma once

#include "../Hardware.h"

namespace micasa {

	class Sunriset final : public Hardware {

	public:
		static const constexpr char* label = "Sunriset";
	
		Sunriset( const unsigned int id_, const Hardware::Type type_, const std::string reference_, const std::shared_ptr<Hardware> parent_ ) : Hardware( id_, type_, reference_, parent_ ) { };
		~Sunriset() { };

		void start() override;
		void stop() override;
		
		std::string getLabel() const throw() override { return Sunriset::label; };
		bool updateDevice( const Device::UpdateSource& source_, std::shared_ptr<Device> device_, bool& apply_ ) throw() override { return true; };
		nlohmann::json getJson(  bool full_ = false  ) const override;
		nlohmann::json getSettingsJson() const override;

	protected:
		std::chrono::milliseconds _work( const unsigned long int& iteration_ ) override;

	private:
		int _sunriset( int year, int month, int day, double lon, double lat, double altit, int upper_limb, double *trise, double *tset );
		//double _daylen( int year, int month, int day, double lon, double lat, double altit, int upper_limb );
		void _sunpos( double d, double *lon, double *r );
		void _sun_RA_dec( double d, double *RA, double *dec, double *r );
		double _rev180( double x );
		double _GMST0( double d );
		double _revolution( double x );

	}; // class Sunriset

}; // namespace micasa
