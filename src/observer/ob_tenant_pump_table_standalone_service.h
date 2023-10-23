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
 
#ifndef OCEANBASE_ROOTSERVER_OB_TENANT_PUMP_TABLE_SERVICE_H
#define OCEANBASE_ROOTSERVER_OB_TENANT_PUMP_TABLE_SERVICE_H

#include "share/ob_rpc_struct.h"

namespace oceanbase {
namespace observer { // instance level

class ObTenantPumpTableStandaloneService 
{
public:
    static int mtl_init(ObTenantPumpTableStandaloneService *&service);

    ObTenantPumpTableStandaloneService(): is_inited_(false) {}
    virtual ~ObTenantPumpTableStandaloneService() {}
    int init();
    int start();
    int stop() { return OB_SUCCESS; }
    void wait() {}
    void destroy() {}
    int do_work(const obrpc::ObExpdpImpdpTableArg& arg);
private:
    int fetch_ls_location(uint64_t tenant_id, const common::ObTabletID &tablet_id, 
                          share::ObLSLocation& ls_location, share::ObLSID& ls_id);
    int expdp_table_work(const obrpc::ObExpdpImpdpTableArg& arg);
    int impdp_table_work(const obrpc::ObExpdpImpdpTableArg& arg);
    bool is_inited_;

    // TODO: start QUEUE_THREAD
};

}
}

#endif