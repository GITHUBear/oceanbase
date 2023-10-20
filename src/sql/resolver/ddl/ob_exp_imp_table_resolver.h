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

#ifndef OCEANBASE_SRC_SQL_RESOLVER_DDL_OB_EXP_IMP_TABLE_RESOLVER_H_
#define OCEANBASE_SRC_SQL_RESOLVER_DDL_OB_EXP_IMP_TABLE_RESOLVER_H_

#include "sql/resolver/ddl/ob_ddl_resolver.h"

namespace oceanbase 
{
namespace sql
{

class ObExpImpTableResolver : public ObDDLResolver
{
public:
    explicit ObExpImpTableResolver(ObResolverParams &params);
    virtual ~ObExpImpTableResolver() {}

    virtual int resolve(const ParseNode &parse_tree);
private:
    DISALLOW_COPY_AND_ASSIGN(ObExpImpTableResolver);
};

}
}

#endif