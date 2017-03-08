// http://stjarnhimlen.se/comp/sunriset.c

#include "../Logger.h"
#include "../Database.h"

#include "../device/Switch.h"
#include "../device/Level.h"

#include "Sunriset.h"

namespace micasa {

/* A macro to compute the number of days elapsed since 2000 Jan 0.0 */
/* (which is equal to 1999 Dec 31, 0h UT)                           */

#define days_since_2000_Jan_0(y,m,d) \
    (367L*(y)-((7*((y)+(((m)+9)/12)))/4)+((275*(m))/9)+(d)-730530L)

/* Some conversion factors between radians and degrees */

#ifndef PI
 #define PI        3.1415926535897932384
#endif

#define RADEG     ( 180.0 / PI )
#define DEGRAD    ( PI / 180.0 )
#define INV360    ( 1.0 / 360.0 )

/* The trigonometric functions in degrees */

#define sind(x)  sin((x)*DEGRAD)
#define cosd(x)  cos((x)*DEGRAD)
#define tand(x)  tan((x)*DEGRAD)

#define atand(x)    (RADEG*atan(x))
#define asind(x)    (RADEG*asin(x))
#define acosd(x)    (RADEG*acos(x))
#define atan2d(y,x) (RADEG*atan2(y,x))

	extern std::shared_ptr<Logger> g_logger;
	extern std::shared_ptr<Database> g_database;

	using namespace nlohmann;

	void Sunriset::start() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Starting..." );
		this->setState( Hardware::State::READY );
		Hardware::start();
	};
	
	void Sunriset::stop() {
		g_logger->log( Logger::LogLevel::VERBOSE, this, "Stopping..." );
		Hardware::stop();
	};

	json Sunriset::getJson( bool full_ ) const {
		json result = Hardware::getJson( full_ );
		result["longitude"] = this->m_settings->get( "longitude", "" );
		result["latitude"] = this->m_settings->get( "latitude", "" );
		if ( full_ ) {
			result["settings"] = this->getSettingsJson();
		}
		return result;
	};

	json Sunriset::getSettingsJson() const {
		json result = Hardware::getSettingsJson();
		result += {
			{ "name", "longitude" },
			{ "label", "Longitude" },
			{ "type", "double" },
			{ "mandatory", true },
			{ "minimum", 0 },
			{ "maximum", 180 },
			{ "sort", 98 }
		};
		result += {
			{ "name", "latitude" },
			{ "label", "Latitude" },
			{ "type", "double" },
			{ "mandatory", true },
			{ "minimum", 0 },
			{ "maximum", 90 },
			{ "sort", 99 }
		};
		return result;
	};
	
	std::chrono::milliseconds Sunriset::_work( const unsigned long int& iteration_ ) {

		if ( ! this->m_settings->contains( { "longitude", "latitude" } ) ) {
			g_logger->log( Logger::LogLevel::ERROR, this, "Missing settings." );
			this->setState( Hardware::State::FAILED );
			return std::chrono::milliseconds( 60 * 1000 );
		}

		auto date = g_database->getQueryRow( "SELECT strftime('%%H', 'now') AS `hour`, strftime('%%M', 'now') AS `minute`, strftime('%%Y', 'now') AS `year`, strftime('%%m', 'now') AS `month`, strftime('%%d', 'now') AS `day`" );
		double rise, set;
		int rs = this->_sunriset(
			std::stoi( date["year"] ),
			std::stoi( date["month"] ),
			std::stoi( date["day"] ),
			this->m_settings->get<double>( "longitude" ),
			this->m_settings->get<double>( "latitude" ),
			-35.0/60.0,
			1,
			&rise,
			&set
		);
		double now = std::stoi( date["hour"] ) + ( std::stoi( date["minute"] ) / 100. );
		g_logger->logr( Logger::LogLevel::WARNING, this, "Sun rises %5.2fh UT, sets %5.2fh UT and now is %5.2fh", rise, set, now );

		auto device = this->declareDevice<Switch>( "daytime", "Daytime", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) }
		} );
		Switch::Option value;
		switch( rs ) {
			case 0: {
				if (
					rise < now
					&& set > now
				) {
					value = Switch::Option::ON;
				} else {
					value = Switch::Option::OFF;
				}
				break;
			}
			case +1: {
				// Sun entire day above horizon.
				value = Switch::Option::ON;
				break;
			}
			case -1: {
				// Sun entire day below horizon.
				value = Switch::Option::OFF;
				break;
			}
		}
		if ( value != device->getValueOption() ) {
			device->updateValue( Device::UpdateSource::HARDWARE, value );
		}


		// Set all statusses
		// Calculate all statusses and get the first next one and wake the hardware up after that time
		/*
		this->declareDevice<Switch>( "civil_twilight", "Civil Twilight", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) }
		} );
		this->declareDevice<Switch>( "nautical_twilight", "Nautical Twilight", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) }
		} );
		this->declareDevice<Switch>( "astronomical_twilight", "Astronomical Twilight", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) }
		} );


		this->declareDevice<Switch>( "sun_set", "Sun Set", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) }
		} );
		this->declareDevice<Level>( "day_length", "Day Length", {
			{ DEVICE_SETTING_ALLOWED_UPDATE_SOURCES, Device::resolveUpdateSource( Device::UpdateSource::CONTROLLER ) }
		} );
		*/

		return std::chrono::milliseconds( 1000 * 60 );
	};








	int Sunriset::_sunriset( int year, int month, int day, double lon, double lat, double altit, int upper_limb, double *trise, double *tset ) {
		double  d,  /* Days since 2000 Jan 0.0 (negative before) */
		sr,         /* Solar distance, astronomical units */
		sRA,        /* Sun's Right Ascension */
		sdec,       /* Sun's declination */
		sradius,    /* Sun's apparent radius */
		t,          /* Diurnal arc */
		tsouth,     /* Time when Sun is at south */
		sidtime;    /* Local sidereal time */

		int rc = 0; /* Return cde from function - usually 0 */

		/* Compute d of 12h local mean solar time */
		d = days_since_2000_Jan_0(year,month,day) + 0.5 - lon/360.0;

		/* Compute the local sidereal time of this moment */
		sidtime = this->_revolution( this->_GMST0(d) + 180.0 + lon );

		/* Compute Sun's RA, Decl and distance at this moment */
		this->_sun_RA_dec( d, &sRA, &sdec, &sr );

		/* Compute time when Sun is at south - in hours UT */
		tsouth = 12.0 - this->_rev180(sidtime - sRA)/15.0;

		/* Compute the Sun's apparent radius in degrees */
		sradius = 0.2666 / sr;

		/* Do correction to upper limb, if necessary */
		if ( upper_limb )
		altit -= sradius;

		/* Compute the diurnal arc that the Sun traverses to reach */
		/* the specified altitude altit: */
		{
		double cost;
		cost = ( sind(altit) - sind(lat) * sind(sdec) ) /
		( cosd(lat) * cosd(sdec) );
		if ( cost >= 1.0 )
		rc = -1, t = 0.0;       /* Sun always below altit */
		else if ( cost <= -1.0 )
		rc = +1, t = 12.0;      /* Sun always above altit */
		else
		t = acosd(cost)/15.0;   /* The diurnal arc, hours */
		}

		/* Store rise and set times - in hours UT */
		*trise = tsouth - t;
		*tset  = tsouth + t;

		return rc;
	};

	

	void Sunriset::_sunpos( double d, double *lon, double *r ) {
		double M,         /* Mean anomaly of the Sun */
		w,         /* Mean longitude of perihelion */
		/* Note: Sun's mean longitude = M + w */
		e,         /* Eccentricity of Earth's orbit */
		E,         /* Eccentric anomaly */
		x, y,      /* x, y coordinates in orbit */
		v;         /* True anomaly */

		/* Compute mean elements */
		M = this->_revolution( 356.0470 + 0.9856002585 * d );
		w = 282.9404 + 4.70935E-5 * d;
		e = 0.016709 - 1.151E-9 * d;

		/* Compute true longitude and radius vector */
		E = M + e * RADEG * sind(M) * ( 1.0 + e * cosd(M) );
		x = cosd(E) - e;
		y = sqrt( 1.0 - e*e ) * sind(E);
		*r = sqrt( x*x + y*y );              /* Solar distance */
		v = atan2d( y, x );                  /* True anomaly */
		*lon = v + w;                        /* True solar longitude */
		if ( *lon >= 360.0 )
		*lon -= 360.0;                   /* Make it 0..360 degrees */
	};

	void Sunriset::_sun_RA_dec( double d, double *RA, double *dec, double *r ) {
		double lon, obl_ecl, x, y, z;

		/* Compute Sun's ecliptical coordinates */
		this->_sunpos( d, &lon, r );

		/* Compute ecliptic rectangular coordinates (z=0) */
		x = *r * cosd(lon);
		y = *r * sind(lon);

		/* Compute obliquity of ecliptic (inclination of Earth's axis) */
		obl_ecl = 23.4393 - 3.563E-7 * d;

		/* Convert to equatorial rectangular coordinates - x is unchanged */
		z = y * sind(obl_ecl);
		y = y * cosd(obl_ecl);

		/* Convert to spherical coordinates */
		*RA = atan2d( y, x );
		*dec = atan2d( z, sqrt(x*x + y*y) );
	};

	double Sunriset::_revolution( double x ) {
		return( x - 360.0 * floor( x * INV360 ) );
	};

	double Sunriset::_rev180( double x ) {
		return( x - 360.0 * floor( x * INV360 + 0.5 ) );
	};

	double Sunriset::_GMST0( double d ) {
		return this->_revolution( ( 180.0 + 356.0470 + 282.9404 ) + ( 0.9856002585 + 4.70935E-5 ) * d );
	};

}; // namespace micasa
