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

#ifndef _OB_CLONE_TENANT_RESOLVER_H
#define _OB_CLONE_TENANT_RESOLVER_H
#include "sql/resolver/cmd/ob_cmd_resolver.h"
namespace oceanbase
{
namespace sql
{
class ObCloneTenantResolver : public ObCMDResolver
{
public:
  explicit ObCloneTenantResolver(ObResolverParams &params);
  virtual ~ObCloneTenantResolver();

  virtual int resolve(const ParseNode &parse_tree);
private:
  int resolve_option_list_(const ParseNode *node,
                           ObString &resource_pool_name,
                           ObString &unit_config_name);
private:
  DISALLOW_COPY_AND_ASSIGN(ObCloneTenantResolver);
};

}
}

#endif /*_OB_CREATE_SNAPSHOT_RESOLVER_H*/
