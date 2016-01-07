/* gps.h -- interface of the libgps library */
/*
 * This file is Copyright (c) 2010 by the GPSD project
 *
 * 			COPYRIGHTS
 *
 * Compilation copyright is held by the GPSD project.  All rights reserved.
 *
 * GPSD project copyrights are assigned to the project lead, currently
 * Eric S. Raymond. Other portions of the GPSD code are Copyright (c)
 * 1997, 1998, 1999, 2000, 2001, 2002 by Remco Treffkorn, and others
 * Copyright (c) 2005 by Eric S. Raymond.  For other copyrights, see
 * individual files.
 *
 * 			BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:<P>
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.<P>
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.<P>
 *
 * Neither name of the GPSD project nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * This file originated from GPSD version 3.6.
 */
#ifndef _GPSD_GPS_H_
#define _GPSD_GPS_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Macro for declaring function arguments unused. */
#if defined(__GNUC__)
#  define UNUSED __attribute__((unused)) /* Flag variable as unused */
#else /* not __GNUC__ */
#  define UNUSED
#endif


#include <sys/types.h>
#include <sys/time.h>
#include <stdbool.h>
#include <inttypes.h>	/* stdint.h would be smaller but not all have it */
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#ifndef S_SPLINT_S
#include <pthread.h>	/* pacifies OpenBSD's compiler */
#endif

/*
 * 4.1 - Base version for initial JSON protocol (Dec 2009, release 2.90)
 * 4.2 - AIS application IDs split into DAC and FID (July 2010, release 2.95)
 * 5.0 - MAXCHANNELS bumped from 20 to 32 for GLONASS (Mar 2011, release 2.96)
 *       gps_open() becomes reentrant, what gps_open_r() used to be. 
 *       gps_poll() removed in favor of gps_read().  The raw hook is gone.
 */
#define GPSD_API_MAJOR_VERSION	5	/* bump on incompatible changes */
#define GPSD_API_MINOR_VERSION	0	/* bump on compatible changes */

#define MAXTAGLEN	8	/* maximum length of sentence tag name */
#define MAXCHANNELS	72	/* must be > 12 GPS + 12 GLONASS + 2 WAAS */
#define GPS_PRNMAX	32	/* above this number are SBAS satellites */
#define GPS_PATH_MAX	64	/* dev files usually have short names */
#define MAXUSERDEVS	4	/* max devices per user */

/* 
 * The structure describing an uncertainty volume in kinematic space.
 * This is what GPSes are meant to produce; all the other info is 
 * technical impedimenta.
 *
 * All double values use NAN to indicate data not available.
 *
 * Usually all the information in this structure was considered valid
 * by the GPS at the time of update.  This will be so if you are using
 * a GPS chipset that speaks SiRF binary, Garmin binary, or Zodiac binary.
 * This covers over 80% of GPS products in early 2005.
 *
 * If you are using a chipset that speaks NMEA, this structure is updated
 * in bits by GPRMC (lat/lon, track, speed), GPGGA (alt, climb), GPGLL 
 * (lat/lon), and GPGSA (eph, epv).  Most NMEA GPSes take a single fix
 * at the beginning of a 1-second cycle and report the same timestamp in
 * GPRMC, GPGGA, and GPGLL; for these, all info is guaranteed correctly
 * synced to the time member, but you'll get different stages of the same 
 * update depending on where in the cycle you poll.  A very few GPSes, 
 * like the Garmin 48, take a new fix before more than one of of 
 * GPRMC/GPGGA/GPGLL during a single cycle; thus, they may have different 
 * timestamps and some data in this structure can be up to 1 cycle (usually
 * 1 second) older than the fix time.
 *
 * Error estimates are at 95% confidence.
 */
typedef double timestamp_t;	/* Unix time in seconds with fractional part */

struct gps_fix_t {
    timestamp_t time;	/* Time of update */
    int    mode;	/* Mode of fix */
#define MODE_NOT_SEEN	0	/* mode update not seen yet */
#define MODE_NO_FIX	1	/* none */
#define MODE_2D  	2	/* good for latitude/longitude */
#define MODE_3D  	3	/* good for altitude/climb too */
    double ept;		/* Expected time uncertainty */
    double latitude;	/* Latitude in degrees (valid if mode >= 2) */
    double epy;  	/* Latitude position uncertainty, meters */
    double longitude;	/* Longitude in degrees (valid if mode >= 2) */
    double epx;  	/* Longitude position uncertainty, meters */
    double altitude;	/* Altitude in meters (valid if mode == 3) */
    double epv;  	/* Vertical position uncertainty, meters */
    double track;	/* Course made good (relative to true north) */
    double epd;		/* Track uncertainty, degrees */
    double speed;	/* Speed over ground, meters/sec */
    double eps;		/* Speed uncertainty, meters/sec */
    double climb;       /* Vertical speed, meters/sec */
    double epc;		/* Vertical speed uncertainty */
};

/*
 * Satellite ID classes.
 * U.S. GPS authorities reserve PRNs 1-64 for GPS satellites.
 * GLONASS birds reuse GPS PRNs;  it is a GPSD convention to map them to 
 * IDs 64 and above (some other programs push them to 33 and above).
 * All SBAS/WAAS/EGNOS birds have PRNs above 100.
 */
#define GNSS_PRN(n)	(((n) >= 1) && ((n) <= 63))	/* GNSS satellite */
#define GLONASS_PRN(n)	(((n) >= 64) && ((n) <= 96))	/* GLONASS satellite */
#define DGPS_PRN(n)	((n) >= 100)

/* 
 * The structure describing the pseudorange errors (GPGST)
 */
struct gst_t {
    double utctime;
    double rms_deviation;
    double smajor_deviation;
    double sminor_deviation;
    double smajor_orientation;
    double lat_err_deviation;
    double lon_err_deviation;
    double alt_err_deviation;
};

/*  
 * From the RCTM104 2.x standard:
 *
 * "The 30 bit words (as opposed to 32 bit words) coupled with a 50 Hz
 * transmission rate provides a convenient timing capability where the
 * times of word boundaries are a rational multiple of 0.6 seconds."
 *
 * "Each frame is N+2 words long, where N is the number of message data
 * words. For example, a filler message (type 6 or 34) with no message
 * data will have N=0, and will consist only of two header words. The
 * maximum number of data words allowed by the format is 31, so that
 * the longest possible message will have a total of 33 words."
 */
#define RTCM2_WORDS_MAX	33
#define MAXCORRECTIONS	18	/* max correction count in type 1 or 9 */
#define MAXSTATIONS	10	/* maximum stations in almanac, type 5 */
/* RTCM104 doesn't specify this, so give it the largest reasonable value */
#define MAXHEALTH	(RTCM2_WORDS_MAX-2)

#ifndef S_SPLINT_S 
/*
 * A nominally 30-bit word (24 bits of data, 6 bits of parity)
 * used both in the GPS downlink protocol described in IS-GPS-200
 * and in the format for DGPS corrections used in RTCM-104v2.
 */
typedef /*@unsignedintegraltype@*/ uint32_t isgps30bits_t;
#endif /* S_SPLINT_S */

/* 
 * Values for "system" fields.  Note, the encoding logic is senstive to the 
 * actual values of these; it's not sufficient that they're distinct.
 */
#define NAVSYSTEM_GPS   	0
#define NAVSYSTEM_GLONASS	1
#define NAVSYSTEM_GALILEO	2
#define NAVSYSTEM_UNKNOWN	3

struct rtcm2_t {
    /* header contents */
    unsigned type;	/* RTCM message type */
    unsigned length;	/* length (words) */
    double   zcount;	/* time within hour: GPS time, no leap secs */
    unsigned refstaid;	/* reference station ID */
    unsigned seqnum;	/* message sequence number (modulo 8) */
    unsigned stathlth;	/* station health */

    /* message data in decoded form */
    union {
	struct {
	    unsigned int nentries;
	    struct gps_rangesat_t {	/* data from messages 1 & 9 */
		unsigned ident;		/* satellite ID */
		unsigned udre;		/* user diff. range error */
		unsigned iod;		/* issue of data */
		double prc;		/* range error */
		double rrc;		/* range error rate */
	    } sat[MAXCORRECTIONS];
	} gps_ranges;
	struct {		/* data for type 3 messages */
	    bool valid;		/* is message well-formed? */
	    double x, y, z;
	} ecef;
	struct {		/* data from type 4 messages */
	    bool valid;		/* is message well-formed? */
	    int system;
	    int sense;
#define SENSE_INVALID	0
#define SENSE_GLOBAL	1
#define SENSE_LOCAL   	2
	    char datum[6];
	    double dx, dy, dz;
	} reference;
	struct {		/* data from type 5 messages */
	    unsigned int nentries;
	    struct consat_t {
		unsigned ident;		/* satellite ID */
		bool iodl;		/* issue of data */
		unsigned int health;	/* is satellite healthy? */
#define HEALTH_NORMAL		(0)	/* Radiobeacon operation normal */
#define HEALTH_UNMONITORED	(1)	/* No integrity monitor operating */
#define HEALTH_NOINFO		(2)	/* No information available */
#define HEALTH_DONOTUSE		(3)	/* Do not use this radiobeacon */
	       int snr;			/* signal-to-noise ratio, dB */
#define SNR_BAD	-1			/* not reported */
		bool health_en; 	/* health enabled */
		bool new_data;		/* new data? */
		bool los_warning;	/* line-of-sight warning */
		unsigned int tou;	/* time to unhealth, seconds */
	    } sat[MAXHEALTH];
	} conhealth;
	struct {		/* data from type 7 messages */
	    unsigned int nentries;
	    struct station_t {
		double latitude, longitude;	/* location */
		unsigned int range;		/* range in km */
		double frequency;		/* broadcast freq */
		unsigned int health;		/* station health */
		unsigned int station_id;	/* of the transmitter */
		unsigned int bitrate;		/* of station transmissions */
	    } station[MAXSTATIONS];
	} almanac;
	struct {		/* data for type 13 messages */
	    bool status;		/* expect a text message */
	    bool rangeflag;		/* station range altered? */
	    double lat, lon;		/* station longitude/latitude */
	    unsigned int range;		/* transmission range in km */
	} xmitter;
	struct {		/* data from type 14 messages */
	    unsigned int week;			/* GPS week (0-1023) */
	    unsigned int hour;			/* Hour in week (0-167) */
	    unsigned int leapsecs;		/* Leap seconds (0-63) */
	} gpstime;
	struct {
	    unsigned int nentries;
	    struct glonass_rangesat_t {		/* data from message type 31 */
		unsigned ident;		/* satellite ID */
		unsigned udre;		/* user diff. range error */
		unsigned tod;		/* issue of data */
		bool change;		/* ephemeris change bit */
		double prc;		/* range error */
		double rrc;		/* range error rate */
	    } sat[MAXCORRECTIONS];
	} glonass_ranges;
	/* data from type 16 messages */
	char message[(RTCM2_WORDS_MAX-2) * sizeof(isgps30bits_t)];
	/* data from messages of unknown type */
	isgps30bits_t	words[RTCM2_WORDS_MAX-2];
    };
};

/* RTCM3 report structures begin here */

#define RTCM3_MAX_SATELLITES	64
#define RTCM3_MAX_DESCRIPTOR	31
#define RTCM3_MAX_ANNOUNCEMENTS	32

struct rtcm3_rtk_hdr {		/* header data from 1001, 1002, 1003, 1004 */
    /* Used for both GPS and GLONASS, but their timebases differ */
    unsigned int station_id;	/* Reference Station ID */
    time_t tow;			/* GPS Epoch Time (TOW) in ms, 
				   or GLONASS Epoch Time in ms */
    bool sync;			/* Synchronous GNSS Message Flag */
    unsigned short satcount;	/* # Satellite Signals Processed */
    bool smoothing;		/* Divergence-free Smoothing Indicator */
    unsigned short interval;	/* Smoothing Interval */
};

struct rtcm3_basic_rtk {
    unsigned char indicator;	/* Indicator */
    short channel;		/* Satellite Frequency Channel Number 
				   (GLONASS only) */
    double pseudorange;		/* Pseudorange */
    double rangediff;		/* PhaseRange – Pseudorange in meters */
    unsigned char locktime;	/* Lock time Indicator */
};

struct rtcm3_extended_rtk {
    unsigned char indicator;	/* Indicator */
    short channel;		/* Satellite Frequency Channel Number 
				   (GLONASS only) */
    double pseudorange;		/* Pseudorange */
    double rangediff;		/* PhaseRange – L1 Pseudorange */
    unsigned char locktime;	/* Lock time Indicator */
    unsigned char ambiguity;	/* Integer Pseudorange 
					   Modulus Ambiguity */
    double CNR;			/* Carrier-to-Noise Ratio */
};

struct rtcm3_network_rtk_header {
    unsigned int network_id;	/* Network ID */
    unsigned int subnetwork_id;	/* Subnetwork ID */
    time_t time;		/* GPS Epoch Time (TOW) in ms */
    bool multimesg;		/* GPS Multiple Message Indicator */
    unsigned master_id;		/* Master Reference Station ID */
    unsigned aux_id;		/* Auxilary Reference Station ID */
    unsigned char satcount;	/* count of GPS satellites */
};

struct rtcm3_correction_diff {
    unsigned char ident;	/* satellite ID */
    enum {reserved, correct, widelane, uncertain} ambiguity;
    unsigned char nonsync;
    double geometric_diff;	/* Geometric Carrier Phase 
				   Correction Difference (1016, 1017) */
    unsigned char iode;		/* GPS IODE (1016, 1017) */
    double ionospheric_diff;	/* Ionospheric Carrier Phase 
				   Correction Difference (1015, 1017) */
};

struct rtcm3_t {
    /* header contents */
    unsigned type;	/* RTCM 3.x message type */
    unsigned length;	/* payload length, inclusive of checksum */

    union {
	/* 1001-1013 were present in the 3.0 version */
	struct {
	    struct rtcm3_rtk_hdr	header;
	    struct rtcm3_1001_t {
		unsigned ident;			/* Satellite ID */
		struct rtcm3_basic_rtk L1;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1001;
	struct rtcm3_1002_t {
	    struct rtcm3_rtk_hdr	header;
	    struct {
		unsigned ident;			/* Satellite ID */
		struct rtcm3_extended_rtk L1;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1002;
	struct rtcm3_1003_t {
	    struct rtcm3_rtk_hdr	header;
	    struct {
		unsigned ident;			/* Satellite ID */
		struct rtcm3_basic_rtk L1;
		struct rtcm3_basic_rtk L2;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1003;
	struct rtcm3_1004_t {
	    struct rtcm3_rtk_hdr	header;
	    struct {
		unsigned ident;			/* Satellite ID */
		struct rtcm3_extended_rtk L1;
		struct rtcm3_extended_rtk L2;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1004;
	struct rtcm3_1005_t {
	    unsigned int station_id;		/* Reference Station ID */
	    int system;				/* Which system is it? */
	    bool reference_station;		/* Reference-station indicator */
	    bool single_receiver;		/* Single Receiver Oscillator */
	    double ecef_x, ecef_y, ecef_z;	/* ECEF antenna location */
	} rtcm3_1005;
	struct rtcm3_1006_t {
	    unsigned int station_id;		/* Reference Station ID */
	    int system;				/* Which system is it? */
	    bool reference_station;		/* Reference-station indicator */
	    bool single_receiver;		/* Single Receiver Oscillator */
	    double ecef_x, ecef_y, ecef_z;	/* ECEF antenna location */
	    double height;			/* Antenna height */
	} rtcm3_1006;
	struct rtcm3_1007_t {
	    unsigned int station_id;			/* Reference Station ID */
	    char descriptor[RTCM3_MAX_DESCRIPTOR+1];	/* Description string */
	    unsigned char setup_id;
	} rtcm3_1007;
	struct rtcm3_1008_t {
	    unsigned int station_id;			/* Reference Station ID */
	    char descriptor[RTCM3_MAX_DESCRIPTOR+1];	/* Description string */
	    unsigned char setup_id;
	    char serial[RTCM3_MAX_DESCRIPTOR+1];	/* Serial # string */
	} rtcm3_1008;
	struct rtcm3_1009_t {
	    struct rtcm3_rtk_hdr	header;
	    struct {
		unsigned ident;		/* Satellite ID */
		struct rtcm3_basic_rtk L1;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1009;
	struct rtcm3_1010_t {
	    struct rtcm3_rtk_hdr	header;
	    struct {
		unsigned ident;		/* Satellite ID */
		struct rtcm3_extended_rtk L1;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1010;
	struct rtcm3_1011_t {
	    struct rtcm3_rtk_hdr	header;
	    struct {
		unsigned ident;			/* Satellite ID */
		struct rtcm3_extended_rtk L1;
		struct rtcm3_extended_rtk L2;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1011;
	struct rtcm3_1012_t {
	    struct rtcm3_rtk_hdr	header;
	    struct {
		unsigned ident;			/* Satellite ID */
		struct rtcm3_extended_rtk L1;
		struct rtcm3_extended_rtk L2;
	    } rtk_data[RTCM3_MAX_SATELLITES];
	} rtcm3_1012;
	struct rtcm3_1013_t {
	    unsigned int station_id;	/* Reference Station ID */
	    unsigned short mjd;		/* Modified Julian Day (MJD) Number */
	    unsigned int sod;		/* Seconds of Day (UTC) */
	    unsigned char leapsecs;	/* Leap Seconds, GPS-UTC */
	    unsigned char ncount;	/* Count of announcements to follow */
	    struct {
		unsigned short id;		/* message type ID */
		bool sync;
		unsigned short interval;	/* interval in 0.1sec units */
	    } announcements[RTCM3_MAX_ANNOUNCEMENTS];
	} rtcm3_1013;
	/* 1014-1017 were added in the 3.1 version */
	struct rtcm3_1014_t {
	    unsigned int network_id;	/* Network ID */
	    unsigned int subnetwork_id;	/* Subnetwork ID */
	    unsigned char stationcount;	/* # auxiliary stations transmitted */
	    unsigned int master_id;	/* Master Reference Station ID */
	    unsigned int aux_id;	/* Auxilary Reference Station ID */
	    double d_lat, d_lon, d_alt;	/* Aux-master location delta */
	} rtcm3_1014;
	struct rtcm3_1015_t {
	    struct rtcm3_network_rtk_header	header;
	    struct rtcm3_correction_diff corrections[RTCM3_MAX_SATELLITES];
	} rtcm3_1015;
	struct rtcm3_1016_t {
	    struct rtcm3_network_rtk_header	header;
	    struct rtcm3_correction_diff corrections[RTCM3_MAX_SATELLITES];
	} rtcm3_1016;
	struct rtcm3_1017_t {
	    struct rtcm3_network_rtk_header	header;
	    struct rtcm3_correction_diff corrections[RTCM3_MAX_SATELLITES];
	} rtcm3_1017;
	/* 1018-1029 were in the 3.0 version */
	struct rtcm3_1019_t {
	    unsigned int ident;		/* Satellite ID */
	    unsigned int week;		/* GPS Week Number */
	    unsigned char sv_accuracy;	/* GPS SV ACCURACY */
	    enum {reserved_code, p, ca, l2c} code;
	    double idot;
	    unsigned char iode;
	    /* ephemeris fields, not scaled */
	    unsigned int t_sub_oc;
	    signed int a_sub_f2;
	    signed int a_sub_f1;
	    signed int a_sub_f0;
	    unsigned int iodc;
	    signed int C_sub_rs;
	    signed int delta_sub_n;
	    signed int M_sub_0;
	    signed int C_sub_uc;
	    unsigned int e;
	    signed int C_sub_us;
	    unsigned int sqrt_sub_A;
	    unsigned int t_sub_oe;
	    signed int C_sub_ic;
	    signed int OMEGA_sub_0;
	    signed int C_sub_is;
	    signed int i_sub_0;
	    signed int C_sub_rc;
	    signed int argument_of_perigee;
	    signed int omegadot;
	    signed int t_sub_GD;
	    unsigned char sv_health;
	    bool p_data;
	    bool fit_interval;
	} rtcm3_1019;
	struct rtcm3_1020_t {
	    unsigned int ident;		/* Satellite ID */
	    unsigned short channel;	/* Satellite Frequency Channel Number */
	    /* ephemeris fields, not scaled */
	    bool C_sub_n;
	    bool health_avAilability_indicator;
	    unsigned char P1;
	    unsigned short t_sub_k;
	    bool msb_of_B_sub_n;
	    bool P2;
	    bool t_sub_b;
	    signed int x_sub_n_t_of_t_sub_b_prime;
	    signed int x_sub_n_t_of_t_sub_b;
	    signed int x_sub_n_t_of_t_sub_b_prime_prime;
	    signed int y_sub_n_t_of_t_sub_b_prime;
	    signed int y_sub_n_t_of_t_sub_b;
	    signed int y_sub_n_t_of_t_sub_b_prime_prime;
	    signed int z_sub_n_t_of_t_sub_b_prime;
	    signed int z_sub_n_t_of_t_sub_b;
	    signed int z_sub_n_t_of_t_sub_b_prime_prime;
	    bool P3;
	    signed int gamma_sub_n_of_t_sub_b;
	    unsigned char MP;
	    bool Ml_n;
	    signed int tau_n_of_t_sub_b;
	    signed int M_delta_tau_sub_n;
	    unsigned int E_sub_n;
	    bool MP4;
	    unsigned char MF_sub_T;
	    unsigned char MN_sub_T;
	    unsigned char MM;
	    bool additioinal_data_availability;
	    unsigned int N_sup_A;
	    unsigned int tau_sub_c;
	    unsigned int M_N_sub_4;
	    signed int M_tau_sub_GPS;
	    bool M_l_sub_n;
	} rtcm3_1020;
	struct rtcm3_1029_t {
	    unsigned int station_id;	/* Reference Station ID */
	    unsigned short mjd;		/* Modified Julian Day (MJD) Number */
	    unsigned int sod;		/* Seconds of Day (UTC) */
	    size_t len;			/* # chars to follow */
	    size_t unicode_units;	/* # Unicode units in text */
	    unsigned char text[128];
	} rtcm3_1029;
	struct rtcm3_1033_t {
	    unsigned int station_id;			/* Reference Station ID */
	    char descriptor[RTCM3_MAX_DESCRIPTOR+1];	/* Description string */
	    unsigned char setup_id;
	    char serial[RTCM3_MAX_DESCRIPTOR+1];	/* Serial # string */
	    char receiver[RTCM3_MAX_DESCRIPTOR+1];	/* Receiver string */
	    char firmware[RTCM3_MAX_DESCRIPTOR+1];	/* Firmware string */
	} rtcm3_1033;
	char data[1024];		/* Max RTCM3 msg length is 1023 bytes */
    } rtcmtypes;
};

/* RTCM3 scaling constants */
#define GPS_AMBIGUITY_MODULUS		299792.458	/* 1004, DF014*/
#define GLONASS_AMBIGUITY_MODULUS	599584.916	/* 1012, DF044 */
#define MESSAGE_INTERVAL_UNITS		0.1		/* 1013, DF047 */

/*
 * Raw IS_GPS subframe data
 */

/* The almanac is a subset of the clock and ephemeris data, with reduced 
 * precision. See IS-GPS-200E, Table 20-VI  */
struct almanac_t
{
    uint8_t sv;  /* The satellite this refers to */
    /* toa, almanac reference time, 8 bits unsigned, seconds */
    uint8_t toa;
    long l_toa;
    /* SV health data, 8 bit unsigned bit map */
    uint8_t svh;
    /* deltai, correction to inclination, 16 bits signed, semi-circles */
    int16_t deltai;
    double d_deltai;
    /* M0, Mean Anomaly at Reference Time, 24 bits signed, semi-circles */
    int32_t M0;
    double d_M0;
    /* Omega0, Longitude of Ascending Node of Orbit Plane at Weekly Epoch,
     * 24 bits signed, semi-circles */
    int32_t Omega0;
    double d_Omega0;
    /* omega, Argument of Perigee, 24 bits signed, semi-circles */
    int32_t omega;
    double d_omega;
    /* af0, SV clock correction constant term
     * 11 bits signed, seconds */
    int16_t af0;
    double d_af0;
    /* af1, SV clock correction first order term
     * 11 bits signed, seconds/second */
    int16_t af1;
    double d_af1;
    /* eccentricity, 16 bits, unsigned, dimensionless */
    uint16_t e;
    double d_eccentricity;
    /* sqrt A, Square Root of the Semi-Major Axis
     * 24 bits unsigned, square_root(meters) */
    uint32_t sqrtA;
    double d_sqrtA;
    /* Omega dot, Rate of Right Ascension, 16 bits signed, semi-circles/sec */
    int16_t Omegad;
    double d_Omegad;
};

struct subframe_t {
    /* subframe number, 3 bits, unsigned, 1 to 5 */
    uint8_t subframe_num;
    /* data_id, denotes the NAV data structure of D(t), 2 bits, in
     * IS-GPS-200E always == 0x1 */
    uint8_t data_id;
    /* SV/page id used for subframes 4 & 5, 6 bits */
    uint8_t pageid; 
    /* tSVID, SV ID of the sat that transmitted this frame, 6 bits unsigned */
    uint8_t tSVID;
    /* TOW, Time of Week of NEXT message, 17 bits unsigned, scale 6, seconds */
    uint32_t TOW17;
    long l_TOW17;
    /* integrity, URA bounds flag, 1 bit */
    bool integrity;
    /* alert, alert flag, SV URA and/or the SV User Differential Range 
     * Accuracy (UDRA) may be worse than indicated, 1 bit */
    bool alert;
    /* antispoof, A-S mode is ON in that SV, 1 bit */
    bool antispoof;
    int is_almanac;
    union {
        /* subframe 1, part of ephemeris, see IS-GPS-200E, Table 20-II
	 * and Table 20-I */
	struct {
	    /* WN, Week Number, 10 bits unsigned, scale 1, weeks */
	    uint16_t WN;
	    /* IODC, Issue of Data, Clock, 10 bits, unsigned, 
	     * issued in 8 data ranges at the same time */
	    uint16_t IODC;
	    /* toc, clock data reference time, 16 bits, unsigned, seconds
	     * scale 2**4, issued in 8 data ranges at the same time */
	    uint16_t toc;
	    long l_toc;
	    /* l2, code on L2, 2 bits, bit map */
	    uint8_t l2;
	    /* l2p, L2 P data flag, 1 bit */
	    uint8_t l2p;
	    /* ura, SV accuracy, 4 bits unsigned index */
	    unsigned int ura;
	    /* hlth, SV health, 6 bits unsigned bitmap */
	    unsigned int hlth;
	    /* af0, SV clock correction constant term
	     * 22 bits signed, scale 2**-31, seconds */
	    int32_t af0;
	    double d_af0;
	    /* af1, SV clock correction first order term
	     * 22 bits signed, scale 2**-43, seconds/second */
	    int16_t af1;
	    double d_af1;
	    /* af2, SV clock correction second order term
	     * 8 bits signed, scale 2**-55, seconds/second**2 */
	    int8_t af2;
	    double d_af2;
	    /* Tgd,  L1-L2 correction term, 8 bits signed,  scale 2**-31,
	     * seconds */
	    int8_t Tgd;
	    double d_Tgd;
	} sub1;
        /* subframe 2, part of ephemeris, see IS-GPS-200E, Table 20-II
	 * and Table 20-III */
	struct {
	    /* Issue of Data (Ephemeris), 
	     * equal to the 8 LSBs of the 10 bit IODC of the same data set */
	    uint8_t IODE;
	    /* Age of Data Offset for the NMCT, 6 bits, scale 900,
	     * ignore if all ones, seconds */
	    uint8_t AODO;
	    uint16_t u_AODO;
	    /* fit, FIT interval flag, indicates a fit interval greater than
	     * 4 hour, 1 bit */
	    uint8_t fit;
	    /* toe, Reference Time Ephemeris, 16 bits unsigned, scale 2**4,
	     * seconds */
	    uint16_t toe;
	    long l_toe;
	    /* Crs, Amplitude of the Sine Harmonic Correction Term to the 
	     * Orbit Radius, 16 bits, scale 2**-5, signed, meters */
	    int16_t Crs;
	    double d_Crs;
	    /* Cus, Amplitude of the Sine Harmonic Correction Term to the 
	     * Argument of Latitude, 16 bits, signed, scale 2**-29, radians */
	    int16_t Cus;
	    double d_Cus;
	    /* Cuc, Amplitude of the Cosine Harmonic Correction Term to the 
	     * Argument of Latitude, 16 bits, signed, scale 2**-29, radians */
	    int16_t Cuc;
	    double d_Cuc;
	    /* deltan, Mean Motion Difference From Computed Value
	     * Mean Motion Difference From Computed Value
	     * 16 bits, signed, scale 2**-43, semi-circles/sec */
	    int16_t deltan;
	    double d_deltan;
	    /* M0, Mean Anomaly at Reference Time, 32 bits signed, 
	     * scale 2**-31, semi-circles */
	    int32_t M0;
	    double d_M0;
	    /* eccentricity, 32 bits, unsigned, scale 2**-33, dimensionless */
	    uint32_t e;
	    double d_eccentricity;
	    /* sqrt A, Square Root of the Semi-Major Axis
	     * 32 bits unsigned, scale 2**-19, square_root(meters) */
	    uint32_t sqrtA;
	    double d_sqrtA;
	} sub2;
        /* subframe 3, part of ephemeris, see IS-GPS-200E, Table 20-II,
	 * Table 20-III */
	struct {
	    /* Issue of Data (Ephemeris), 8 bits, unsigned 
	     * equal to the 8 LSBs of the 10 bit IODC of the same data set */
	    uint8_t IODE;
	    /* Rate of Inclination Angle, 14 bits signed, scale2**-43,
	     * semi-circles/sec */
	    int16_t IDOT;
	    double d_IDOT;
	    /* Cic, Amplitude of the Cosine Harmonic Correction Term to the 
	     * Angle of Inclination, 16 bits signed, scale 2**-29, radians*/
	    int16_t Cic;
	    double d_Cic;
	    /* Cis, Amplitude of the Sine Harmonic Correction Term to the
	     * Angle of Inclination, 16 bits, unsigned, scale 2**-29, radians */
	    int16_t Cis;
	    double d_Cis;
            /* Crc, Amplitude of the Cosine Harmonic Correction Term to the
	     * Orbit Radius, 16 bits signed, scale 2**-5, meters */
	    int16_t Crc;
	    double d_Crc;
	    /* i0, Inclination Angle at Reference Time, 32 bits, signed,
	     * scale 2**-31, semi-circles */
	    int32_t i0;
	    double d_i0;
	    /* Omega0, Longitude of Ascending Node of Orbit Plane at Weekly 
	     * Epoch, 32 bits signed, semi-circles */
	    int32_t Omega0;
	    double d_Omega0;
	    /* omega, Argument of Perigee, 32 bits signed, scale 2**-31,
	     * semi-circles */
	    int32_t omega;
	    double d_omega;
	    /* Omega dot, Rate of Right Ascension, 24 bits signed, 
	     * scale 2**-43, semi-circles/sec */
	    int32_t Omegad;
	    double d_Omegad;
	} sub3;
	struct {
	    struct almanac_t almanac;
	} sub4;
	/* subframe 4, page 13 */
	struct {
	    /* mapping ord ERD# to SV # is non trivial
	     * leave it alone.  See IS-GPS-200E Section 20.3.3.5.1.9 */
	    /* Estimated Range Deviation, 6 bits signed, meters */
	    char ERD[33];
	    /* ai, Availability Indicator, 2bits, bit map */
	    unsigned char ai;
	} sub4_13;
	/* subframe 4, page 17, system message, 23 chars, plus nul */
	struct {
	    char str[24];
	} sub4_17;
	/* subframe 4, page 18 */
	struct {
	    /* ionospheric and UTC data */
	    /* A0, Bias coefficient of GPS time scale relative to UTC time 
	     * scale, 32 bits signed, scale 2**-30, seconds */
	    int32_t A0;
	    double d_A0;
	    /* A1, Drift coefficient of GPS time scale relative to UTC time 
	     * scale, 24 bits signed, scale 2**-50, seconds/second */
	    int32_t A1;
	    double d_A1;

	    /* alphaX, the four coefficients of a cubic equation representing 
	     * the amplitude of the vertical delay */

	    /* alpha0, 8 bits signed, scale w**-30, seconds */
	    int8_t alpha0;
	    double d_alpha0;
	    /* alpha1, 8 bits signed, scale w**-27, seconds/semi-circle */
	    int8_t alpha1;
	    double d_alpha1;
	    /* alpha2, 8 bits signed, scale w**-24, seconds/semi-circle**2 */
	    int8_t alpha2;
	    double d_alpha2;
	    /* alpha3, 8 bits signed, scale w**-24, seconds/semi-circle**3 */
	    int8_t alpha3;
	    double d_alpha3;

	    /* betaX, the four coefficients of a cubic equation representing 
	     * the period of the model */

	    /* beta0, 8 bits signed, scale w**11, seconds */
	    int8_t beta0;
	    double d_beta0;
	    /* beta1, 8 bits signed, scale w**14, seconds/semi-circle */
	    int8_t beta1;
	    double d_beta1;
	    /* beta2, 8 bits signed, scale w**16, seconds/semi-circle**2 */
	    int8_t beta2;
	    double d_beta2;
	    /* beta3, 8 bits signed, scale w**16, seconds/semi-circle**3 */
	    int8_t beta3;
	    double d_beta3;
	    
	    /* leap (delta t ls), current leap second, 8 bits signed, 
	     * scale 1, seconds */
	    int8_t leap;
	    /* lsf (delta t lsf), future leap second, 8 bits signed, 
	     * scale 1, seconds */
	    int8_t lsf;

	    /* tot, reference time for UTC data,
	     * 8 bits unsigned, scale 2**12, seconds */
	    uint8_t tot;
	    double d_tot;

	    /* WNt, UTC reference week number, 8 bits unsigned, scale 1, 
	     * weeks */
	    uint8_t WNt;
	    /* WNlsf, Leap second reference Week Number,
	     * 8 bits unsigned, scale 1, weeks */
	    uint8_t WNlsf;
	    /* DN, Leap second reference Day Number , 8 bits unsigned,
	     * scale 1, days */
	    uint8_t DN;
	} sub4_18;
	/* subframe 4, page 25 */
	struct {
	    /* svf, A-S status and the configuration code of each SV
	     * 4 bits unsigned, bitmap */
	    unsigned char svf[33];
	    /* svh, SV health data for SV 25 through 32
	     * 6 bits unsigned bitmap */
	    uint8_t svhx[8];
	} sub4_25;
	struct {
	    struct almanac_t almanac;
	} sub5;
	struct {
	    /* toa, Almanac reference Time, 8 bits unsigned, scale 2**12,
	     * seconds */
	    uint8_t toa;
	    long l_toa;
	    /* WNa, Week Number almanac, 8 bits, scale 2, GPS Week 
	     * Number % 256 */
	    uint8_t WNa;
	    /* sv, SV health status, 6 bits, bitmap */
	    uint8_t sv[25];
	} sub5_25;
    };
};

#ifndef S_SPLINT_S
typedef uint64_t gps_mask_t;
#else
typedef /*@unsignedintegraltype@*/ unsigned long long gps_mask_t;
#endif /* S_SPLINT_S */

/* 
 * Is an MMSI number that of an auxiliary associated with a mother ship?
 * We need to be able to test this for decoding AIS Type 24 messages.
 * According to <http://www.navcen.uscg.gov/marcomms/gmdss/mmsi.htm#format>,
 * auxiliary-craft MMSIs have the form 98MIDXXXX, where MID is a country 
 * code and XXXX the vessel ID.
 */
#define AIS_AUXILIARY_MMSI(n)	((n) / 10000000 == 98)

/* N/A values and scaling constant for 25/24 bit lon/lat pairs */
#define AIS_LON3_NOT_AVAILABLE	181000
#define AIS_LAT3_NOT_AVAILABLE	91000
#define AIS_LATLON3_SCALE	60000.0

/* N/A values and scaling constant for 28/27 bit lon/lat pairs */
#define AIS_LON4_NOT_AVAILABLE	1810000
#define AIS_LAT4_NOT_AVAILABLE	910000
#define AIS_LATLON4_SCALE	600000.0

struct route_info {
    unsigned int linkage;	/* Message Linkage ID */
    unsigned int sender;	/* Sender Class */
    unsigned int rtype;		/* Route Type */
    unsigned int month;		/* Start month */
    unsigned int day;		/* Start day */
    unsigned int hour;		/* Start hour */
    unsigned int minute;	/* Start minute */
    unsigned int duration;	/* Duration */
    int waycount;		/* Waypoint count */
    struct waypoint_t {
	signed int lon;		/* Longitude */
	signed int lat;		/* Latitude */
    } waypoints[16];
};

struct ais_t
{
    unsigned int	type;		/* message type */
    unsigned int    	repeat;		/* Repeat indicator */
    unsigned int	mmsi;		/* MMSI */
    union {
	/* Types 1-3 Common navigation info */
	struct {
	    unsigned int status;		/* navigation status */
	    signed turn;			/* rate of turn */
#define AIS_TURN_HARD_LEFT	-127
#define AIS_TURN_HARD_RIGHT	127
#define AIS_TURN_NOT_AVAILABLE	128
	    unsigned int speed;			/* speed over ground in deciknots */
#define AIS_SPEED_NOT_AVAILABLE	1023
#define AIS_SPEED_FAST_MOVER	1022		/* >= 102.2 knots */
	    bool accuracy;			/* position accuracy */
#define AIS_LATLON_SCALE	600000.0
	    int lon;				/* longitude */
#define AIS_LON_NOT_AVAILABLE	0x6791AC0
	    int lat;				/* latitude */
#define AIS_LAT_NOT_AVAILABLE	0x3412140
	    unsigned int course;		/* course over ground */
#define AIS_COURSE_NOT_AVAILABLE	3600
	    unsigned int heading;		/* true heading */
#define AIS_HEADING_NOT_AVAILABLE	511
	    unsigned int second;		/* seconds of UTC timestamp */
#define AIS_SEC_NOT_AVAILABLE	60
#define AIS_SEC_MANUAL		61
#define AIS_SEC_ESTIMATED	62
#define AIS_SEC_INOPERATIVE	63
	    unsigned int maneuver;	/* maneuver indicator */
	    //unsigned int spare;	spare bits */
	    bool raim;			/* RAIM flag */
	    unsigned int radio;		/* radio status bits */
	} type1;
	/* Type 4 - Base Station Report & Type 11 - UTC and Date Response */
	struct {
	    unsigned int year;			/* UTC year */
#define AIS_YEAR_NOT_AVAILABLE	0
	    unsigned int month;			/* UTC month */
#define AIS_MONTH_NOT_AVAILABLE	0
	    unsigned int day;			/* UTC day */
#define AIS_DAY_NOT_AVAILABLE	0
	    unsigned int hour;			/* UTC hour */
#define AIS_HOUR_NOT_AVAILABLE	24
	    unsigned int minute;		/* UTC minute */
#define AIS_MINUTE_NOT_AVAILABLE	60
	    unsigned int second;		/* UTC second */
#define AIS_SECOND_NOT_AVAILABLE	60
	    bool accuracy;		/* fix quality */
	    int lon;			/* longitude */
	    int lat;			/* latitude */
	    unsigned int epfd;		/* type of position fix device */
	    //unsigned int spare;	spare bits */
	    bool raim;			/* RAIM flag */
	    unsigned int radio;		/* radio status bits */
	} type4;
	/* Type 5 - Ship static and voyage related data */
	struct {
	    unsigned int ais_version;	/* AIS version level */
	    unsigned int imo;		/* IMO identification */
	    char callsign[7+1];		/* callsign */ 
#define AIS_SHIPNAME_MAXLEN	20
	    char shipname[AIS_SHIPNAME_MAXLEN+1];	/* vessel name */
	    unsigned int shiptype;	/* ship type code */
	    unsigned int to_bow;	/* dimension to bow */
	    unsigned int to_stern;	/* dimension to stern */
	    unsigned int to_port;	/* dimension to port */
	    unsigned int to_starboard;	/* dimension to starboard */
	    unsigned int epfd;		/* type of position fix deviuce */
	    unsigned int month;		/* UTC month */
	    unsigned int day;		/* UTC day */
	    unsigned int hour;		/* UTC hour */
	    unsigned int minute;	/* UTC minute */
	    unsigned int draught;	/* draft in meters */
	    char destination[20+1];	/* ship destination */
	    unsigned int dte;		/* data terminal enable */
	    //unsigned int spare;	spare bits */
	} type5;
	/* Type 6 - Addressed Binary Message */
	struct {
	    unsigned int seqno;		/* sequence number */
	    unsigned int dest_mmsi;	/* destination MMSI */
	    bool retransmit;		/* retransmit flag */
	    //unsigned int spare;	spare bit(s) */
	    unsigned int dac;           /* Application ID */
	    unsigned int fid;           /* Functional ID */
#define AIS_TYPE6_BINARY_MAX	920	/* 920 bits */
	    size_t bitcount;		/* bit count of the data */
	    union {
		char bitdata[(AIS_TYPE6_BINARY_MAX + 7) / 8];
		/* IMO236 - Dangerous Cargo Indication */
		struct {
		    char lastport[5+1];		/* Last Port Of Call */
		    unsigned int lmonth;	/* ETA month */
		    unsigned int lday;		/* ETA day */
		    unsigned int lhour;		/* ETA hour */
		    unsigned int lminute;	/* ETA minute */
		    char nextport[5+1];		/* Next Port Of Call */
		    unsigned int nmonth;	/* ETA month */
		    unsigned int nday;		/* ETA day */
		    unsigned int nhour;		/* ETA hour */
		    unsigned int nminute;	/* ETA minute */
		    char dangerous[20+1];	/* Main Dangerous Good */
		    char imdcat[4+1];		/* IMD Category */
		    unsigned int unid;		/* UN Number */
		    unsigned int amount;	/* Amount of Cargo */
		    unsigned int unit;		/* Unit of Quantity */
		} dac1fid12;
		/* IMO236 - Extended Ship Static and Voyage Related Data */
		struct {
		    unsigned int airdraught;	/* Air Draught */
		} dac1fid15;
		/* IMO236 - Number of Persons on board */
		struct {
		    unsigned persons;	/* number of persons */
		} dac1fid16;
		/* IMO289 - Clearance Time To Enter Port */
		struct {
		    unsigned int linkage;	/* Message Linkage ID */
		    unsigned int month;	/* Month (UTC) */
		    unsigned int day;	/* Day (UTC) */
		    unsigned int hour;	/* Hour (UTC) */
		    unsigned int minute;	/* Minute (UTC) */
		    char portname[20+1];	/* Name of Port & Berth */
		    char destination[5+1];	/* Destination */
		    signed int lon;	/* Longitude */
		    signed int lat;	/* Latitude */
		} dac1fid18;
		/* IMO289 - Berthing Data (addressed) */
		struct {
		    unsigned int linkage;	/* Message Linkage ID */
		    unsigned int berth_length;	/* Berth length */
		    unsigned int berth_depth;	/* Berth Water Depth */
		    unsigned int position;	/* Mooring Position */
		    unsigned int month;	/* Month (UTC) */
		    unsigned int day;	/* Day (UTC) */
		    unsigned int hour;	/* Hour (UTC) */
		    unsigned int minute;	/* Minute (UTC) */
		    unsigned int availability;	/* Services Availability */
		    unsigned int agent;	/* Agent */
		    unsigned int fuel;	/* Bunker/fuel */
		    unsigned int chandler;	/* Chandler */
		    unsigned int stevedore;	/* Stevedore */
		    unsigned int electrical;	/* Electrical */
		    unsigned int water;	/* Potable water */
		    unsigned int customs;	/* Customs house */
		    unsigned int cartage;	/* Cartage */
		    unsigned int crane;	/* Crane(s) */
		    unsigned int lift;	/* Lift(s) */
		    unsigned int medical;	/* Medical facilities */
		    unsigned int navrepair;	/* Navigation repair */
		    unsigned int provisions;	/* Provisions */
		    unsigned int shiprepair;	/* Ship repair */
		    unsigned int surveyor;	/* Surveyor */
		    unsigned int steam;	/* Steam */
		    unsigned int tugs;	/* Tugs */
		    unsigned int solidwaste;	/* Waste disposal (solid) */
		    unsigned int liquidwaste;	/* Waste disposal (liquid) */
		    unsigned int hazardouswaste;	/* Waste disposal (hazardous) */
		    unsigned int ballast;	/* Reserved ballast exchange */
		    unsigned int additional;	/* Additional services */
		    unsigned int regional1;	/* Regional reserved 1 */
		    unsigned int regional2;	/* Regional reserved 2 */
		    unsigned int future1;	/* Reserved for future */
		    unsigned int future2;	/* Reserved for future */
		    char berth_name[20+1];	/* Name of Berth */
		    signed int berth_lon;	/* Longitude */
		    signed int berth_lat;	/* Latitude */
		} dac1fid20;
		/* IMO289 - Dangerous Cargo Indication */
		struct {
		    unsigned int unit;	/* Unit of Quantity */
		    unsigned int amount;	/* Amount of Cargo */
		    int ncargos;
		    struct cargo_t {
			unsigned int code;	/* Cargo code */
			unsigned int subtype;	/* Cargo subtype */
		    } cargos[28];
		} dac1fid25;
		/* IMO289 - Route info (addressed) */
		struct route_info dac1fid28;
		/* IMO289 - Text message (addressed) */
		struct {
		    unsigned int linkage;
#define AIS_DAC1FID30_TEXT_MAX	154	/* 920 bits of six-bit, plus NUL */
		    char text[AIS_DAC1FID30_TEXT_MAX];
		} dac1fid30;
		/* IMO289 & IMO236 - Tidal Window */
		struct {
		    unsigned int type;	/* Message Type */
		    unsigned int repeat;	/* Repeat Indicator */
		    unsigned int mmsi;	/* Source MMSI */
		    unsigned int seqno;	/* Sequence Number */
		    unsigned int dest_mmsi;	/* Destination MMSI */
		    signed int retransmit;	/* Retransmit flag */
		    unsigned int dac;	/* DAC */
		    unsigned int fid;	/* FID */
		    unsigned int month;	/* Month */
		    unsigned int day;	/* Day */
		    signed int ntidals;
		    struct tidal_t {
			signed int lon;	/* Longitude */
			signed int lat;	/* Latitude */
			unsigned int from_hour;	/* From UTC Hour */
			unsigned int from_min;	/* From UTC Minute */
			unsigned int to_hour;	/* To UTC Hour */
			unsigned int to_min;	/* To UTC Minute */
#define DAC1FID32_CDIR_NOT_AVAILABLE		360
			unsigned int cdir;	/* Current Dir. Predicted */
#define DAC1FID32_CSPEED_NOT_AVAILABLE		127
			unsigned int cspeed;	/* Current Speed Predicted */
		    } tidals[3];
		} dac1fid32;
	    };
	} type6;
	/* Type 7 - Binary Acknowledge */
	struct {
	    unsigned int mmsi1;
	    unsigned int mmsi2;
	    unsigned int mmsi3;
	    unsigned int mmsi4;
	    /* spares ignored, they're only padding here */
	} type7;
	/* Type 8 - Broadcast Binary Message */
	struct {
	    unsigned int dac;       	/* Designated Area Code */
	    unsigned int fid;       	/* Functional ID */
#define AIS_TYPE8_BINARY_MAX	952	/* 952 bits */
	    size_t bitcount;		/* bit count of the data */
	    union {
		char bitdata[(AIS_TYPE8_BINARY_MAX + 7) / 8];
		/* IMO236  - Meteorological-Hydrological data
		 * Trial message, not to be used after January 2013
		 * Replaced by IMO289 (DAC 1, FID 31)
		 */
		struct {
#define DAC1FID11_LATLON_SCALE			1000
		    int lon;			/* longitude in minutes * .001 */
#define DAC1FID11_LON_NOT_AVAILABLE		0x1FFFFFF
		    int lat;			/* longitude in minutes * .001 */
#define DAC1FID11_LAT_NOT_AVAILABLE		0xFFFFFF
		    unsigned int day;		/* UTC day */
		    unsigned int hour;		/* UTC hour */
		    unsigned int minute;	/* UTC minute */
		    unsigned int wspeed;	/* average wind speed */
		    unsigned int wgust;		/* wind gust */
#define DAC1FID11_WSPEED_NOT_AVAILABLE		127
		    unsigned int wdir;		/* wind direction */
		    unsigned int wgustdir;	/* wind gust direction */
#define DAC1FID11_WDIR_NOT_AVAILABLE		511
		    int airtemp;		/* temperature, units 0.1C */
#define DAC1FID11_AIRTEMP_NOT_AVAILABLE		1447
		    unsigned int humidity;	/* relative humidity, % */
#define DAC1FID11_HUMIDITY_NOT_AVAILABLE	127
		    int dewpoint;		/* dew point, units 0.1C */
#define DAC1FID11_DEWPOINT_NOT_AVAILABLE	823
		    unsigned int pressure;	/* air pressure, hpa */
#define DAC1FID11_PRESSURE_NOT_AVAILABLE	1311
		    unsigned int pressuretend;	/* tendency */
#define DAC1FID11_PRESSURETREND_NOT_AVAILABLE	3
		    unsigned int visibility;	/* units 0.1 nautical miles */
#define DAC1FID11_VISIBILITY_NOT_AVAILABLE	255
		    int waterlevel;		/* decimeters */
#define DAC1FID11_WATERLEVEL_NOT_AVAILABLE	411
		    unsigned int leveltrend;	/* water level trend code */
#define DAC1FID11_LEVELTREND_NOT_AVAILABLE	3
		    unsigned int cspeed;	/* surface current speed in deciknots */
#define DAC1FID11_CSPEED_NOT_AVAILABLE		255
		    unsigned int cdir;		/* surface current dir., degrees */
#define DAC1FID11_CDIR_NOT_AVAILABLE		511
		    unsigned int cspeed2;	/* current speed in deciknots */
		    unsigned int cdir2;		/* current dir., degrees */
		    unsigned int cdepth2;	/* measurement depth, m */
#define DAC1FID11_CDEPTH_NOT_AVAILABLE		31
		    unsigned int cspeed3;	/* current speed in deciknots */
		    unsigned int cdir3;		/* current dir., degrees */
		    unsigned int cdepth3;	/* measurement depth, m */
		    unsigned int waveheight;	/* in decimeters */
#define DAC1FID11_WAVEHEIGHT_NOT_AVAILABLE	255
		    unsigned int waveperiod;	/* in seconds */
#define DAC1FID11_WAVEPERIOD_NOT_AVAILABLE	63
		    unsigned int wavedir;	/* direction in degrees */
#define DAC1FID11_WAVEDIR_NOT_AVAILABLE		511
		    unsigned int swellheight;	/* in decimeters */
		    unsigned int swellperiod;	/* in seconds */
		    unsigned int swelldir;	/* direction in degrees */
		    unsigned int seastate;	/* Beaufort scale, 0-12 */
#define DAC1FID11_SEASTATE_NOT_AVAILABLE	15
		    int watertemp;		/* units 0.1deg Celsius */
#define DAC1FID11_WATERTEMP_NOT_AVAILABLE	923
		    unsigned int preciptype;	/* 0-7, enumerated */
#define DAC1FID11_PRECIPTYPE_NOT_AVAILABLE	7
		    unsigned int salinity;	/* units of 0.1ppt */
#define DAC1FID11_SALINITY_NOT_AVAILABLE	511
		    unsigned int ice;		/* is there sea ice? */
#define DAC1FID11_ICE_NOT_AVAILABLE		3
		} dac1fid11;
		/* IMO236 - Fairway Closed */
		struct {
		    char reason[20+1];	/* Reason For Closing */
		    char closefrom[20+1];	/* Location Of Closing From */
		    char closeto[20+1];	/* Location of Closing To */
		    unsigned int radius;	/* Radius extension */
#define AIS_DAC1FID13_RADIUS_NOT_AVAILABLE 10001
		    unsigned int extunit;	/* Unit of extension */
#define AIS_DAC1FID13_EXTUNIT_NOT_AVAILABLE 0
		    unsigned int fday;	/* From day (UTC) */
		    unsigned int fmonth;	/* From month (UTC) */
		    unsigned int fhour;	/* From hour (UTC) */
		    unsigned int fminute;	/* From minute (UTC) */
		    unsigned int tday;	/* To day (UTC) */
		    unsigned int tmonth;	/* To month (UTC) */
		    unsigned int thour;	/* To hour (UTC) */
		    unsigned int tminute;	/* To minute (UTC) */
		} dac1fid13;
	        /* IMO236 - Extended ship and voyage data */
		struct {
		    unsigned int airdraught;	/* Air Draught */
		} dac1fid15;
		/* IMO289 - VTS-generated/Synthetic Targets */
		struct {
		    signed int ntargets;
		    struct target_t {
#define DAC1FID17_IDTYPE_MMSI		0
#define DAC1FID17_IDTYPE_IMO		1
#define DAC1FID17_IDTYPE_CALLSIGN	2
#define DAC1FID17_IDTYPE_OTHER		3
			unsigned int idtype;	/* Identifier type */
			union target_id {	/* Target identifier */
			    unsigned int mmsi;
			    unsigned int imo;
#define DAC1FID17_ID_LENGTH		7
			    char callsign[DAC1FID17_ID_LENGTH+1];
			    char other[DAC1FID17_ID_LENGTH+1];
			} id;
			signed int lat;		/* Latitude */
			signed int lon;		/* Longitude */
#define DAC1FID17_COURSE_NOT_AVAILABLE		360
			unsigned int course;	/* Course Over Ground */
			unsigned int second;	/* Time Stamp */
#define DAC1FID17_SPEED_NOT_AVAILABLE		255
			unsigned int speed;	/* Speed Over Ground */
		    } targets[4];
		} dac1fid17;
		/* IMO 289 - Marine Traffic Signal */
		struct {
		    unsigned int linkage;	/* Message Linkage ID */
		    char station[20+1];		/* Name of Signal Station */
		    signed int lon;		/* Longitude */
		    signed int lat;		/* Latitude */
		    unsigned int status;	/* Status of Signal */
		    unsigned int signal;	/* Signal In Service */
		    unsigned int hour;		/* UTC hour */
		    unsigned int minute;	/* UTC minute */
		    unsigned int nextsignal;	/* Expected Next Signal */
		} dac1fid19;
		/* IMO289 - Route info (broadcast) */
		struct route_info dac1fid27;
		/* IMO289 - Text message (broadcast) */
		struct {
		    unsigned int linkage;
#define AIS_DAC1FID29_TEXT_MAX	162	/* 920 bits of six-bit, plus NUL */
		    char text[AIS_DAC1FID29_TEXT_MAX];
		} dac1fid29;
		/* IMO289 - Meteorological-Hydrological data */
		struct {
		    bool accuracy;	/* position accuracy, <10m if true */
#define DAC1FID31_LATLON_SCALE	1000
		    int lon;		/* longitude in minutes * .001 */
#define DAC1FID31_LON_NOT_AVAILABLE	(181*60*DAC1FID31_LATLON_SCALE)
		    int lat;		/* longitude in minutes * .001 */
#define DAC1FID31_LAT_NOT_AVAILABLE	(91*60*DAC1FID31_LATLON_SCALE)
		    unsigned int day;		/* UTC day */
		    unsigned int hour;		/* UTC hour */
		    unsigned int minute;	/* UTC minute */
		    unsigned int wspeed;	/* average wind speed */
		    unsigned int wgust;		/* wind gust */
#define DAC1FID31_WIND_HIGH			126
#define DAC1FID31_WIND_NOT_AVAILABLE		127
		    unsigned int wdir;		/* wind direction */
		    unsigned int wgustdir;	/* wind gust direction */
#define DAC1FID31_DIR_NOT_AVAILABLE		360
		    int airtemp;		/* temperature, units 0.1C */
#define DAC1FID31_AIRTEMP_NOT_AVAILABLE		-1084
		    unsigned int humidity;	/* relative humidity, % */
#define DAC1FID31_HUMIDITY_NOT_AVAILABLE	101
		    int dewpoint;		/* dew point, units 0.1C */
#define DAC1FID31_DEWPOINT_NOT_AVAILABLE	501
		    unsigned int pressure;	/* air pressure, hpa */
#define DAC1FID31_PRESSURE_NOT_AVAILABLE	511
#define DAC1FID31_PRESSURE_HIGH			402
		    unsigned int pressuretend;	/* tendency */
#define DAC1FID31_PRESSURETREND_NOT_AVAILABLE	3
		    bool visgreater;            /* visibility greater than */
		    unsigned int visibility;	/* units 0.1 nautical miles */
#define DAC1FID31_VISIBILITY_NOT_AVAILABLE	127
		    int waterlevel;		/* decimeters or cm */
#define DAC1FID31_WATERLEVEL_NOT_AVAILABLE	40001
		    unsigned int leveltrend;	/* water level trend code */
#define DAC1FID31_LEVELTREND_NOT_AVAILABLE	3
		    unsigned int cspeed;	/* current speed in deciknots */
#define DAC1FID31_CSPEED_NOT_AVAILABLE		255
		    unsigned int cdir;		/* current dir., degrees */
		    unsigned int cspeed2;	/* current speed in deciknots */
		    unsigned int cdir2;		/* current dir., degrees */
		    unsigned int cdepth2;	/* measurement depth, 0.1m */
#define DAC1FID31_CDEPTH_NOT_AVAILABLE		301
		    unsigned int cspeed3;	/* current speed in deciknots */
		    unsigned int cdir3;		/* current dir., degrees */
		    unsigned int cdepth3;	/* measurement depth, 0.1m */
		    unsigned int waveheight;	/* in decimeters */
#define DAC1FID31_HEIGHT_NOT_AVAILABLE		31
		    unsigned int waveperiod;	/* in seconds */
#define DAC1FID31_PERIOD_NOT_AVAILABLE		63
		    unsigned int wavedir;	/* direction in degrees */
		    unsigned int swellheight;	/* in decimeters */
		    unsigned int swellperiod;	/* in seconds */
		    unsigned int swelldir;	/* direction in degrees */
		    unsigned int seastate;	/* Beaufort scale, 0-12 */
#define DAC1FID31_SEASTATE_NOT_AVAILABLE	15
		    int watertemp;		/* units 0.1deg Celsius */
#define DAC1FID31_PRECIPTYPE_NOT_AVAILABLE	7
		    unsigned int preciptype;	/* 0-7, enumerated */
		    unsigned int salinity;	/* units of 0.1% */
#define DAC1FID31_SALINITY_NOT_AVAILABLE	510
		    bool ice;			/* is there sea ice? */
		} dac1fid31;
	    };
	} type8;
	/* Type 9 - Standard SAR Aircraft Position Report */
	struct {
	    unsigned int alt;		/* altitude in meters */
#define AIS_ALT_NOT_AVAILABLE	4095
#define AIS_ALT_HIGH    	4094	/* 4094 meters or higher */
	    unsigned int speed;		/* speed over ground in deciknots */
#define AIS_SAR_SPEED_NOT_AVAILABLE	1023
#define AIS_SAR_FAST_MOVER  	1022
	    bool accuracy;		/* position accuracy */
	    int lon;			/* longitude */
	    int lat;			/* latitude */
	    unsigned int course;	/* course over ground */
	    unsigned int second;	/* seconds of UTC timestamp */
	    unsigned int regional;	/* regional reserved */
	    unsigned int dte;		/* data terminal enable */
	    //unsigned int spare;	spare bits */
	    bool assigned;		/* assigned-mode flag */
	    bool raim;			/* RAIM flag */
	    unsigned int radio;		/* radio status bits */
	} type9;
	/* Type 10 - UTC/Date Inquiry */
	struct {
	    //unsigned int spare;
	    unsigned int dest_mmsi;	/* destination MMSI */
	    //unsigned int spare2;
	} type10;
	/* Type 12 - Safety-Related Message */
	struct {
	    unsigned int seqno;		/* sequence number */
	    unsigned int dest_mmsi;	/* destination MMSI */
	    bool retransmit;		/* retransmit flag */
	    //unsigned int spare;	spare bit(s) */
#define AIS_TYPE12_TEXT_MAX	157	/* 936 bits of six-bit, plus NUL */
	    char text[AIS_TYPE12_TEXT_MAX];
	} type12;
	/* Type 14 - Safety-Related Broadcast Message */
	struct {
	    //unsigned int spare;	spare bit(s) */
#define AIS_TYPE14_TEXT_MAX	161	/* 952 bits of six-bit, plus NUL */
	    char text[AIS_TYPE14_TEXT_MAX];
	} type14;
	/* Type 15 - Interrogation */
	struct {
	    //unsigned int spare;	spare bit(s) */
	    unsigned int mmsi1;
	    unsigned int type1_1;
	    unsigned int offset1_1;
	    //unsigned int spare2;	spare bit(s) */
	    unsigned int type1_2;
	    unsigned int offset1_2;
	    //unsigned int spare3;	spare bit(s) */
	    unsigned int mmsi2;
	    unsigned int type2_1;
	    unsigned int offset2_1;
	    //unsigned int spare4;	spare bit(s) */
	} type15;
	/* Type 16 - Assigned Mode Command */
	struct {
	    //unsigned int spare;	spare bit(s) */
	    unsigned int mmsi1;
	    unsigned int offset1;
	    unsigned int increment1;
	    unsigned int mmsi2;
	    unsigned int offset2;
	    unsigned int increment2;
	} type16;
	/* Type 17 - GNSS Broadcast Binary Message */
	struct {
	    //unsigned int spare;	spare bit(s) */
#define AIS_GNSS_LATLON_SCALE	600.0
	    int lon;			/* longitude */
	    int lat;			/* latitude */
	    //unsigned int spare2;	spare bit(s) */
#define AIS_TYPE17_BINARY_MAX	736	/* 920 bits */
	    size_t bitcount;		/* bit count of the data */
	    char bitdata[(AIS_TYPE17_BINARY_MAX + 7) / 8];
	} type17;
	/* Type 18 - Standard Class B CS Position Report */
	struct {
	    unsigned int reserved;	/* altitude in meters */
	    unsigned int speed;		/* speed over ground in deciknots */
	    bool accuracy;		/* position accuracy */
	    int lon;			/* longitude */
#define AIS_GNS_LON_NOT_AVAILABLE	0x1a838
	    int lat;			/* latitude */
#define AIS_GNS_LAT_NOT_AVAILABLE	0xd548
	    unsigned int course;	/* course over ground */
	    unsigned int heading;	/* true heading */
	    unsigned int second;	/* seconds of UTC timestamp */
	    unsigned int regional;	/* regional reserved */
	    bool cs;     		/* carrier sense unit flag */
	    bool display;		/* unit has attached display? */
	    bool dsc;   		/* unit attached to radio with DSC? */
	    bool band;   		/* unit can switch frequency bands? */
	    bool msg22;	        	/* can accept Message 22 management? */
	    bool assigned;		/* assigned-mode flag */
	    bool raim;			/* RAIM flag */
	    unsigned int radio;		/* radio status bits */
	} type18;
	/* Type 19 - Extended Class B CS Position Report */
	struct {
	    unsigned int reserved;	/* altitude in meters */
	    unsigned int speed;		/* speed over ground in deciknots */
	    bool accuracy;		/* position accuracy */
	    int lon;			/* longitude */
	    int lat;			/* latitude */
	    unsigned int course;	/* course over ground */
	    unsigned int heading;	/* true heading */
	    unsigned int second;	/* seconds of UTC timestamp */
	    unsigned int regional;	/* regional reserved */
	    char shipname[AIS_SHIPNAME_MAXLEN+1];		/* ship name */
	    unsigned int shiptype;	/* ship type code */
	    unsigned int to_bow;	/* dimension to bow */
	    unsigned int to_stern;	/* dimension to stern */
	    unsigned int to_port;	/* dimension to port */
	    unsigned int to_starboard;	/* dimension to starboard */
	    unsigned int epfd;		/* type of position fix deviuce */
	    bool raim;			/* RAIM flag */
	    unsigned int dte;    	/* date terminal enable */
	    bool assigned;		/* assigned-mode flag */
	    //unsigned int spare;	spare bits */
	} type19;
	/* Type 20 - Data Link Management Message */
	struct {
	    //unsigned int spare;	spare bit(s) */
	    unsigned int offset1;	/* TDMA slot offset */
	    unsigned int number1;	/* number of xlots to allocate */
	    unsigned int timeout1;	/* allocation timeout */
	    unsigned int increment1;	/* repeat increment */
	    unsigned int offset2;	/* TDMA slot offset */
	    unsigned int number2;	/* number of xlots to allocate */
	    unsigned int timeout2;	/* allocation timeout */
	    unsigned int increment2;	/* repeat increment */
	    unsigned int offset3;	/* TDMA slot offset */
	    unsigned int number3;	/* number of xlots to allocate */
	    unsigned int timeout3;	/* allocation timeout */
	    unsigned int increment3;	/* repeat increment */
	    unsigned int offset4;	/* TDMA slot offset */
	    unsigned int number4;	/* number of xlots to allocate */
	    unsigned int timeout4;	/* allocation timeout */
	    unsigned int increment4;	/* repeat increment */
	} type20;
	/* Type 21 - Aids to Navigation Report */
	struct {
	    unsigned int aid_type;	/* aid type */
	    char name[35];		/* name of aid to navigation */
	    bool accuracy;		/* position accuracy */
	    int lon;			/* longitude */
	    int lat;			/* latitude */
	    unsigned int to_bow;	/* dimension to bow */
	    unsigned int to_stern;	/* dimension to stern */
	    unsigned int to_port;	/* dimension to port */
	    unsigned int to_starboard;	/* dimension to starboard */
	    unsigned int epfd;		/* type of EPFD */
	    unsigned int second;	/* second of UTC timestamp */
	    bool off_position;		/* off-position indicator */
	    unsigned int regional;	/* regional reserved field */
	    bool raim;			/* RAIM flag */
	    bool virtual_aid;		/* is virtual station? */
	    bool assigned;		/* assigned-mode flag */
	    //unsigned int spare;	unused */
	} type21;
	/* Type 22 - Channel Management */
	struct {
	    //unsigned int spare;	spare bit(s) */
	    unsigned int channel_a;	/* Channel A number */
	    unsigned int channel_b;	/* Channel B number */
	    unsigned int txrx;		/* transmit/receive mode */
	    bool power;			/* high-power flag */
#define AIS_CHANNEL_LATLON_SCALE	600.0
	    union {
		struct {
		    int ne_lon;		/* NE corner longitude */
		    int ne_lat;		/* NE corner latitude */
		    int sw_lon;		/* SW corner longitude */
		    int sw_lat;		/* SW corner latitude */
		} area;
		struct {
		    unsigned int dest1;	/* addressed station MMSI 1 */
		    unsigned int dest2;	/* addressed station MMSI 2 */
		} mmsi;
	    };
	    bool addressed;		/* addressed vs. broadast flag */
	    bool band_a;		/* fix 1.5kHz band for channel A */
	    bool band_b;		/* fix 1.5kHz band for channel B */
	    unsigned int zonesize;	/* size of transitional zone */
	} type22;
	/* Type 23 - Group Assignment Command */
	struct {
	    int ne_lon;			/* NE corner longitude */
	    int ne_lat;			/* NE corner latitude */
	    int sw_lon;			/* SW corner longitude */
	    int sw_lat;			/* SW corner latitude */
	    //unsigned int spare;	spare bit(s) */
	    unsigned int stationtype;	/* station type code */
	    unsigned int shiptype;	/* ship type code */
	    //unsigned int spare2;	spare bit(s) */
	    unsigned int txrx;		/* transmit-enable code */
	    unsigned int interval;	/* report interval */
	    unsigned int quiet;		/* quiet time */
	    //unsigned int spare3;	spare bit(s) */
	} type23;
	/* Type 24 - Class B CS Static Data Report */
	struct {
	    char shipname[AIS_SHIPNAME_MAXLEN+1];	/* vessel name */
	    unsigned int shiptype;	/* ship type code */
	    char vendorid[8];		/* vendor ID */
	    char callsign[8];		/* callsign */
	    union {
		unsigned int mothership_mmsi;	/* MMSI of main vessel */
		struct {
		    unsigned int to_bow;	/* dimension to bow */
		    unsigned int to_stern;	/* dimension to stern */
		    unsigned int to_port;	/* dimension to port */
		    unsigned int to_starboard;	/* dimension to starboard */
		} dim;
	    };
	} type24;
	/* Type 25 - Addressed Binary Message */
	struct {
	    bool addressed;		/* addressed-vs.broadcast flag */
	    bool structured;		/* structured-binary flag */
	    unsigned int dest_mmsi;	/* destination MMSI */
	    unsigned int app_id;        /* Application ID */
#define AIS_TYPE25_BINARY_MAX	128	/* Up to 128 bits */
	    size_t bitcount;		/* bit count of the data */
	    char bitdata[(AIS_TYPE25_BINARY_MAX + 7) / 8];
	} type25;
	/* Type 26 - Addressed Binary Message */
	struct {
	    bool addressed;		/* addressed-vs.broadcast flag */
	    bool structured;		/* structured-binary flag */
	    unsigned int dest_mmsi;	/* destination MMSI */
	    unsigned int app_id;        /* Application ID */
#define AIS_TYPE26_BINARY_MAX	1004	/* Up to 128 bits */
	    size_t bitcount;		/* bit count of the data */
	    char bitdata[(AIS_TYPE26_BINARY_MAX + 7) / 8];
	    unsigned int radio;		/* radio status bits */
	} type26;
	/* Type 27 - Long Range AIS Broadcast message */
	struct {
	    bool accuracy;		/* position accuracy */
	    bool raim;			/* RAIM flag */
	    unsigned int status;	/* navigation status */
#define AIS_LONGRANGE_LATLON_SCALE	600.0
	    int lon;			/* longitude */
#define AIS_LONGRANGE_LON_NOT_AVAILABLE	0x1a838
	    int lat;			/* latitude */
#define AIS_LONGRANGE_LAT_NOT_AVAILABLE	0xd548
	    unsigned int speed;		/* speed over ground in deciknots */
#define AIS_LONGRANGE_SPEED_NOT_AVAILABLE 63
	    unsigned int course;	/* course over ground */
#define AIS_LONGRANGE_COURSE_NOT_AVAILABLE 511
	    bool gnss;			/* are we reporting GNSS position? */
	} type27;
    };
};

struct attitude_t {
    double heading;
    double pitch;
    double roll;
    double yaw;
    double dip;
    double mag_len; /* unitvector sqrt(x^2 + y^2 +z^2) */
    double mag_x;
    double mag_y;
    double mag_z;
    double acc_len; /* unitvector sqrt(x^2 + y^2 +z^2) */
    double acc_x;
    double acc_y;
    double acc_z;
    double gyro_x;
    double gyro_y;
    double temp;
    double depth;
    /* compass status -- TrueNorth (and any similar) devices only */
    char mag_st;
    char pitch_st;
    char roll_st;
    char yaw_st;
};

struct dop_t {
    /* Dilution of precision factors */
    double xdop, ydop, pdop, hdop, vdop, tdop, gdop;
};

struct rawdata_t {
    /* raw measurement data */
    double codephase[MAXCHANNELS];	/* meters */
    double carrierphase[MAXCHANNELS];	/* meters */
    double pseudorange[MAXCHANNELS];	/* meters */
    double deltarange[MAXCHANNELS];	/* meters/sec */
    double doppler[MAXCHANNELS];	/* Hz */
    double mtime[MAXCHANNELS];		/* sec */
    unsigned satstat[MAXCHANNELS];	/* tracking status */
#define SAT_ACQUIRED	0x01		/* satellite acquired */
#define SAT_CODE_TRACK	0x02		/* code-tracking loop acquired */
#define SAT_CARR_TRACK	0x04		/* carrier-tracking loop acquired */
#define SAT_DATA_SYNC	0x08		/* data-bit synchronization done */
#define SAT_FRAME_SYNC	0x10		/* frame synchronization done */
#define SAT_EPHEMERIS	0x20		/* ephemeris collected */
#define SAT_FIX_USED	0x40		/* used for position fix */
};

struct version_t {
    char release[64];			/* external version */
    char rev[64];			/* internal revision ID */
    int proto_major, proto_minor;	/* API major and minor versions */
    char remote[GPS_PATH_MAX];		/* could be from a remote device */
};

struct devconfig_t {
    char path[GPS_PATH_MAX];
    int flags;
#define SEEN_GPS 	0x01
#define SEEN_RTCM2	0x02
#define SEEN_RTCM3	0x04
#define SEEN_AIS 	0x08
    char driver[64];
    char subtype[64];
    double activated;
    unsigned int baudrate, stopbits;	/* RS232 link parameters */
    char parity;			/* 'N', 'O', or 'E' */
    double cycle, mincycle;     	/* refresh cycle time in seconds */
    int driver_mode;    		/* is driver in native mode or not? */
};

struct policy_t {
    bool watcher;			/* is watcher mode on? */
    bool json;				/* requesting JSON? */
    bool nmea;				/* requesting dumping as NMEA? */
    int raw;				/* requesting raw data? */
    bool scaled;			/* requesting report scaling? */ 
    bool timing;			/* requesting timing info */
    int loglevel;			/* requested log level of messages */
    char devpath[GPS_PATH_MAX];		/* specific device to watch */
    char remote[GPS_PATH_MAX];		/* ...if this was passthrough */
};

/* 
 * Someday we may support Windows, under which socket_t is a separate type.
 * In the meantime, having a typedef for this semantic kind is no bad thing,
 * as it makes clearer what some declarations are doing without breaking
 * binary compatibility. 
 */
typedef int socket_t;

/* mode flags for setting streaming policy */
#define WATCH_ENABLE	0x000001u	/* enable streaming */
#define WATCH_DISABLE	0x000002u	/* disable watching */
#define WATCH_JSON	0x000010u	/* JSON output */
#define WATCH_NMEA	0x000020u	/* output in NMEA */
#define WATCH_RARE	0x000040u	/* output of packets in hex */
#define WATCH_RAW	0x000080u	/* output of raw packets */
#define WATCH_SCALED	0x000100u	/* scale output to floats */ 
#define WATCH_TIMING	0x000200u	/* timing information */
#define WATCH_DEVICE	0x000800u	/* watch specific device */
#define WATCH_NEWSTYLE	0x010000u	/* force JSON streaming */
#define WATCH_OLDSTYLE	0x020000u	/* force old-style streaming */

/* 
 * Main structure that includes all previous substructures
 */

struct gps_data_t {
    gps_mask_t set;	/* has field been set since this was last cleared? */
#define ONLINE_SET	(1llu<<1)
#define TIME_SET	(1llu<<2)
#define TIMERR_SET	(1llu<<3)
#define LATLON_SET	(1llu<<4)
#define ALTITUDE_SET	(1llu<<5)
#define SPEED_SET	(1llu<<6)
#define TRACK_SET	(1llu<<7)
#define CLIMB_SET	(1llu<<8)
#define STATUS_SET	(1llu<<9)
#define MODE_SET	(1llu<<10)
#define DOP_SET  	(1llu<<11)
#define HERR_SET	(1llu<<12)
#define VERR_SET	(1llu<<13)
#define ATTITUDE_SET	(1llu<<14)
#define SATELLITE_SET	(1llu<<15)
#define SPEEDERR_SET	(1llu<<16)
#define TRACKERR_SET	(1llu<<17)
#define CLIMBERR_SET	(1llu<<18)
#define DEVICE_SET	(1llu<<19)
#define DEVICELIST_SET	(1llu<<20)
#define DEVICEID_SET	(1llu<<21)
#define RTCM2_SET	(1llu<<22)
#define RTCM3_SET	(1llu<<23)
#define AIS_SET 	(1llu<<24)
#define PACKET_SET	(1llu<<25)
#define SUBFRAME_SET	(1llu<<26)
#define GST_SET 	(1llu<<27)
#define VERSION_SET	(1llu<<28)
#define POLICY_SET	(1llu<<29)
#define LOGMESSAGE_SET	(1llu<<30)
#define ERROR_SET	(1llu<<31)
#define SET_HIGH_BIT	31
    timestamp_t online;		/* NZ if GPS is on line, 0 if not.
				 *
				 * Note: gpsd clears this time when sentences
				 * fail to show up within the GPS's normal
				 * send cycle time. If the host-to-GPS 
				 * link is lossy enough to drop entire
				 * sentences, this field will be
				 * prone to false zero values.
				 */

#ifndef USE_QT
    socket_t gps_fd;		/* socket or file descriptor to GPS */
#else
    void* gps_fd;
#endif
    struct gps_fix_t	fix;	/* accumulated PVT data */

    /* this should move to the per-driver structure */
    double separation;		/* Geoidal separation, MSL - WGS84 (Meters) */

    /* GPS status -- always valid */
    int    status;		/* Do we have a fix? */
#define STATUS_NO_FIX	0	/* no */
#define STATUS_FIX	1	/* yes, without DGPS */
#define STATUS_DGPS_FIX	2	/* yes, with DGPS */

    /* precision of fix -- valid if satellites_used > 0 */
    int satellites_used;	/* Number of satellites used in solution */
    int used[MAXCHANNELS];	/* PRNs of satellites used in solution */
    struct dop_t dop;

    /* redundant with the estimate elements in the fix structure */
    double epe;  /* spherical position error, 95% confidence (meters)  */

    /* satellite status -- valid when satellites_visible > 0 */
    timestamp_t skyview_time;	/* skyview timestamp */
    int satellites_visible;	/* # of satellites in view */
    int PRN[MAXCHANNELS];	/* PRNs of satellite */
    int elevation[MAXCHANNELS];	/* elevation of satellite */
    int azimuth[MAXCHANNELS];	/* azimuth */
    double ss[MAXCHANNELS];	/* signal-to-noise ratio (dB) */

    struct devconfig_t dev;	/* device that shipped last update */

    struct policy_t policy;	/* our listening policy */

    /* should be moved to privdata someday */
    char tag[MAXTAGLEN+1];	/* tag of last sentence processed */

    /* pack things never reported together to reduce structure size */ 
#define UNION_SET	(RTCM2_SET|RTCM3_SET|SUBFRAME_SET|AIS_SET|ATTITUDE_SET|GST_SET|VERSION_SET|DEVICELIST_SET|LOGMESSAGE_SET|ERROR_SET)
    union {
	/* unusual forms of sensor data that might come up the pipe */ 
	struct rtcm2_t	rtcm2;
	struct rtcm3_t	rtcm3;
	struct subframe_t subframe;
	struct ais_t ais;
	struct attitude_t attitude;
	struct rawdata_t raw;
	struct gst_t gst;
	/* "artificial" structures for various protocol responses */
	struct version_t version;
	struct {
	    timestamp_t time;
	    int ndevices;
	    struct devconfig_t list[MAXUSERDEVS];
	} devices;
	char error[256];
    };

    /* Private data - client code must not set this */
    void *privdata;
};

extern int gps_open(/*@null@*/const char *, /*@null@*/const char *, 
		      /*@out@*/struct gps_data_t *);
extern int gps_close(struct gps_data_t *);
extern int gps_send(struct gps_data_t *, const char *, ... );
extern int gps_read(/*@out@*/struct gps_data_t *);
extern int gps_unpack(char *, struct gps_data_t *);
extern bool gps_waiting(const struct gps_data_t *, int);
extern int gps_stream(struct gps_data_t *, unsigned int, /*@null@*/void *);
extern int gps_mainloop(struct gps_data_t *, int,
			void (*)(struct gps_data_t *));
extern const char /*@null observer@*/ *gps_data(const struct gps_data_t *);
extern const char /*@observer@*/ *gps_errstr(const int);

/* dependencies on struct gpsdata_t end hrere */

extern void libgps_trace(int errlevel, const char *, ...);

extern void gps_clear_fix(/*@ out @*/struct gps_fix_t *);
extern void gps_clear_dop( /*@out@*/ struct dop_t *);
extern void gps_merge_fix(/*@ out @*/struct gps_fix_t *,
			  gps_mask_t,
			  /*@ in @*/struct gps_fix_t *);
extern void gps_enable_debug(int, FILE *);
extern /*@observer@*/const char *gps_maskdump(gps_mask_t);

extern double safe_atof(const char *);
extern time_t mkgmtime(register struct tm *);
extern timestamp_t timestamp(void);
extern timestamp_t iso8601_to_unix(char *);
extern /*@observer@*/char *unix_to_iso8601(timestamp_t t, /*@ out @*/char[], size_t len);
extern double earth_distance(double, double, double, double);
extern double earth_distance_and_bearings(double, double, double, double, 
					  /*@null@*//*@out@*/double *, 
					  /*@null@*//*@out@*/double *);
extern double wgs84_separation(double, double);

/* some multipliers for interpreting GPS output */
#define METERS_TO_FEET	3.2808399	/* Meters to U.S./British feet */
#define METERS_TO_MILES	0.00062137119	/* Meters to miles */
#define METERS_TO_FATHOMS	0.54680665	/* Meters to fathoms */
#define KNOTS_TO_MPH	1.1507794	/* Knots to miles per hour */
#define KNOTS_TO_KPH	1.852		/* Knots to kilometers per hour */
#define KNOTS_TO_MPS	0.51444444	/* Knots to meters per second */
#define MPS_TO_KPH	3.6		/* Meters per second to klicks/hr */
#define MPS_TO_MPH	2.2369363	/* Meters/second to miles per hour */
#define MPS_TO_KNOTS	1.9438445	/* Meters per second to knots */
/* miles and knots are both the international standard versions of the units */

/* angle conversion multipliers */
#define GPS_PI      	3.1415926535897932384626433832795029
#define RAD_2_DEG	57.2957795130823208767981548141051703
#define DEG_2_RAD	0.0174532925199432957692369076848861271

/* geodetic constants */
#define WGS84A 6378137		/* equatorial radius */
#define WGS84F 298.257223563	/* flattening */
#define WGS84B 6356752.3142	/* polar radius */

/* netlib_connectsock() errno return values */
#define NL_NOSERVICE	-1	/* can't get service entry */
#define NL_NOHOST	-2	/* can't get host entry */
#define NL_NOPROTO	-3	/* can't get protocol entry */
#define NL_NOSOCK	-4	/* can't create socket */
#define NL_NOSOCKOPT	-5	/* error SETSOCKOPT SO_REUSEADDR */
#define NL_NOCONNECT	-6	/* can't connect to host/socket pair */
#define SHM_NOSHARED	-7	/* shared-memory segment not available */
#define SHM_NOATTACH	-8	/* shared-memory attach failed */
#define DBUS_FAILURE	-9	/* DBUS initialization failure */

#define DEFAULT_GPSD_PORT	"2947"	/* IANA assignment */
#define DEFAULT_RTCM_PORT	"2101"	/* IANA assignment */

/* special host values for non-socket exports */ 
#define GPSD_SHARED_MEMORY	"shared memory"
#define GPSD_DBUS_EXPORT	"DBUS export"

/*
 * Platform-specific declarations
 */

#ifdef _WIN32
#define strtok_r(s,d,p) strtok_s(s,d,p)
#endif

/* Some libc's don't have strlcat/strlcpy. Local copies are provided */
#ifndef HAVE_STRLCAT
size_t strlcat(/*@out@*/char *dst, /*@in@*/const char *src, size_t size);
#endif
#ifndef HAVE_STRLCPY
size_t strlcpy(/*@out@*/char *dst, /*@in@*/const char *src, size_t size);
#endif

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif /* _GPSD_GPS_H_ */
/* gps.h ends here */
