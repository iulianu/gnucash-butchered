#include "qof.h"


		#define GncObject_t QofObject
		#define gncObjectLookup qof_object_lookup
		#define gncObjectRegister qof_object_register
		#define gncObjectGetTypeLabel qof_object_get_type_label
		#define gncObjectRegisterBackend qof_object_register_backend
		#define gncObjectLookupBackend qof_object_lookup_backend
		#define gncObjectForeachBackend qof_object_foreach_backend

		#define gncObjectInitialize qof_object_initialize
		#define gncObjectShutdown qof_object_shutdown
		#define gncObjectBookBegin qof_object_book_begin
		#define gncObjectBookEnd qof_object_book_end
		#define gncObjectIsDirty qof_object_is_dirty
		#define gncObjectMarkClean qof_object_mark_clean

		#define gncObjectForeachType qof_object_foreach_type
		#define gncObjectForeach qof_object_foreach
		#define gncObjectPrintable qof_object_printable
