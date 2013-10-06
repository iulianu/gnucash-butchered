
#include "qof.h"

QofSession * gnc_get_current_session (void);
void gnc_clear_current_session(void);
void gnc_set_current_session (QofSession *session);
bool gnc_current_session_exist(void);
