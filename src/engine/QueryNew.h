
#include "qofquery.h"


		#define QUERY_AND QOF_QUERY_AND
		#define QUERY_OR QOF_QUERY_OR
		#define QUERY_NAND QOF_QUERY_NAND
		#define QUERY_NOR QOF_QUERY_NOR
		#define QUERY_XOR QOF_QUERY_XOR
		#define QUERY_PARAM_BOOK QOF_QUERY_PARAM_BOOK
		#define QUERY_PARAM_GUID QOF_QUERY_PARAM_GUID
		#define QUERY_PARAM_ACTIVE QOF_QUERY_PARAM_ACTIVE

		#define querynew_s _QofQuery
		#define QueryNew QofQuery
		#define QueryOp QofQueryOp
		#define query_new_term _QofQueryTerm
		#define query_new_sort _QofQuerySort

		#define gncQueryBuildParamList qof_query_build_param_list
		#define gncQueryCreate qof_query_create
		#define gncQueryCreateFor qof_query_create_for
		#define gncQueryDestroy qof_query_destroy
		#define gncQuerySearchFor qof_query_search_for
		#define gncQuerySetBook qof_query_set_book
		#define gncQueryAddTerm qof_query_add_term
		#define gncQueryAddGUIDMatch qof_query_add_guid_match
		#define gncQueryAddGUIDListMatch qof_query_add_guid_list_match
		#define gncQueryAddBooleanMatch qof_query_add_boolean_match
		#define gncQueryRun qof_query_run
		#define gncQueryLastRun qof_query_last_run
		#define gncQueryClear qof_query_clear
		#define gncQueryPurgeTerms qof_query_purge_terms
		#define gncQueryHasTerms qof_query_has_terms
		#define gncQueryNumTerms qof_query_num_terms
		#define gncQueryHasTermType qof_query_has_term_type
		#define gncQueryCopy qof_query_copy
		#define gncQueryInvert qof_query_invert
		#define gncQueryMerge qof_query_merge
		#define gncQueryMergeInPlace qof_query_merge_in_place
		#define gncQuerySetSortOrder qof_query_set_sort_order
		#define gncQuerySetSortOptions qof_query_set_sort_options
		#define gncQuerySetSortIncreasing qof_query_set_sort_increasing
		#define gncQuerySetMaxResults qof_query_set_max_results
		#define gncQueryEqual qof_query_equal
		#define gncQueryPrint qof_query_print
		#define gncQueryGetSearchFor qof_query_get_search_for
		#define gncQueryGetBooks qof_query_get_books

