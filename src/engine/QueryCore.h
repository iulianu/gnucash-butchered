
#include "qofquerycore.h"

		#define QueryPredData_t QofQueryPredData*

		#define gncQueryStringPredicate qof_query_string_predicate
		#define gncQueryDatePredicate qof_query_date_predicate
		#define gncQueryNumericPredicate qof_query_numeric_predicate
		#define gncQueryGUIDPredicate qof_query_guid_predicate
		#define gncQueryInt32Predicate qof_query_int32_predicate
		#define gncQueryInt64Predicate qof_query_int64_predicate
		#define gncQueryDoublePredicate qof_query_double_predicate
		#define gncQueryBooleanPredicate qof_query_boolean_predicate
		#define gncQueryCharPredicate qof_query_char_predicate
		#define gncQueryKVPPredicate qof_query_kvp_predicate
		#define gncQueryCorePredicateFree qof_query_core_predicate_free

		#define COMPARE_LT QOF_COMPARE_LT
		#define COMPARE_LTE QOF_COMPARE_LTE
		#define COMPARE_EQUAL QOF_COMPARE_EQUAL
		#define COMPARE_GT QOF_COMPARE_GT
		#define COMPARE_GTE QOF_COMPARE_GTE
		#define COMPARE_NEQ QOF_COMPARE_NEQ

		#define STRING_MATCH_NORMAL QOF_STRING_MATCH_NORMAL
		#define STRING_MATCH_CASEINSENSITIVE QOF_STRING_MATCH_CASEINSENSITIVE

		#define DATE_MATCH_NORMAL QOF_DATE_MATCH_NORMAL
		#define DATE_MATCH_ROUNDED QOF_DATE_MATCH_ROUNDED

		#define NUMERIC_MATCH_ANY QOF_NUMERIC_MATCH_ANY
		#define NUMERIC_MATCH_CREDIT QOF_NUMERIC_MATCH_CREDIT
		#define NUMERIC_MATCH_DEBIT QOF_NUMERIC_MATCH_DEBIT

		#define GUID_MATCH_ANY QOF_GUID_MATCH_ANY
		#define GUID_MATCH_NONE QOF_GUID_MATCH_NONE
		#define GUID_MATCH_NULL QOF_GUID_MATCH_NULL
		#define GUID_MATCH_ALL QOF_GUID_MATCH_ALL
		#define GUID_MATCH_LIST_ANY QOF_GUID_MATCH_LIST_ANY

		#define CHAR_MATCH_ANY QOF_CHAR_MATCH_ANY
		#define CHAR_MATCH_NONE QOF_CHAR_MATCH_NONE

		#define char_match_t QofCharMatch
		#define guid_match_t QofGuidMatch
		#define numeric_match_t QofNumericMatch
		#define date_match_t QofDateMatch
		#define string_match_t QofStringMatch
		#define query_compare_t QofQueryCompare

		#define gncQueryCoreInit qof_query_core_init
		#define gncQueryCoreShutdown qof_query_core_shutdown
		#define gncQueryCoreGetPredicate qof_query_core_get_predicate
		#define gncQueryCoreGetCompare qof_query_core_get_compare
		
		#define gncQueryCorePredicateEqual qof_query_core_predicate_equal
		
		#define QUERYCORE_DEBCRED QOF_QUERYCORE_DEBCRED
		#define QUERYCORE_BOOLEAN QOF_QUERYCORE_BOOLEAN
		#define QUERYCORE_NUMERIC QOF_QUERYCORE_NUMERIC
		#define QUERYCORE_STRING QOF_QUERYCORE_STRING
		#define QUERYCORE_DATE QOF_QUERYCORE_DATE
		#define QUERYCORE_INT64 QOF_QUERYCORE_INT64
		#define QUERYCORE_DOUBLE QOF_QUERYCORE_DOUBLE

		#define QueryAccess QofQueryAccess
		#define gncQueryCoreToString qof_query_core_to_string
