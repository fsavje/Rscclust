/* =============================================================================
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
 * ========================================================================== */

#include "Rscc_nng.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <R.h>
#include <Rinternals.h>
#include <scclust.h>
#include <ann_wrapper.h>
#include "Rscc_error.h"


// =============================================================================
// Internal function prototypes
// =============================================================================

static scc_SeedMethod iRscc_parse_seed_method(SEXP R_seed_method);

static scc_UnassignedMethod iRscc_parse_unassigned_method(SEXP R_unassigned_method);


// =============================================================================
// External function implementations
// =============================================================================

SEXP Rscc_nng_clustering(const SEXP R_distance_object,
                         const SEXP R_size_constraint,
                         const SEXP R_seed_method,
                         const SEXP R_unassigned_method,
                         const SEXP R_radius,
                         const SEXP R_primary_data_points,
                         const SEXP R_secondary_unassigned_method,
                         const SEXP R_secondary_radius)
{
	if (!scc_set_ann_dist_search()) {
		iRscc_error("Cannot change NN search functions to ANN.");
	}
	if (!isMatrix(R_distance_object) || !isReal(R_distance_object)) {
		iRscc_error("`R_distance_object` is not a valid distance object.");
	}
	if (!isInteger(R_size_constraint)) {
		iRscc_error("`R_size_constraint` must be integer.");
	}
	if (!isString(R_seed_method)) {
		iRscc_error("`R_seed_method` must be string.");
	}
	if (!isString(R_unassigned_method)) {
		iRscc_error("`R_unassigned_method` must be string.");
	}
	if (!isNull(R_radius) && !isReal(R_radius)) {
		iRscc_error("`R_radius` must be NULL or double.");
	}
	if (!isNull(R_primary_data_points) && !isLogical(R_primary_data_points)) {
		iRscc_error("`R_primary_data_points` must be NULL or logical.");
	}
	if (!isString(R_secondary_unassigned_method)) {
		iRscc_error("`R_secondary_unassigned_method` must be string.");
	}
	if (!isNull(R_secondary_radius) && !isReal(R_secondary_radius)) {
		iRscc_error("`R_secondary_radius` must be NULL or double.");
	}

	const uintmax_t num_data_points = (uintmax_t) INTEGER(getAttrib(R_distance_object, R_DimSymbol))[1];
	const uintmax_t num_dimensions = (uintmax_t) INTEGER(getAttrib(R_distance_object, R_DimSymbol))[0];
	const uint32_t size_constraint = (uint32_t) asInteger(R_size_constraint);
	const scc_SeedMethod seed_method = iRscc_parse_seed_method(R_seed_method);
	const scc_UnassignedMethod unassigned_method = iRscc_parse_unassigned_method(R_unassigned_method);
	const scc_UnassignedMethod secondary_unassigned_method = iRscc_parse_unassigned_method(R_secondary_unassigned_method);

	scc_RadiusMethod radius_constraint = false;
	double radius = 0.0;
	if (isReal(R_radius)) {
		radius_constraint = true;
		radius = asReal(R_radius);
	}

	scc_RadiusMethod primary_radius_constraint = SCC_RM_USE_SEED_RADIUS;

	scc_RadiusMethod secondary_radius_constraint = false;
	double secondary_radius = 0.0;
	if (isReal(R_secondary_radius)) {
		secondary_radius_constraint = true;
		secondary_radius = asReal(R_secondary_radius);
	}

	if (strcmp(CHAR(asChar(R_unassigned_method)), "estimated_radius_closest_seed") == 0) {
		primary_radius_constraint = SCC_RM_USE_ESTIMATED;
	}
	if (strcmp(CHAR(asChar(R_secondary_unassigned_method)), "estimated_radius_closest_seed") == 0) {
		secondary_radius_constraint = SCC_RM_USE_ESTIMATED;
	}

	size_t len_primary_data_points = 0;
	bool* primary_data_points = NULL;
	if (isLogical(R_primary_data_points)) {
		len_primary_data_points = (size_t) xlength(R_primary_data_points);
		if (len_primary_data_points < num_data_points) {
			iRscc_error("Invalid `R_primary_data_points`.");
		}
		primary_data_points = (bool*) R_alloc(len_primary_data_points, sizeof(bool)); // Automatically freed by R on return
		if (primary_data_points == NULL) iRscc_error("Could not allocate memory.");
		const int* const tmp_primary_data_points = LOGICAL(R_primary_data_points);
		for (size_t i = 0; i < len_primary_data_points; ++i) {
			primary_data_points[i] = (tmp_primary_data_points[i] == 1);
		}
	}

	scc_ErrorCode ec;
	scc_DataSet* data_set;
	if ((ec = scc_init_data_set(num_data_points,
	                            num_dimensions,
	                            (size_t) xlength(R_distance_object),
	                            REAL(R_distance_object),
	                            &data_set)) != SCC_ER_OK) {
		iRscc_scc_error();
	}

	SEXP R_cluster_labels = PROTECT(allocVector(INTSXP, (R_xlen_t) num_data_points));
	scc_Clustering* clustering;
	if ((ec = scc_init_empty_clustering(num_data_points,
	                                    INTEGER(R_cluster_labels),
	                                    &clustering)) != SCC_ER_OK) {
		scc_free_data_set(&data_set);
		UNPROTECT(1);
		iRscc_scc_error();
	}

	scc_ClusterOptions options = scc_default_cluster_options;

	options.size_constraint = size_constraint;
	options.seed_method = seed_method;
	options.len_primary_data_points = len_primary_data_points;
	options.primary_data_points = primary_data_points;
	options.primary_unassigned_method = unassigned_method;
	options.secondary_unassigned_method = secondary_unassigned_method;
	options.seed_radius = radius_constraint;
	options.seed_supplied_radius = radius;
	options.primary_radius = primary_radius_constraint;
	options.secondary_radius = secondary_radius_constraint;
	options.secondary_supplied_radius = secondary_radius;

	if ((ec = scc_make_clustering(data_set,
	                              clustering,
	                              &options)) != SCC_ER_OK) {
		scc_free_clustering(&clustering);
		scc_free_data_set(&data_set);
		UNPROTECT(1);
		iRscc_scc_error();
	}

	scc_free_data_set(&data_set);

	uintmax_t num_clusters = 0;
	if ((ec = scc_get_clustering_info(clustering,
	                                  NULL,
	                                  &num_clusters)) != SCC_ER_OK) {
		scc_free_clustering(&clustering);
		UNPROTECT(1);
		iRscc_scc_error();
	}

	scc_free_clustering(&clustering);

	if (num_clusters > INT_MAX) iRscc_error("Too many clusters.");
	const int num_clusters_int = (int) num_clusters;

	const SEXP R_clustering_obj = PROTECT(allocVector(VECSXP, 2));
	SET_VECTOR_ELT(R_clustering_obj, 0, R_cluster_labels);
	SET_VECTOR_ELT(R_clustering_obj, 1, ScalarInteger(num_clusters_int));

	const SEXP R_obj_elem_names = PROTECT(allocVector(STRSXP, 2));
	SET_STRING_ELT(R_obj_elem_names, 0, mkChar("cluster_labels"));
	SET_STRING_ELT(R_obj_elem_names, 1, mkChar("cluster_count"));
	setAttrib(R_clustering_obj, R_NamesSymbol, R_obj_elem_names);

	UNPROTECT(3);
	return R_clustering_obj;
}


SEXP Rscc_nng_clustering_batches(const SEXP R_distance_object,
                                 const SEXP R_size_constraint,
                                 const SEXP R_unassigned_method,
                                 const SEXP R_radius,
                                 const SEXP R_primary_data_points,
                                 const SEXP R_batch_size)
{
	if (!scc_set_ann_dist_search()) {
		iRscc_error("Cannot change NN search functions to ANN.");
	}
	if (!isMatrix(R_distance_object) || !isReal(R_distance_object)) {
		iRscc_error("`R_distance_object` is not a valid distance object.");
	}
	if (!isInteger(R_size_constraint)) {
		iRscc_error("`R_size_constraint` must be integer.");
	}
	if (!isString(R_unassigned_method)) {
		iRscc_error("`R_unassigned_method` must be string.");
	}
	if (!isNull(R_radius) && !isReal(R_radius)) {
		iRscc_error("`R_radius` must be NULL or double.");
	}
	if (!isNull(R_primary_data_points) && !isLogical(R_primary_data_points)) {
		iRscc_error("`R_primary_data_points` must be NULL or logical.");
	}
	if (!isInteger(R_batch_size)) {
		iRscc_error("`R_batch_size` must be integer.");
	}

	const uintmax_t num_data_points = (uintmax_t) INTEGER(getAttrib(R_distance_object, R_DimSymbol))[1];
	const uintmax_t num_dimensions = (uintmax_t) INTEGER(getAttrib(R_distance_object, R_DimSymbol))[0];
	const uint32_t size_constraint = (uint32_t) asInteger(R_size_constraint);
	const scc_UnassignedMethod unassigned_method = iRscc_parse_unassigned_method(R_unassigned_method);
	const uint32_t batch_size = (uint32_t) asInteger(R_batch_size);

	bool radius_constraint = false;
	double radius = 0.0;
	if (isReal(R_radius)) {
		radius_constraint = true;
		radius = asReal(R_radius);
	}

	size_t len_primary_data_points = 0;
	bool* primary_data_points = NULL;
	if (isLogical(R_primary_data_points)) {
		len_primary_data_points = (size_t) xlength(R_primary_data_points);
		if (len_primary_data_points < num_data_points) {
			iRscc_error("Invalid `R_primary_data_points`.");
		}
		primary_data_points = (bool*) R_alloc(len_primary_data_points, sizeof(bool)); // Automatically freed by R on return
		if (primary_data_points == NULL) iRscc_error("Could not allocate memory.");
		const int* const tmp_primary_data_points = LOGICAL(R_primary_data_points);
		for (size_t i = 0; i < len_primary_data_points; ++i) {
			primary_data_points[i] = (tmp_primary_data_points[i] == 1);
		}
	}

	scc_ErrorCode ec;
	scc_DataSet* data_set;
	if ((ec = scc_init_data_set(num_data_points,
	                            num_dimensions,
	                            (size_t) xlength(R_distance_object),
	                            REAL(R_distance_object),
	                            &data_set)) != SCC_ER_OK) {
		iRscc_scc_error();
	}

	SEXP R_cluster_labels = PROTECT(allocVector(INTSXP, (R_xlen_t) num_data_points));
	scc_Clustering* clustering;
	if ((ec = scc_init_empty_clustering(num_data_points,
	                                    INTEGER(R_cluster_labels),
	                                    &clustering)) != SCC_ER_OK) {
		scc_free_data_set(&data_set);
		UNPROTECT(1);
		iRscc_scc_error();
	}

	scc_ClusterOptions options = scc_default_cluster_options;

	options.size_constraint = size_constraint;
	options.seed_method = SCC_SM_BATCHES;
	options.len_primary_data_points = len_primary_data_points;
	options.primary_data_points = primary_data_points;
	options.primary_unassigned_method = unassigned_method;
	options.secondary_unassigned_method = SCC_UM_IGNORE;
	options.seed_radius = radius_constraint;
	options.seed_supplied_radius = radius;
	options.batch_size = batch_size;

	if ((ec = scc_make_clustering(data_set,
	                              clustering,
	                              &options)) != SCC_ER_OK) {
		scc_free_clustering(&clustering);
		scc_free_data_set(&data_set);
		UNPROTECT(1);
		iRscc_scc_error();
	}

	scc_free_data_set(&data_set);

	uintmax_t num_clusters = 0;
	if ((ec = scc_get_clustering_info(clustering,
	                                  NULL,
	                                  &num_clusters)) != SCC_ER_OK) {
		scc_free_clustering(&clustering);
		UNPROTECT(1);
		iRscc_scc_error();
	}

	scc_free_clustering(&clustering);

	if (num_clusters > INT_MAX) iRscc_error("Too many clusters.");
	const int num_clusters_int = (int) num_clusters;

	const SEXP R_clustering_obj = PROTECT(allocVector(VECSXP, 2));
	SET_VECTOR_ELT(R_clustering_obj, 0, R_cluster_labels);
	SET_VECTOR_ELT(R_clustering_obj, 1, ScalarInteger(num_clusters_int));

	const SEXP R_obj_elem_names = PROTECT(allocVector(STRSXP, 2));
	SET_STRING_ELT(R_obj_elem_names, 0, mkChar("cluster_labels"));
	SET_STRING_ELT(R_obj_elem_names, 1, mkChar("cluster_count"));
	setAttrib(R_clustering_obj, R_NamesSymbol, R_obj_elem_names);

	UNPROTECT(3);
	return R_clustering_obj;
}


SEXP Rscc_nng_clustering_types(const SEXP R_distance_object,
                               const SEXP R_type_labels,
                               const SEXP R_type_size_constraints,
                               const SEXP R_total_size_constraint,
                               const SEXP R_seed_method,
                               const SEXP R_unassigned_method,
                               const SEXP R_radius,
                               const SEXP R_primary_data_points,
                               const SEXP R_secondary_unassigned_method,
                               const SEXP R_secondary_radius)
{
	if (!scc_set_ann_dist_search()) {
		iRscc_error("Cannot change NN search functions to ANN.");
	}
	if (!isMatrix(R_distance_object) || !isReal(R_distance_object)) {
		iRscc_error("`R_distance_object` is not a valid distance object.");
	}
	if (!isInteger(R_type_labels)) {
		iRscc_error("`R_type_labels` must be factor or integer.");
	}
	if (!isInteger(R_type_size_constraints)) {
		iRscc_error("`R_type_size_constraints` must be integer.");
	}
	if (!isInteger(R_total_size_constraint)) {
		iRscc_error("`R_total_size_constraint` must be integer.");
	}
	if (!isString(R_seed_method)) {
		iRscc_error("`R_seed_method` must be string.");
	}
	if (!isString(R_unassigned_method)) {
		iRscc_error("`R_unassigned_method` must be string.");
	}
	if (!isNull(R_radius) && !isReal(R_radius)) {
		iRscc_error("`R_radius` must be NULL or double.");
	}
	if (!isNull(R_primary_data_points) && !isLogical(R_primary_data_points)) {
		iRscc_error("`R_primary_data_points` must be NULL or logical.");
	}
	if (!isString(R_secondary_unassigned_method)) {
		iRscc_error("`R_secondary_unassigned_method` must be string.");
	}
	if (!isNull(R_secondary_radius) && !isReal(R_secondary_radius)) {
		iRscc_error("`R_secondary_radius` must be NULL or double.");
	}

	const uintmax_t num_data_points = (uintmax_t) INTEGER(getAttrib(R_distance_object, R_DimSymbol))[1];
	const uintmax_t num_dimensions = (uintmax_t) INTEGER(getAttrib(R_distance_object, R_DimSymbol))[0];
	const size_t len_type_labels = (size_t) xlength(R_type_labels);
	const int* const type_labels = INTEGER(R_type_labels);
	const uint32_t total_size_constraint = (uint32_t) asInteger(R_total_size_constraint);
	const scc_SeedMethod seed_method = iRscc_parse_seed_method(R_seed_method);
	const scc_UnassignedMethod unassigned_method = iRscc_parse_unassigned_method(R_unassigned_method);
	const scc_UnassignedMethod secondary_unassigned_method = iRscc_parse_unassigned_method(R_secondary_unassigned_method);

	if (len_type_labels != num_data_points) {
		iRscc_error("`R_type_labels` does not match `R_distance_object`.");
	}

	const uintmax_t num_types = (uintmax_t) xlength(R_type_size_constraints);
	uint32_t* const type_size_constraints = (uint32_t*) R_alloc(num_types, sizeof(uint32_t)); // Automatically freed by R on return
	if (type_size_constraints == NULL) iRscc_error("Could not allocate memory.");
	const int* const tmp_type_size_constraints = INTEGER(R_type_size_constraints);
	for (size_t i = 0; i < num_types; ++i) {
		if (tmp_type_size_constraints[i] < 0) {
		  iRscc_error("Negative type size constraint.");
		}
		type_size_constraints[i] = (uint32_t) tmp_type_size_constraints[i];
	}

	scc_RadiusMethod radius_constraint = false;
	double radius = 0.0;
	if (isReal(R_radius)) {
		radius_constraint = true;
		radius = asReal(R_radius);
	}

	scc_RadiusMethod primary_radius_constraint = SCC_RM_USE_SEED_RADIUS;

	scc_RadiusMethod secondary_radius_constraint = false;
	double secondary_radius = 0.0;
	if (isReal(R_secondary_radius)) {
		secondary_radius_constraint = true;
		secondary_radius = asReal(R_secondary_radius);
	}

	if (strcmp(CHAR(asChar(R_unassigned_method)), "estimated_radius_closest_seed") == 0) {
		primary_radius_constraint = SCC_RM_USE_ESTIMATED;
	}
	if (strcmp(CHAR(asChar(R_secondary_unassigned_method)), "estimated_radius_closest_seed") == 0) {
		secondary_radius_constraint = SCC_RM_USE_ESTIMATED;
	}

	size_t len_primary_data_points = 0;
	bool* primary_data_points = NULL;
	if (isLogical(R_primary_data_points)) {
		len_primary_data_points = (size_t) xlength(R_primary_data_points);
		if (len_primary_data_points < num_data_points) {
			iRscc_error("Invalid `R_primary_data_points`.");
		}
		primary_data_points = (bool*) R_alloc(len_primary_data_points, sizeof(bool)); // Automatically freed by R on return
		if (primary_data_points == NULL) iRscc_error("Could not allocate memory.");
		const int* const tmp_primary_data_points = LOGICAL(R_primary_data_points);
		for (size_t i = 0; i < len_primary_data_points; ++i) {
			primary_data_points[i] = (tmp_primary_data_points[i] == 1);
		}
	}

	scc_ErrorCode ec;
	scc_DataSet* data_set;
	if ((ec = scc_init_data_set(num_data_points,
	                            num_dimensions,
	                            (size_t) xlength(R_distance_object),
	                            REAL(R_distance_object),
	                            &data_set)) != SCC_ER_OK) {
		iRscc_scc_error();
	}

	SEXP R_cluster_labels = PROTECT(allocVector(INTSXP, (R_xlen_t) num_data_points));
	scc_Clustering* clustering;
	if ((ec = scc_init_empty_clustering(num_data_points,
	                                    INTEGER(R_cluster_labels),
	                                    &clustering)) != SCC_ER_OK) {
		scc_free_data_set(&data_set);
		UNPROTECT(1);
		iRscc_scc_error();
	}

	scc_ClusterOptions options = scc_default_cluster_options;

	options.size_constraint = total_size_constraint;
	options.num_types = num_types;
	options.type_constraints = type_size_constraints;
	options.len_type_labels = len_type_labels;
	options.type_labels = type_labels;
	options.seed_method = seed_method;
	options.len_primary_data_points = len_primary_data_points;
	options.primary_data_points = primary_data_points;
	options.primary_unassigned_method = unassigned_method;
	options.secondary_unassigned_method = secondary_unassigned_method;
	options.seed_radius = radius_constraint;
	options.seed_supplied_radius = radius;
	options.primary_radius = primary_radius_constraint;
	options.secondary_radius = secondary_radius_constraint;
	options.secondary_supplied_radius = secondary_radius;

	if ((ec = scc_make_clustering(data_set,
	                              clustering,
	                              &options)) != SCC_ER_OK) {
		scc_free_clustering(&clustering);
		scc_free_data_set(&data_set);
		UNPROTECT(1);
		iRscc_scc_error();
	}

	scc_free_data_set(&data_set);

	uintmax_t num_clusters = 0;
	if ((ec = scc_get_clustering_info(clustering,
	                                  NULL,
	                                  &num_clusters)) != SCC_ER_OK) {
		scc_free_clustering(&clustering);
		UNPROTECT(1);
		iRscc_scc_error();
	}

	scc_free_clustering(&clustering);

	if (num_clusters > INT_MAX) iRscc_error("Too many clusters.");
	const int num_clusters_int = (int) num_clusters;

	const SEXP R_clustering_obj = PROTECT(allocVector(VECSXP, 2));
	SET_VECTOR_ELT(R_clustering_obj, 0, R_cluster_labels);
	SET_VECTOR_ELT(R_clustering_obj, 1, ScalarInteger(num_clusters_int));

	const SEXP R_obj_elem_names = PROTECT(allocVector(STRSXP, 2));
	SET_STRING_ELT(R_obj_elem_names, 0, mkChar("cluster_labels"));
	SET_STRING_ELT(R_obj_elem_names, 1, mkChar("cluster_count"));
	setAttrib(R_clustering_obj, R_NamesSymbol, R_obj_elem_names);

	UNPROTECT(3);
	return R_clustering_obj;
}


// =============================================================================
// Internal function implementations
// =============================================================================

static scc_SeedMethod iRscc_parse_seed_method(const SEXP R_seed_method)
{
	if (!isString(R_seed_method)) iRscc_error("`R_seed_method` must be string.");

	const char* seed_method_string = CHAR(asChar(R_seed_method));
	if (strcmp(seed_method_string, "lexical") == 0) {
		return SCC_SM_LEXICAL;
	} else if (strcmp(seed_method_string, "inwards_order") == 0) {
		return SCC_SM_INWARDS_ORDER;
	} else if (strcmp(seed_method_string, "inwards_updating") == 0) {
		return SCC_SM_INWARDS_UPDATING;
	} else if (strcmp(seed_method_string, "inwards_alt_updating") == 0) {
		return SCC_SM_INWARDS_ALT_UPDATING;
	} else if (strcmp(seed_method_string, "exclusion_order") == 0) {
		return SCC_SM_EXCLUSION_ORDER;
	} else if (strcmp(seed_method_string, "exclusion_updating") == 0) {
		return SCC_SM_EXCLUSION_UPDATING;
	} else {
		iRscc_error("Not a valid seed method.");
	}

	return 999; // Unreachable, but needed to silence compiler warning
}


static scc_UnassignedMethod iRscc_parse_unassigned_method(const SEXP R_unassigned_method)
{
	if (!isString(R_unassigned_method)) iRscc_error("`R_unassigned_method` must be string.");

	const char* unassigned_method_string = CHAR(asChar(R_unassigned_method));
	if (strcmp(unassigned_method_string, "ignore") == 0) {
		return SCC_UM_IGNORE;
	} else if (strcmp(unassigned_method_string, "by_nng") == 0) {
		return SCC_UM_ANY_NEIGHBOR;
	} else if (strcmp(unassigned_method_string, "closest_assigned") == 0) {
		return SCC_UM_CLOSEST_ASSIGNED;
	} else if (strcmp(unassigned_method_string, "closest_seed") == 0) {
		return SCC_UM_CLOSEST_SEED;
	} else if (strcmp(unassigned_method_string, "estimated_radius_closest_seed") == 0) {
		return SCC_UM_CLOSEST_SEED;
	} else {
		iRscc_error("Not a valid unassigned method.");
	}

	return 999; // Unreachable, but needed to silence compiler warning
}
