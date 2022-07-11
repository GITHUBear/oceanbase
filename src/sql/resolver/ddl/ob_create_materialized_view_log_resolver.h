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

#ifndef OCEANBASE_SQL_RESOLVER_DDL_CREATE_MATERIALIZED_VIEW_LOG_RESOLVER_H_
#define OCEANBASE_SQL_RESOLVER_DDL_CREATE_MATERIALIZED_VIEW_LOG_RESOLVER_H_

#include "sql/resolver/ddl/ob_ddl_resolver.h"
#include "share/schema/ob_table_schema.h"
#include "lib/hash/ob_hashset.h"

namespace oceanbase {
namespace sql {
class ObCreateMaterializedViewLogResolver : public ObDDLResolver {
    static const int64_t BASE_TABLE_NAME_NODE = 0;
    static const int64_t CML_NODE_NUM = 1;
public:
    explicit ObCreateMaterializedViewLogResolver(ObResolverParams& params);
    virtual ~ObCreateMaterializedViewLogResolver();

    virtual int resolve(const ParseNode& parse_tree);

private:
    DISALLOW_COPY_AND_ASSIGN(ObCreateMaterializedViewLogResolver);
};
}
}

#endif