/*
 *   This file is part of Wavfix.
 *
 *   Copyright (c) 2016, 2017 Adrien Gesta-Fline
 *
 *   Wavfix is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Affero General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   any later version.
 *
 *   Wavfix is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Affero General Public License for more details.
 *
 *   You should have received a copy of the GNU Affero General Public License
 *   along with Wavfix. If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <libgen.h> // basename

// This block should be better implemented.
#if defined(__linux__)
#include <linux/limits.h>	// NAME_MAX
#elif defined(__APPLE__)
#include <sys/syslimits.h>
#else
#define NAME_MAX FILENAME_MAX   // windows
#endif

#include "file.h"
#include "libriff.h"
#include "libwav.h"


size_t load_file( unsigned char **buf, char *filename ) {


	/*
		get file size
	*/

	struct stat sb;

	if ( stat(filename, &sb) == -1 ) {
		fprintf( stderr, "%s : %s.\n",
			filename,
			strerror(errno) );

		return 0;
	}


	/*
		prepare buffer
	*/

	*buf = malloc( sb.st_size + 1 );

	if ( *buf == NULL ) {
		fprintf( stderr, "%s:%d in %s() : %s\n",
			__FILE__,
			__LINE__,
			__func__,
			strerror(errno) );

		return 0;
	}

	memset( *buf, 0x00, (sb.st_size + 1) );


	/*
		load file to buffer
	*/

	FILE *f = fopen( filename, "rb" );

	if ( f == NULL ) {
		fprintf( stderr, "Could not open '%s': %s.\n",
			filename,
			strerror(errno) );

		return 0;
	}

	// off_t match struct stat -> st_size type
	off_t bytes_read = fread( *buf, sizeof(unsigned char), sb.st_size, f );

	if ( bytes_read != sb.st_size ) {
		fprintf( stderr, "Short read of '%s': expected %lu bytes but got %lu: %s.\n",
			filename,
			sb.st_size,
			bytes_read,
			strerror(errno) );

		return 0;
	}

	if ( fclose(f) != 0 ) {
		fprintf( stderr, "Error closing '%s': %s.\n",
			filename,
			strerror(errno) );

		return 0;
	}


	return sb.st_size;
}

// https://stackoverflow.com/a/5309508
const char *get_filename_ext(const char *filename) {
	const char *dot = strrchr(filename, '.');
	if(!dot || dot == filename) return "";
	return dot + 1;
}


int build_output_file_path( char *outfile, const char *infile, const char *suffix ) {
	char *bname;
	char **ext;
	memset(outfile, 0, NAME_MAX);
	ext = (char **) get_filename_ext(infile);
	bname = basename((char *) infile);
	snprintf( outfile, NAME_MAX, "%s%s.%s", bname, suffix, *ext );
	return 1;
}


int write_repaired_file( struct chunk *ckls, const char *outfile ) {


	if ( ckls == NULL )
		return 0;

	FILE *f = fopen(outfile, "wb");

	if ( f == NULL ) {
		fprintf( stderr, "Could not open '%s': %s.\n",
			outfile,
			strerror(errno) );

		return 0;
	}

	size_t size = 4; // include 'WAVE' from below

	fwrite("RIFF\0\0\0\0WAVE", 1, RIFF_HEADER_SZ, f);

	while ( ckls != NULL ) {

		// write CKID & CKSZ
		fwrite(ckls, 1, RIFF_CK_HEADER_SZ, f);
		size += RIFF_CK_HEADER_SZ;

		size_t i = 0;
		while ( i < ckls->sz ) {

			fputc(*(ckls->bytes + i), f);
			i++;
			size++;
		}

		// padding byte if needed
		if ( ckls->sz % 2 ) {
			printf("ADDING PADDING BYTE\n");
			fputc(0x41, f);
			size++;
		}

		ckls = ckls->next;
	}

	// update filesize
	fseek( f,  4, SEEK_SET );
	fputs( (char *)&size, f);

	fclose(f);

	return 1;
}
