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

#define USING_LOG_PREFIX SQL_OPT
#include "ob_log_delete.h"
#include "ob_log_plan.h"
#include "sql/optimizer/ob_optimizer_context.h"
#include "sql/resolver/dml/ob_delete_stmt.h"

using namespace oceanbase;
using namespace sql;
using namespace oceanbase::common;

int ObLogDelete::allocate_expr_pre(ObAllocExprContext& ctx)
{
  int ret = OB_SUCCESS;
  LOG_TRACE("allocate expr pre for delete", K(is_pdml()));
  if (OB_FAIL(add_table_columns_to_ctx(ctx))) {
    LOG_WARN("failed to add table columns to ctx", K(ret));
  } else if (OB_FAIL(ObLogicalOperator::allocate_expr_pre(ctx))) {
    LOG_WARN("failed to allocate expr pre", K(ret));
  } else if (is_pdml()) {
    if (OB_FAIL(alloc_partition_id_expr(ctx))) {
      LOG_WARN("failed alloc pseudo partition_id column for delete", K(ret));
    }
  } else { /*do nothing*/
  }
  return ret;
}

int ObLogDelete::allocate_expr_post(ObAllocExprContext& ctx)
{
  // TODO(wendongbo)：Only use for mvlog update, make sure normal delete don't go along this path(or has no side effect)
  int ret = OB_SUCCESS;
  ObDeleteStmt *stmt = static_cast<ObDeleteStmt*>(get_stmt());
  uint64_t mvlog_table_id = 0;
  if(OB_ISNULL(stmt)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("get unexpected null ptr", K(stmt), K(ret));
  } else {
    mvlog_table_id = stmt->get_mv_log_table_id();
  }
  
  for (int64_t i = 0; OB_SUCC(ret) && i < stmt->get_all_table_columns().count(); ++i) {
    const TableColumns cols = stmt->get_all_table_columns().at(i);
    for (int64_t j = 0; j < cols.index_dml_infos_.count(); ++j) {
      const IndexDMLInfo index_dml_info = cols.index_dml_infos_.at(j);
      LOG_INFO("delete op alloc expr, test for index dml info", K(index_dml_info.table_id_), K(index_dml_info.index_name_), K(index_dml_info.index_tid_));

      if (index_dml_info.index_tid_ == mvlog_table_id) {
        for (int64_t t = 0; t < cols.index_dml_infos_.at(j).column_exprs_.count(); ++t){
          ObColumnRefRawExpr* expr = const_cast<ObColumnRefRawExpr*>(cols.index_dml_infos_.at(j).column_exprs_.at(t));
          if (OB_NOT_NULL(expr)){
            // TODO: change this stupid method to some flag(when resolver resolves mvlog column exprs)
            if (expr->get_column_name()[0] == '_') {  // __pk_increment, _dml_type, _seqno are mvlog columns
              expr->add_flag(ObExprInfoFlag::IS_HIDDEN_MVLOG_COLUMN);
            }
            
            bool expr_is_required = false;
            if (OB_FAIL(mark_expr_produced(static_cast<ObRawExpr*>(expr), branch_id_, id_, ctx, expr_is_required))) {
              LOG_WARN("mark_expr_produced fail", K(ret));
            }
          }
        }
      }
    }
  }

  if (OB_SUCC(ret)) {
    if (OB_FAIL(ObLogDelUpd::allocate_expr_post(ctx))) {
      LOG_WARN("fail to allocatte expr post", K(ret));
    }
  }
  return ret;
}

const char* ObLogDelete::get_name() const
{
  const char* name = NULL;
  int ret = OB_SUCCESS;
  if (is_multi_part_dml()) {
    name = "MULTI PARTITION DELETE";
  } else if (is_pdml() && is_index_maintenance()) {
    name = "INDEX DELETE";
  } else {
    name = ObLogDelUpd::get_name();
  }
  return name;
}

int ObLogDelete::est_cost()
{
  int ret = OB_SUCCESS;
  ObLogicalOperator* child = NULL;
  if (OB_ISNULL(child = get_child(ObLogicalOperator::first_child))) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("child is null", K(ret), K(child));
  } else {
    set_op_cost(child->get_card() * static_cast<double>(DELETE_ONE_ROW_COST));
    set_cost(child->get_cost() + get_op_cost());
    set_card(child->get_card());
  }
  return ret;
}
