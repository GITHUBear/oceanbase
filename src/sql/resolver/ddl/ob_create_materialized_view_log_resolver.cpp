/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#define USING_LOG_PREFIX SQL_RESV

#include "sql/resolver/ddl/ob_create_materialized_view_log_resolver.h"
#include "sql/resolver/ob_resolver_utils.h"
#include "sql/resolver/ddl/ob_create_table_stmt.h"
#include "sql/resolver/dml/ob_dml_stmt.h"
#include "sql/ob_sql_context.h"
#include "sql/session/ob_sql_session_info.h"

namespace oceanbase {
using namespace common;
using namespace obrpc;
using namespace share::schema;
namespace sql {
ObCreateMaterializedViewLogResolver::ObCreateMaterializedViewLogResolver(ObResolverParams& params) : ObDDLResolver(params)
{}

ObCreateMaterializedViewLogResolver::~ObCreateMaterializedViewLogResolver()
{}

int ObCreateMaterializedViewLogResolver::resolve(const ParseNode& parse_tree) 
{
    int ret = OB_SUCCESS;
    if (OB_UNLIKELY(T_CREATE_MATERIALIZED_VIEW_LOG != parse_tree.type_)) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("unexpected type of parse_tree", K(parse_tree.type_), K(ret));
    } else if (OB_ISNULL(parse_tree.children_)) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("children should not be NULL", K(ret));
    } else if (OB_UNLIKELY(CML_NODE_NUM != parse_tree.num_child_)) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("unexpected num_child of parse_tree", K(parse_tree.num_child_), K(ret));
    } else if (OB_ISNULL(allocator_)) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("allocator_ should not be NULL", K(ret));
    } else if (OB_ISNULL(session_info_)) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("session_info_ should not be NULL", K(ret));
    } else if (OB_ISNULL(params_.query_ctx_)) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("query_ctx should not be NULL", K(ret));
    } else {
        uint64_t col_cnt = 0;
        ObSchemaChecker* schema_checker = params_.schema_checker_;
        ObCreateTableStmt* create_table_stmt = NULL;
        if (OB_UNLIKELY(NULL == (create_table_stmt = create_stmt<ObCreateTableStmt>()))) {
            ret = OB_ALLOCATE_MEMORY_FAILED;
            LOG_ERROR("create view stmt failed", K(ret));
        } else {
            create_table_stmt->set_allocator(*allocator_);
            stmt_ = create_table_stmt;
        }
        ObCreateTableArg& create_arg = create_table_stmt->get_create_table_arg();
        ObTableSchema& table_schema = create_arg.schema_;
        ParseNode* base_table_name_node = parse_tree.children_[BASE_TABLE_NAME_NODE];
        if (OB_ISNULL(base_table_name_node)) {
            ret = OB_ERR_UNEXPECTED;
            LOG_WARN("base table name node should not be null", K(ret));
        } else {
            const uint64_t tenant_id = session_info_->get_effective_tenant_id();
            ObString database_name = session_info_->get_database_name();
            uint64_t database_id = 0;
            bool base_table_exists = false, mv_log_table_exists = false;
            ObString base_table_name, mv_log_table_name_str;
            base_table_name.assign_ptr(base_table_name_node->str_value_, base_table_name_node->str_len_);
            uint64_t base_table_id = 0;
            // 1. check if table exists
            if (OB_FAIL(schema_checker->get_database_id(tenant_id, database_name, database_id))) {
                ret = OB_ERR_UNEXPECTED;
                LOG_WARN("fail to get database id", K(ret));
            } else if (OB_FAIL(schema_checker->check_table_exists(tenant_id, database_id, base_table_name, false, base_table_exists))) {
                ret = OB_ERR_UNEXPECTED;
                LOG_WARN("fail to check table exists", K(ret));
            }  else if (!base_table_exists) {
                ret = OB_NOT_SUPPORTED;
                LOG_WARN("base table not found", K(ret));
            } else if (OB_FAIL(schema_checker->get_schema_guard()->get_table_id(tenant_id, database_id, base_table_name, 
                                                                              false, ObSchemaGetterGuard::ALL_TYPES, base_table_id))) {
                ret = OB_ERR_UNEXPECTED;
                LOG_WARN("fail to get table id", K(ret));
            }
            // 2. set MV Log table name: base_table_name_mvlog
            if (OB_SUCC(ret)) {
                char* mv_log_table_name = nullptr;
                mv_log_table_name = static_cast<char*>(allocator_->alloc(base_table_name_node->str_len_ + 7));
                if (OB_ISNULL(mv_log_table_name)) {
                    ret = OB_ALLOCATE_MEMORY_FAILED;
                    LOG_ERROR("Failed to malloc new table name string", K(ret));
                } else {
                    memmove(mv_log_table_name, base_table_name_node->str_value_, base_table_name_node->str_len_);
                    memmove(mv_log_table_name + base_table_name_node->str_len_, "_mvlog", 6);
                    mv_log_table_name[base_table_name_node->str_len_ + 6] = '\0';
                    mv_log_table_name_str.assign_ptr(mv_log_table_name, base_table_name_node->str_len_ + 6);
                }
            }
            // 3. get base table schema & column schemas
            const ObTableSchema* base_table_schema = nullptr;
            ObArray<const ObColumnSchemaV2*> base_table_col_schema;
            if (OB_SUCC(ret)) {
                if (OB_FAIL(schema_checker->get_table_schema(base_table_id, base_table_schema))) {
                    ret = OB_ERR_UNEXPECTED;
                    LOG_WARN("fail to get table schema", K(ret));
                } else {
                    for (int64_t i = 0; i < base_table_schema->get_column_count(); ++i) {
                        base_table_col_schema.push_back(base_table_schema->get_column_schema_by_idx(i));
                    }
                }
            }
            // 4. set create_table_stmt
            create_table_stmt->set_if_not_exists(true);
            if (OB_FAIL(ob_write_string(*allocator_, database_name, create_table_stmt->get_non_const_db_name()))) {
                ret = OB_ERR_UNEXPECTED;
                LOG_WARN("fail to deep copy database name to stmt", K(ret));
            } else if (OB_FAIL(table_schema.set_table_name(mv_log_table_name_str))) {
                ret = OB_ERR_UNEXPECTED;
                LOG_WARN("fail to set table name", K(ret));
            } else {
                // root service need database_name instead of database_id for create operation,
                // but the temp schema_checker need database_id, so set any value here,
                // and reset to OB_INVALID_ID when resolve done.
                create_table_stmt->set_database_id(generate_table_id());
            }
            table_schema.set_tenant_id(tenant_id);
            table_schema.set_database_id(database_id);
            const ObColumnSchemaV2* tmp = nullptr;
            bool find_pk = false;
            int64_t first_hidden_rk_idx = -1, first_rk_idx = -1;
            uint64_t cur_column_id = OB_APP_MIN_COLUMN_ID - 1;
            for (int64_t i = 0; OB_SUCC(ret) && i < base_table_col_schema.size(); ++i) {
                tmp = base_table_col_schema.at(i);
                if (tmp->is_hidden()) {
                    if (first_hidden_rk_idx == -1) {
                        first_hidden_rk_idx = i;
                    }
                    continue;
                }
                if (tmp->is_rowkey_column()) {
                    if (find_pk) {
                        ret = OB_NOT_SUPPORTED;
                        LOG_WARN("multi pk/rowkeys in non-hidden column is not supported", K(ret));
                    } else {
                        find_pk = true;
                        first_rk_idx = i;
                    }
                }
                ObColumnSchemaV2 new_col_schema;
                new_col_schema.reset();
                new_col_schema.set_tenant_id(tenant_id);
                new_col_schema.set_column_id(++cur_column_id);
                new_col_schema.set_data_type(tmp->get_data_type());
                new_col_schema.set_data_length(tmp->get_data_length());
                new_col_schema.set_charset_type(tmp->get_charset_type());
                new_col_schema.set_collation_type(tmp->get_collation_type());
                if (OB_FAIL(new_col_schema.set_column_name(tmp->get_column_name()))) {
                    ret = OB_ERR_UNEXPECTED;
                    LOG_WARN("failed to set column name", K(ret));
                } else if (OB_FAIL(table_schema.add_column(new_col_schema))) {
                    ret = OB_ERR_UNEXPECTED;
                    LOG_WARN("add column schema failed", K(ret));
                }
                col_cnt = table_schema.get_column_count();
            }
            // add '_primary_key' column
            if (OB_SUCC(ret)) {
                if (!find_pk) {
                    // use first hidden rowkey as '_primary_key' column
                    if (first_hidden_rk_idx == -1) {
                        ret = OB_ERR_UNEXPECTED;
                        LOG_WARN("first_hidden_rk_idx can not be -1 here", K(ret));
                    } else {
                        const ObColumnSchemaV2* tmp = base_table_col_schema.at(first_hidden_rk_idx); 
                        ObColumnSchemaV2 pk_column;
                        pk_column.reset();
                        pk_column.set_tenant_id(tenant_id);
                        pk_column.set_column_id(++cur_column_id);
                        pk_column.set_nullable(false);
                        pk_column.set_data_type(tmp->get_data_type());
                        pk_column.set_data_length(tmp->get_data_length());
                        pk_column.set_charset_type(tmp->get_charset_type());
                        pk_column.set_collation_type(tmp->get_collation_type());
                        if (OB_FAIL(pk_column.set_column_name("_primary_key"))) {
                            ret = OB_ERR_UNEXPECTED;
                            LOG_WARN("failed to set column name", K(ret));
                        } else {
                            if (OB_FAIL(table_schema.add_column(pk_column))) {
                                ret = OB_ERR_UNEXPECTED;
                                LOG_WARN("add column schema failed", K(ret));
                            }
                        }
                    }
                } else {
                    // use first single rowkey as '_primary_key' column
                    if (first_rk_idx == -1) {
                        ret = OB_ERR_UNEXPECTED;
                        LOG_WARN("first_rk_idx can not be -1 here", K(ret));
                    } else {
                        const ObColumnSchemaV2* tmp = base_table_col_schema.at(first_rk_idx); 
                        ObColumnSchemaV2 pk_column;
                        pk_column.reset();
                        pk_column.set_tenant_id(tenant_id);
                        pk_column.set_column_id(++cur_column_id);
                        pk_column.set_nullable(false);
                        pk_column.set_data_type(tmp->get_data_type());
                        pk_column.set_data_length(tmp->get_data_length());
                        pk_column.set_charset_type(tmp->get_charset_type());
                        pk_column.set_collation_type(tmp->get_collation_type());
                        if (pk_column.set_column_name("_primary_key")) {
                            ret = OB_ERR_UNEXPECTED;
                            LOG_WARN("failed to set column name", K(ret));
                        } else {
                            if (OB_FAIL(table_schema.add_column(pk_column))) {
                                ret = OB_ERR_UNEXPECTED;
                                LOG_WARN("add column schema failed", K(ret));
                            }
                        }
                    }
                }
            }
            col_cnt = table_schema.get_column_count();
            // add '_dml_type' column
            if (OB_SUCC(ret)) {
                ObColumnSchemaV2 dml_type_column;
                dml_type_column.reset();
                dml_type_column.set_tenant_id(tenant_id);
                dml_type_column.set_is_hidden(false);
                dml_type_column.set_column_id(++cur_column_id);
                dml_type_column.set_data_type(ObUInt64Type);
                dml_type_column.set_nullable(false);
                dml_type_column.set_charset_type(CHARSET_BINARY);
                dml_type_column.set_collation_type(CS_TYPE_BINARY);
                if (OB_FAIL(dml_type_column.set_column_name("_dml_type"))) {
                    ret = OB_ERR_UNEXPECTED;
                    LOG_WARN("failed to set column name", K(ret));
                } else if (table_schema.add_column(dml_type_column)) {
                    ret = OB_ERR_UNEXPECTED;
                    LOG_WARN("add column schema failed", K(ret));
                }
            }
            // add 'seqno' column
            if (OB_SUCC(ret)) {
                ObColumnSchemaV2 seqno_column;
                seqno_column.reset();
                seqno_column.set_tenant_id(tenant_id);
                seqno_column.set_is_hidden(false);
                seqno_column.set_column_id(++cur_column_id);
                seqno_column.set_data_type(ObUInt64Type);
                seqno_column.set_nullable(false);
                seqno_column.set_charset_type(CHARSET_BINARY);
                seqno_column.set_collation_type(CS_TYPE_BINARY);
                if (OB_FAIL(seqno_column.set_column_name("_seqno"))) {
                    ret = OB_ERR_UNEXPECTED;
                    LOG_WARN("failed to set column name", K(ret));
                } else if (table_schema.add_column(seqno_column)) {
                    ret = OB_ERR_UNEXPECTED;
                    LOG_WARN("add column schema failed", K(ret));
                }
            }
            col_cnt = table_schema.get_column_count();
            // add sequence col
            ObColumnSchemaV2 hidden_pk;
            hidden_pk.reset();
            hidden_pk.set_tenant_id(tenant_id);
            hidden_pk.set_column_id(OB_HIDDEN_PK_INCREMENT_COLUMN_ID);  // reserved for hidden primary key
            hidden_pk.set_data_type(ObUInt64Type);
            hidden_pk.set_nullable(false);
            hidden_pk.set_autoincrement(true);
            hidden_pk.set_is_hidden(true);
            hidden_pk.set_charset_type(CHARSET_BINARY);
            hidden_pk.set_collation_type(CS_TYPE_BINARY);
            if (OB_SUCC(ret)) {
                if (OB_FAIL(hidden_pk.set_column_name(OB_HIDDEN_PK_INCREMENT_COLUMN_NAME))) {
                    ret = OB_ERR_UNEXPECTED;
                    LOG_WARN("failed to set column name", K(ret));
                } else {
                    hidden_pk.set_rowkey_position(1);
                    if (OB_FAIL(table_schema.add_column(hidden_pk))) {
                        ret = OB_ERR_UNEXPECTED;
                        LOG_WARN("add column schema failed", K(ret));
                    }
                }
            }
            col_cnt = table_schema.get_column_count();
        }
    }
    return ret;
}

}
}