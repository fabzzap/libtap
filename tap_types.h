/* TAP shared library: a library for converting audio data (as a stream of
 * PCM signed 32-bit samples, mono) to raw TAP data and back
 * 
 * TAP specification was written by Per Håkan Sundell and is available at
 * http://www.computerbrains.com/tapformat.html
 * 
 * The algorithm for TAP encoding was originally written by Janne Veli
 * Kujala, based on the one written by Andreas Matthies for Tape64.
 * Some modifications and adaptation to library format by Fabrizio Gennari
 *
 * The algorithm to decode TAP data into square waves is by Fabrizio Gennari.
 * The choice to use square waves has been inspired by 64TAPVOC by Tomaz Kac,
 * and confirmed by discussions with Luigi Di Fraia
 *
 * The clocks were taken from mtap by Markus Brenner
 *
 * Copyright (c) Fabrizio Gennari, 2003
 *
 * The program is distributed under the GNU Lesser General Public License.
 * See file LESSER-LICENSE.TXT for details.
 */

#ifndef TAP_TYPES_H_INCLUDED
#define TAP_TYPES_H_INCLUDED

/* Machine entry used in extended TAP header */
#define TAP_MACHINE_C64 0
#define TAP_MACHINE_VIC 1
#define TAP_MACHINE_C16 2
#define TAP_MACHINE_MAX 2

/* Video standards */
#define TAP_VIDEOTYPE_PAL  0
#define TAP_VIDEOTYPE_NTSC 1
#define TAP_VIDEOTYPE_MAX  1

/*struct tap_pulse{
  unsigned char version_0;
  unsigned char version_1[4];
};*/

/* These typedef's are taken from Cygwin's headers, therefore they should be right */
#if (defined WIN32 && !defined __CYGWIN__)
typedef int int32_t;
typedef unsigned int u_int32_t;
typedef unsigned char u_int8_t;
typedef long long int64_t;
#endif

#endif /* TAP_TYPES_H_INCLUDED */
