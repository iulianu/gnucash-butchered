#! /usr/bin/perl
#
# FUNCTION: this utility script converts old code to the new
#           QOF routine names, and does other related cleanup
#
# USAGE: ls *.c *.h | ./xlate.pl ; make ; cvs commit
#        
# AUTHOR: Linas Vepstas <linas@linas.org> June 2003

@files = <STDIN>;

# print @files;

foreach (@files)
{
	chop;
	$afile= $_;
	open (AF, $afile);
	open (OF, "> " . $afile . ".tmp");

	while (<AF>)
	{

		if (0) {
		s/GNCBook/QofBook/g;
		s/gnc_book_not_saved/qof_book_not_saved/g;
		s/gnc_book_get_guid/qof_book_get_guid/g;
		s/BookList/QofBookList/g;
		s/gnc_book_get_slots/qof_book_get_slots/g;
		s/gnc_book_set_guid/qof_book_set_guid/g;
		s/gnc_book_new/qof_book_new/g;
		

		s/GncObject_t/QofObject/g;
		s/gncObjectLookup/qof_object_lookup/g;
		s/gncObjectRegister/qof_object_register/g;
		s/gncObjectGetTypeLabel/qof_object_get_type_label/g;
		s/gncObjectRegisterBackend/qof_object_register_backend/g;
		s/gncObjectLookupBackend/qof_object_lookup_backend/g;
		s/gncObjectForeachBackend/qof_object_foreach_backend/g;
		s/gncObjectInitialize/qof_object_initialize/g;

		s/gncObjectShutdown/qof_object_shutdown/g;
		s/gncObjectBookBegin/qof_object_book_begin/g;
		s/gncObjectBookEnd/qof_object_book_end/g;
		s/gncObjectIsDirty/qof_object_is_dirty/g;
		s/gncObjectMarkClean/qof_object_mark_clean/g;

		s/gncObjectForeachType/qof_object_foreach_type/g;
		s/gncObjectForeach/qof_object_foreach/g;
		s/gncObjectPrintable/qof_object_printable/g;

		s/foreachTypeCB/QofForeachTypeCB/g;
		s/foreachBackendTypeCB/QofForeachBackendTypeCB/g;

		s/gncQueryStringPredicate/qof_query_string_predicate/g;
		s/gncQueryDatePredicate/qof_query_date_predicate/g;
		s/gncQueryNumericPredicate/qof_query_numeric_predicate/g;
		s/gncQueryGUIDPredicate/qof_query_guid_predicate/g;
		s/gncQueryInt32Predicate/qof_query_int32_predicate/g;
		s/gncQueryInt64Predicate/qof_query_int64_predicate/g;
		s/gncQueryDoublePredicate/qof_query_double_predicate/g;
		s/gncQueryBooleanPredicate/qof_query_boolean_predicate/g;
		s/gncQueryCharPredicate/qof_query_char_predicate/g;
		s/gncQueryKVPPredicate/qof_query_kvp_predicate/g;
		s/gncQueryCorePredicateFree/qof_query_core_predicate_free/g;

		s/COMPARE_LT/QOF_COMPARE_LT/g;
		s/COMPARE_LTE/QOF_COMPARE_LTE/g;
		s/COMPARE_EQUAL/QOF_COMPARE_EQUAL/g;
		s/COMPARE_GT/QOF_COMPARE_GT/g;
		s/COMPARE_GTE/QOF_COMPARE_GTE/g;
		s/COMPARE_NEQ/QOF_COMPARE_NEQ/g;

		s/STRING_MATCH_NORMAL/QOF_STRING_MATCH_NORMAL/g;
		s/STRING_MATCH_CASEINSENSITIVE/QOF_STRING_MATCH_CASEINSENSITIVE/g;

		s/DATE_MATCH_NORMAL/QOF_DATE_MATCH_NORMAL/g;
		s/DATE_MATCH_ROUNDED/QOF_DATE_MATCH_ROUNDED/g;

		s/NUMERIC_MATCH_ANY/QOF_NUMERIC_MATCH_ANY/g;
		s/NUMERIC_MATCH_CREDIT/QOF_NUMERIC_MATCH_CREDIT/g;
		s/NUMERIC_MATCH_DEBIT/QOF_NUMERIC_MATCH_DEBIT/g;

		s/GUID_MATCH_ANY/QOF_GUID_MATCH_ANY/g;
		s/GUID_MATCH_NONE/QOF_GUID_MATCH_NONE/g;
		s/GUID_MATCH_NULL/QOF_GUID_MATCH_NULL/g;
		s/GUID_MATCH_ALL/QOF_GUID_MATCH_ALL/g;
		s/GUID_MATCH_LIST_ANY/QOF_GUID_MATCH_LIST_ANY/g;

		s/CHAR_MATCH_ANY/QOF_CHAR_MATCH_ANY/g;
		s/CHAR_MATCH_NONE/QOF_CHAR_MATCH_NONE/g;

		s/char_match_t/QofCharMatch/g;
		s/guid_match_t/QofGuidMatch/g;
		s/numeric_match_t/QofNumericMatch/g;
		s/date_match_t/QofDateMatch/g;
		s/string_match_t/QofStringMatch/g;
		s/query_compare_t/QofQueryCompare/g;

		s/gncQueryCoreInit/qof_query_core_init/g;
		s/gncQueryCoreShutdown/qof_query_core_shutdown/g;
		s/gncQueryCoreGetPredicate/qof_query_core_get_predicate/g;
		s/gncQueryCoreGetCompare/qof_query_core_get_compare/g;
		
		s/gncQueryCorePredicateEqual/qof_query_core_predicate_equal/g;

		s/QUERY_AND/QOF_QUERY_AND/g;
		s/QUERY_OR/QOF_QUERY_OR/g;
		s/QUERY_NAND/QOF_QUERY_NAND/g;
		s/QUERY_NOR/QOF_QUERY_NOR/g;
		s/QUERY_XOR/QOF_QUERY_XOR/g;
		s/QUERY_PARAM_BOOK/QOF_QUERY_PARAM_BOOK/g;
		s/QUERY_PARAM_GUID/QOF_QUERY_PARAM_GUID/g;
		s/QUERY_PARAM_ACTIVE/QOF_QUERY_PARAM_ACTIVE/g;

		s/QUERYCORE_INT64/QOF_QUERYCORE_INT64/g;
		s/QUERYCORE_STRING/QOF_QUERYCORE_STRING/g;
		s/QUERYCORE_GUID/QOF_QUERYCORE_GUID/g;
		s/QUERYCORE_DATE/QOF_QUERYCORE_DATE/g;
		s/QUERYCORE_BOOLEAN/QOF_QUERYCORE_BOOLEAN/g;
		s/QUERYCORE_NUMERIC/QOF_QUERYCORE_NUMERIC/g;

		s/gncQueryBuildParamList/qof_query_build_param_list/g;
		s/gncQueryCreateFor/qof_query_create_for/g;
		s/gncQueryCreate/qof_query_create/g;
		s/gncQueryDestroy/qof_query_destroy/g;
		s/gncQuerySearchFor/qof_query_search_for/g;
		s/gncQuerySetBook/qof_query_set_book/g;
		s/gncQueryAddTerm/qof_query_add_term/g;
		s/gncQueryAddGUIDMatch/qof_query_add_guid_match/g;
		s/gncQueryAddGUIDListMatch/qof_query_add_guid_list_match/g;
		s/gncQueryAddBooleanMatch/qof_query_add_boolean_match/g;
		s/gncQueryRun/qof_query_run/g;
		s/gncQueryLastRun/qof_query_last_run/g;
		s/gncQueryClear/qof_query_clear/g;
		s/gncQueryPurgeTerms/qof_query_purge_terms/g;
		s/gncQueryHasTerms/qof_query_has_terms/g;
		s/gncQueryNumTerms/qof_query_num_terms/g;
		s/gncQueryHasTermType/qof_query_has_term_type/g;
		s/gncQueryCopy/qof_query_copy/g;
		s/gncQueryInvert/qof_query_invert/g;
		s/gncQueryMerge/qof_query_merge/g;
		s/gncQueryMergeInPlace/qof_query_merges_ins_place/g;
		s/gncQuerySetSortOrder/qof_query_set_sort_order/g;
		s/gncQuerySetSortOptions/qof_query_set_sort_options/g;
		s/gncQuerySetSortIncreasing/qof_query_set_sort_increasing/g;
		s/gncQuerySetMaxResults/qof_query_set_max_results/g;
		s/gncQueryEqual/qof_query_equal/g;
		s/gncQueryPrint/qof_query_print/g;
		s/gncQueryGetSearchFor/qof_query_get_search_for/g;
		s/gncQueryGetBooks/qof_query_get_books/g;

		s/gncQueryNewInit/qof_query_new_init/g;
		s/gncQueryNewShutdown/qof_query_new_shutdown/g;
		s/gncQueryGetMaxResults/qof_query_get_max_results/g;
		s/gncQueryGetTerms/qof_query_get_terms/g;
		s/gncQueryTermGetParamPath/qof_query_term_get_param_path/g;
		s/gncQueryTermGetPredData/qof_query_term_get_pred_data/g;
		s/gncQueryTermIsInverted/qof_query_term_is_inverted/g;
		s/gncQueryGetSorts/qof_query_get_sorts/g;
		s/gncQuerySortGetParamPath/qof_query_sort_get_param_path/g;
		s/gncQuerySortGetSortOptions/qof_query_sort_get_sort_options/g;
		s/gncQuerySortGetIncreasing/qof_query_sort_get_increasing/g;

		s/QuerySort/QofSortFunc/g;

		s/querynew_s/_QofQuery/g;
		s/QueryNew/QofQuery/g;
		s/QueryOp/QofQueryOp/g;
		s/query_new_term/_QofQueryTerm/g;
		s/query_new_sort/_QofQuerySort/g;

		s/query_object_def/_QofQueryObject/g;
		s/QueryObjectDef/QofQueryObject/g;

		s/QueryAccess/QofAccessFunc/g;

		s/gncQueryObjectRegister/qof_query_object_register/g;
		s/gncQueryObjectParameterType/qof_query_object_parameter_type/g;
		s/gncQueryObjectGetParameterGetter/qof_query_object_get_parameter_getter/g;
		s/gncQueryObjectGetParameter/qof_query_object_get_parameter/g;
		s/gncQueryObjectInit/qof_query_object_init/g;
		s/gncQueryObjectShutdown/qof_query_object_shutdown/g;
		s/gncQueryObjectDefaultSort/qof_query_object_default_sort/g;

		s/xaccGUIDNULL/guid_null/g;
		s/xaccGUIDMalloc/guid_malloc/g;
		s/xaccGUIDFree/guid_free/g;

		s/GNCIdTypeConst/QofIdTypeConst/g;
		s/GNCIdType/QofIdType/g;
		s/GNCEntityTable/QofEntityTable/g;
		s/xaccGUIDTypeEntityTable/qof_guid_type/g;

		s/xaccEntityTableNew/qof_entity_new/g;
		s/xaccEntityTableDestroy/qof_entity_destroy/g;
		s/xaccGUIDNewEntityTable/qof_entity_guid_new/g;
		s/xaccLookupEntity/qof_entity_lookup/g;
		s/xaccStoreEntity/qof_entity_store/g;
		s/xaccRemoveEntity/qof_entity_remove/g;
		s/xaccForeachEntity/qof_entity_foreach/g;

		s/foreachObjectCB/QofEntityForeachCB/g;
		s/GNC_OBJECT_VERSION/QOF_OBJECT_VERSION/g;

		s/GNCSession/QofSession/g;
		s/gnc_session/qof_session/g;
		s/GNCPercentageFunc/QofPercentageFunc/g;
		s/gnc_get_current_session/qof_session_get_current_session/g;
		s/gnc_set_current_session/qof_session_set_current_session/g;

      s/ Backend/ QofBackend/g;
      s/\(Backend/\(QofBackend/g;
		s/GNCBackendError/QofBackendError/g;
		s/GNCBePercentageFunc/QofBePercentageFunc/g;
		s/xaccBackendSetError/qof_backend_set_error/g;
		s/xaccBackendGetError/qof_backend_get_error/g;
		s/xaccBackendSetMessage/qof_backend_set_message/g;
		s/xaccBackendGetMessage/qof_backend_get_message/g;
		s/xaccInitBackend/qof_backend_init/g;

		s/GNCId\.h/qofid\.h/g;
		s/gncObject\.h/qofobject\.h/g;
		s/QueryCore\.h/qofquerycore\.h/g;
		s/QofQuery\.h/qofquery\.h/g;
		s/QueryObject\.h/qofqueryobject\.h/g;


		s/kvp_frame /KvpFrame /g;
		s/kvp_frame* /KvpFrame* /g;
		s/kvp_value /KvpValue /g;
		s/kvp_value* /KvpValue* /g;
		s/kvp_value_t/KvpValueType/g;

		}
		s/QofQueryTerm_t/QofQueryTerm/g;
		s/QofSortFunc_t/QofQuerySort/g;

		s/QofQof/Qof/g;
		s/QOF_QOF/QOF/g;

		print OF $_;
	}
   close OF;
   close AF;

	$rn = "mv " . $afile . ".tmp " . $afile;
	system ($rn);
}
