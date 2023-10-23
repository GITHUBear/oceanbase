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
#define USING_LOG_PREFIX SQL_EXE
#include "sql/engine/cmd/ob_pump_table_executor.h"
#include "sql/resolver/ddl/ob_exp_imp_table_stmt.h"
#include "sql/engine/cmd/ob_ddl_executor_util.h"

namespace oceanbase
{
namespace sql
{

int ObPumpTableExecutor::execute(ObExecContext &ctx, ObExpImpTableStmt &stmt)
{
    int ret = OB_SUCCESS;
    ObTaskExecutorCtx *task_exec_ctx = NULL;
    obrpc::ObCommonRpcProxy *common_rpc_proxy = NULL;
    const obrpc::ObExpdpImpdpTableArg &pump_table_arg = stmt.get_pump_table_arg();
    obrpc::ObExpdpImpdpTableArg &tmp_arg = const_cast<obrpc::ObExpdpImpdpTableArg&>(pump_table_arg);
    ObString first_stmt;
    ObSQLSessionInfo *my_session = NULL;
    if (OB_FAIL(stmt.get_first_stmt(first_stmt))) {
        LOG_WARN("get first statement failed", K(ret));
    } else {
        int64_t affected_rows = 0;
        tmp_arg.ddl_stmt_str_ = first_stmt;
        tmp_arg.consumer_group_id_ = THIS_WORKER.get_group_id();
        my_session = ctx.get_my_session();
        if (NULL == my_session) {
            ret = OB_ERR_UNEXPECTED;
            LOG_WARN("failed to get my session", K(ret), K(ctx));
        } else if (OB_FALSE_IT(tmp_arg.exec_tenant_id_ = my_session->get_effective_tenant_id())) {
        } else if (OB_ISNULL(task_exec_ctx = GET_TASK_EXECUTOR_CTX(ctx))) {
            ret = OB_NOT_INIT;
            LOG_WARN("get task executor context failed");
        } else if (OB_FAIL(task_exec_ctx->get_common_rpc(common_rpc_proxy))) {
            LOG_WARN("get common rpc proxy failed", K(ret));
        } else if (OB_ISNULL(common_rpc_proxy)){
            ret = OB_ERR_UNEXPECTED;
            LOG_WARN("common rpc proxy should not be null", K(ret));
        } /*else if (pump_table_arg.data_pump_mode_ == obrpc::ObExpdpImpdpTableArg::PumpMode::PUMP_MODE_EXPDP &&
                   OB_FAIL(common_rpc_proxy->expdp_table(pump_table_arg))) {
            LOG_WARN("rpc proxy expdp table failed", K(ret), "dst", common_rpc_proxy->get_server());
        } else if (pump_table_arg.data_pump_mode_ == obrpc::ObExpdpImpdpTableArg::PumpMode::PUMP_MDOE_IMPDP &&
                   OB_FAIL(common_rpc_proxy->impdp_table(pump_table_arg))) {
            LOG_WARN("rpc proxy impdp table failed", K(ret), "dst", common_rpc_proxy->get_server());
        }*/ 
        // Only use in standalone mode
        else if (GCTX.srv_rpc_proxy_->to(GCTX.self_addr()).by(MTL_ID()).pump_table(pump_table_arg)) {
            LOG_WARN("pump_table failed", K(ret), "dst", common_rpc_proxy->get_server());
        }
    }
    return ret;
}

} // namespace sql
} // namespace oceanbase