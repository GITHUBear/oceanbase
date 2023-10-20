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

#ifndef OCEANBASE_SQL_RESOLVER_DDL_DROP_TABLE_STMT_
#define OCEANBASE_SQL_RESOLVER_DDL_DROP_TABLE_STMT_

#include "share/ob_rpc_struct.h"
#include "sql/resolver/ddl/ob_ddl_stmt.h"

namespace oceanbase
{
namespace sql
{

class ObExpImpTableStmt : public ObDDLStmt
{
public:
    ObExpImpTableStmt() : ObDDLStmt(stmt::T_PUMP_TABLE), pump_table_args_() {}
    virtual ~ObExpImpTableStmt() {}
    
    const obrpc::ObExpdpImpdpTableArg &get_pump_table_arg() const { return pump_table_args_; }
    obrpc::ObExpdpImpdpTableArg &get_pump_table_arg() { return pump_table_args_; }
    
    obrpc::ObDDLArg &get_ddl_arg() override { return pump_table_args_; }
private:
    obrpc::ObExpdpImpdpTableArg pump_table_args_;
    DISALLOW_COPY_AND_ASSIGN(ObExpImpTableStmt);
};

}
}

#endif