/*
 * $Id: CreateTask3069Logins.sql,v 1.1 2009-09-23 18:51:21 bkline Exp $
 */

USE cdr_task3069
IF EXISTS (SELECT * 
             FROM dbo.sysusers 
            WHERE name = 'cdr' 
              AND UID < 16382)
	EXEC sp_revokedbaccess 'cdr'
GO

IF EXISTS (SELECT * 
             FROM dbo.sysusers 
            WHERE name = 'CdrGuest' 
              AND UID < 16382)
	EXEC sp_revokedbaccess 'CdrGuest'
GO

IF EXISTS (SELECT * 
             FROM dbo.sysusers 
            WHERE name = 'CdrPublishing' 
              AND UID < 16382)
	EXEC sp_revokedbaccess 'CdrPublishing'
GO
GO
EXEC sp_grantdbaccess 'CdrGuest'
GO
EXEC sp_grantdbaccess 'CdrPublishing'
GO

/*
 * Make cdr database owner.
 */
EXEC sp_changedbowner 'cdr'
GO

/*
 * Grant specific rights to the other two new users.
 */

GRANT SELECT ON usr TO CdrGuest
GO
GRANT SELECT ON usr TO CdrPublishing
GO
GRANT SELECT ON session TO CdrGuest
GO
GRANT SELECT ON session TO CdrPublishing
GO
GRANT SELECT ON format TO CdrGuest
GO
GRANT SELECT ON format TO CdrPublishing
GO
GRANT SELECT ON doc_type TO CdrGuest
GO
GRANT SELECT ON doc_type TO CdrPublishing
GO
GRANT SELECT ON action TO CdrGuest
GO
GRANT SELECT ON action TO CdrPublishing
GO
GRANT SELECT ON grp TO CdrGuest
GO
GRANT SELECT ON grp TO CdrPublishing
GO
GRANT SELECT ON doc_status TO CdrGuest
GO
GRANT SELECT ON doc_status TO CdrPublishing
GO
GRANT SELECT ON active_status TO CdrGuest
GO
GRANT SELECT ON active_status TO CdrPublishing
GO
GRANT SELECT ON all_docs TO CdrGuest
GO
GRANT SELECT ON all_docs TO CdrPublishing
GO
GRANT SELECT ON ready_for_review TO CdrGuest
GO
GRANT SELECT ON ready_for_review TO CdrPublishing
GO
GRANT SELECT ON checkout TO CdrGuest
GO
GRANT SELECT, UPDATE, INSERT ON checkout TO CdrPublishing
GO
GRANT SELECT ON doc_blob TO CdrGuest
GO
GRANT SELECT ON doc_blob TO CdrPublishing
GO
GRANT SELECT ON audit_trail TO CdrGuest
GO
GRANT SELECT ON audit_trail TO CdrPublishing
GO
GRANT SELECT ON debug_log TO CdrGuest
GO
GRANT SELECT, INSERT ON debug_log TO CdrPublishing
GO
GRANT SELECT ON doc_version TO CdrGuest
GO
GRANT SELECT ON doc_version TO CdrPublishing
GO
GRANT SELECT ON version_label TO CdrGuest
GO
GRANT SELECT ON version_label TO CdrPublishing
GO
GRANT SELECT ON doc_version_label TO CdrGuest
GO
GRANT SELECT ON doc_version_label TO CdrPublishing
GO
GRANT SELECT ON doc_blob_usage TO CdrGuest
GO
GRANT SELECT ON doc_blob_usage TO CdrPublishing
GO
GRANT SELECT ON version_blob_usage TO CdrGuest
GO
GRANT SELECT ON version_blob_usage TO CdrPublishing
GO
GRANT SELECT ON grp_action TO CdrGuest
GO
GRANT SELECT ON grp_action TO CdrPublishing
GO
GRANT SELECT ON grp_usr TO CdrGuest
GO
GRANT SELECT ON grp_usr TO CdrPublishing
GO
GRANT SELECT ON link_type TO CdrGuest
GO
GRANT SELECT ON link_type TO CdrPublishing
GO
GRANT SELECT ON link_xml TO CdrGuest
GO
GRANT SELECT ON link_xml TO CdrPublishing
GO
GRANT SELECT ON link_target TO CdrGuest
GO
GRANT SELECT ON link_target TO CdrPublishing
GO
GRANT SELECT ON link_prop_type TO CdrGuest
GO
GRANT SELECT ON link_prop_type TO CdrPublishing
GO
GRANT SELECT ON link_properties TO CdrGuest
GO
GRANT SELECT ON link_properties TO CdrPublishing
GO
GRANT SELECT ON link_net TO CdrGuest
GO
GRANT SELECT ON link_net TO CdrPublishing
GO
GRANT SELECT ON link_fragment TO CdrGuest
GO
GRANT SELECT ON link_fragment TO CdrPublishing
GO
GRANT SELECT ON query_term TO CdrGuest
GO
GRANT SELECT ON query_term TO CdrPublishing
GO
GRANT SELECT ON query_term_rule TO CdrGuest
GO
GRANT SELECT ON query_term_rule TO CdrPublishing
GO
GRANT SELECT ON query_term_def TO CdrGuest
GO
GRANT SELECT ON query_term_def TO CdrPublishing
GO
GRANT SELECT ON query_term_pub TO CdrGuest
GO
GRANT SELECT ON query_term_pub TO CdrPublishing
GO
GRANT SELECT ON dev_task_status TO CdrGuest
GO
GRANT SELECT ON dev_task TO CdrGuest
GO
GRANT SELECT ON issue_priority TO CdrGuest
GO
GRANT SELECT ON issue_user TO CdrGuest
GO
GRANT SELECT ON issue TO CdrGuest
GO
GRANT SELECT ON pub_proc TO CdrGuest
GO
GRANT SELECT, UPDATE, INSERT ON pub_proc TO CdrPublishing
GO
GRANT SELECT ON pub_proc_parm TO CdrGuest
GO
GRANT SELECT, UPDATE, INSERT ON pub_proc_parm TO CdrPublishing
GO
GRANT SELECT ON pub_proc_doc TO CdrGuest
GO
GRANT SELECT, UPDATE, INSERT ON pub_proc_doc TO CdrPublishing
GO
GRANT SELECT ON pub_proc_cg TO CdrGuest
GO
GRANT SELECT, UPDATE, INSERT, DELETE ON pub_proc_cg TO CdrPublishing
GO
GRANT SELECT ON pub_proc_cg_work TO CdrGuest
GO
GRANT SELECT, UPDATE, INSERT, DELETE ON pub_proc_cg_work TO CdrPublishing
GO
GRANT SELECT ON remailer_ids TO CdrGuest
GO
GRANT SELECT, UPDATE, INSERT, DELETE ON remailer_ids TO CdrPublishing
GO
GRANT SELECT ON active_doc TO CdrGuest
GO
GRANT SELECT ON active_doc TO CdrPublishing
GO
GRANT SELECT ON control_docs TO CdrGuest
GO
GRANT SELECT ON control_docs TO CdrPublishing
GO
GRANT SELECT ON creation_date TO CdrGuest
GO
GRANT SELECT ON creation_date TO CdrPublishing
GO
GRANT SELECT ON deleted_doc TO CdrGuest
GO
GRANT SELECT ON deleted_doc TO CdrPublishing
GO
GRANT SELECT, UPDATE, INSERT ON document TO CdrPublishing
GO
GRANT SELECT ON document TO CdrGuest
GO
GRANT SELECT ON doc_created TO CdrGuest
GO
GRANT SELECT ON doc_created TO CdrPublishing
GO
GRANT SELECT ON doc_header TO CdrGuest
GO
GRANT SELECT ON doc_header TO CdrPublishing
GO
GRANT SELECT ON last_doc_publication TO CdrGuest
GO
GRANT SELECT ON last_doc_publication TO CdrPublishing
GO
GRANT SELECT ON orphan_terms TO CdrGuest
GO
GRANT SELECT ON orphan_terms TO CdrPublishing
GO
GRANT SELECT ON pub_event TO CdrGuest
GO
GRANT SELECT ON pub_event TO CdrPublishing
GO
GRANT SELECT ON published_doc TO CdrGuest
GO
GRANT SELECT ON published_doc TO CdrPublishing
GO
GRANT SELECT ON pushed_doc TO CdrGuest
GO
GRANT SELECT ON pushed_doc TO CdrPublishing
GO
GRANT SELECT ON removed_doc TO CdrGuest
GO
GRANT SELECT ON removed_doc TO CdrPublishing
GO
GRANT SELECT ON publishable_version TO CdrGuest
GO
GRANT SELECT ON publishable_version TO CdrPublishing
GO
GRANT SELECT ON term_children TO CdrGuest
GO
GRANT SELECT ON term_children TO CdrPublishing
GO
GRANT SELECT ON term_kids TO CdrGuest
GO
GRANT SELECT ON term_kids TO CdrPublishing
GO
GRANT SELECT ON term_parents TO CdrGuest
GO
GRANT SELECT ON term_parents TO CdrPublishing
GO
GRANT SELECT ON TermsWithParents TO CdrGuest
GO
GRANT SELECT ON TermsWithParents TO CdrPublishing
GO
GRANT SELECT ON failed_login_attempts TO CdrGuest
GO
GRANT EXECUTE ON cdr_get_count_of_links_to_persons TO CdrGuest
GO
GRANT SELECT ON doc_info TO CdrGuest
GO
GRANT SELECT ON docs_with_pub_status TO CdrGuest
GO
GRANT SELECT ON primary_pub_job TO CdrGuest
GO
GRANT SELECT ON primary_pub_doc TO CdrGuest
GO
GRANT SELECT, UPDATE, INSERT ON batch_job TO CdrPublishing
GO
GRANT SELECT ON batch_job TO CdrGuest
GO
GRANT SELECT, UPDATE, INSERT ON batch_job_parm TO CdrPublishing
GO
GRANT SELECT ON batch_job_parm TO CdrGuest
GO
GRANT SELECT ON filter_set TO CdrGuest
GO
GRANT SELECT ON filter_set_member TO CdrGuest
GO
GRANT SELECT ON zipcode TO CdrGuest
GO
GRANT SELECT, UPDATE, INSERT, DELETE ON query TO CdrGuest
GO
GRANT SELECT ON ctgov_import TO CdrGuest
GO
GRANT SELECT ON ctgov_disposition TO CdrGuest
GO
GRANT SELECT ON ctgov_import_job TO CdrGuest
GO
GRANT SELECT ON ctgov_import_event TO CdrGuest
GO
GRANT SELECT ON external_map TO CdrGuest
GO
GRANT SELECT ON external_map_usage TO CdrGuest
GO
GRANT SELECT ON ctgov_export TO CdrGuest
GO
GRANT SELECT, INSERT ON ctgov_export TO CdrPublishing
GO
GRANT SELECT ON external_map_rule TO CdrGuest
GO
GRANT SELECT ON import_disposition TO CdrGuest
GO
GRANT SELECT ON import_source TO CdrGuest
GO
GRANT SELECT ON import_doc TO CdrGuest
GO
GRANT SELECT ON import_job TO CdrGuest
GO
GRANT SELECT ON import_event TO CdrGuest
GO
GRANT SELECT ON external_map_type TO CdrGuest
GO
GRANT EXECUTE ON get_prot_person_connections TO CdrGuest
GO
GRANT EXECUTE ON get_prot_person_connections TO CdrPublishing
GO
GRANT SELECT ON pub_proc_nlm TO CdrGuest
GO
GRANT SELECT ON doc_save_action TO CdrGuest
GO
GRANT SELECT ON doc_last_save TO CdrGuest
GO
GRANT SELECT ON doc_save_action TO CdrPublishing
GO
GRANT SELECT ON doc_last_save TO CdrPublishing
GO