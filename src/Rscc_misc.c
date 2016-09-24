/* ==============================================================================
 * Rscclust -- R wrapper for the scclust library
 * https://github.com/fsavje/Rscclust
 *
 * Copyright (C) 2016  Fredrik Savje -- http://fredriksavje.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://www.gnu.org/licenses/
 * ============================================================================== */

#include "Rscc_misc.h"

#include <R.h>
#include <Rinternals.h>
#include <scclust.h>


void iRsccwrap_error__(const char* const msg,
                       const char* const file,
                       const int line) {
	char error_buffer[255];
	if (snprintf(error_buffer, 255, "(%s:%d) %s", file, line, msg) < 0) {
		error("Rscc_misc.c: Error printing error message.");
	}
	error(error_buffer);
}


void iRsccwrap_scc_error(void) {
	char error_buffer[255];
	scc_get_latest_error(255, error_buffer);
	error(error_buffer);
}
