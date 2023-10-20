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

#include "sql/resolver/ddl/ob_exp_imp_table_resolver.h"
#include "sql/resolver/ddl/ob_exp_imp_table_stmt.h"
#include "sql/session/ob_sql_session_info.h"

namespace oceanbase
{
using namespace common;
using obrpc::ObTableItem;
namespace sql
{
ObExpImpTableResolver::ObExpImpTableResolver(ObResolverParams &params)
    : ObDDLResolver(params)
{}

int ObExpImpTableResolver::resolve(const ParseNode &parse_tree)
{
    int ret = OB_SUCCESS;
    ObExpImpTableStmt *pump_table_stmt = NULL;
    const ParseNode* dumpfile_path_node = NULL;
    const ParseNode* relation_node = NULL;
    if (OB_ISNULL(session_info_) || OB_ISNULL(allocator_) ||
        (T_EXPDP != parse_tree.type_ &&  T_IMPDP != parse_tree.type_) ||
        (2 != parse_tree.num_child_) || OB_ISNULL(parse_tree.children_) ||
        OB_ISNULL(dumpfile_path_node = parse_tree.children_[0]) || 
        OB_ISNULL(relation_node = parse_tree.children_[1])) {
        ret = OB_ERR_UNEXPECTED;
        SQL_RESV_LOG(WARN, "invalid parse tree!", K(ret));
    } else if (OB_UNLIKELY(relation_node->num_child_ != 3 || 
                           OB_NOT_NULL(relation_node->children_[2]))) {
        ret = OB_NOT_SUPPORTED;
        SQL_RESV_LOG(WARN, "<relation_name>.<mysql_reserved_keyword> & opt_dblink is not supported", K(ret));
    } else if (OB_ISNULL(pump_table_stmt = create_stmt<ObExpImpTableStmt>())) {
        ret = OB_ALLOCATE_MEMORY_FAILED;
        SQL_RESV_LOG(ERROR, "create drop table stmt failed");
    } else {
        stmt_ = pump_table_stmt;
    }

    if (OB_SUCC(ret)) {
        obrpc::ObExpdpImpdpTableArg &pump_table_arg = pump_table_stmt->get_pump_table_arg();

        pump_table_arg.tenant_id_ = session_info_->get_effective_tenant_id();
        if (T_EXPDP == parse_tree.type_) {
            pump_table_arg.data_pump_mode_ = obrpc::ObExpdpImpdpTableArg::PumpMode::PUMP_MODE_EXPDP;
        } else {
            pump_table_arg.data_pump_mode_ = obrpc::ObExpdpImpdpTableArg::PumpMode::PUMP_MDOE_IMPDP;
        }
        pump_table_arg.dumpfile_path_.assign_ptr(const_cast<char*>(dumpfile_path_node->str_value_), dumpfile_path_node->str_len_);

        ObString db_name;
        ObString table_name;
        obrpc::ObTableItem table_item;
        bool is_exists = false;
        if (OB_FAIL(resolve_table_relation_node(relation_node, table_name, db_name))) {
            SQL_RESV_LOG(WARN, "failed to resolve table relation node!", K(ret));
        } else if (OB_FAIL(session_info_->get_name_case_mode(table_item.mode_))) {
            SQL_RESV_LOG(WARN, "failed to get name case mode!", K(ret));
        } else {
            table_item.database_name_ = db_name;
            table_item.table_name_ = table_name;
            if (OB_FAIL(pump_table_arg.tables_.push_back(table_item))) {
                SQL_RESV_LOG(WARN, "failed to add table item!", K(table_item), K(ret));
            } else if (T_EXPDP != parse_tree.type_) {
                // do nothing
            } else if (OB_FAIL(schema_checker_->check_table_exists(session_info_->get_effective_tenant_id(),
                                                            db_name,
                                                            table_name,
                                                            false,
                                                            false/*is_hidden*/,
                                                            is_exists))) {
                SQL_RESV_LOG(WARN, "failed to check table or view exists", K(db_name),
                                K(table_name), K(ret));
            } else if (!is_exists) {
                ret = OB_TABLE_NOT_EXIST;
                SQL_RESV_LOG(WARN, "table not exists", K(db_name),
                                K(table_name), K(ret));
            }
        }
    }
    return ret;
}

} // namespace sql
} // namespace oceanbase