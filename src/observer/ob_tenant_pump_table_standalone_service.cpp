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
#define USING_LOG_PREFIX SERVER
#include "observer/ob_tenant_pump_table_standalone_service.h"
#include "share/location_cache/ob_location_service.h"
#include "observer/ob_server.h"
#include "storage/data_pump/ob_direct_export.h"

namespace oceanbase
{
namespace observer 
{

int ObTenantPumpTableStandaloneService::mtl_init(ObTenantPumpTableStandaloneService *&service)
{
    int ret = OB_SUCCESS;
    if (OB_ISNULL(service)) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("service is null", K(ret));
    } else if (OB_FAIL(service->init())) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("service init failed", K(ret));
    }
    return ret;
}

int ObTenantPumpTableStandaloneService::init() {  
    int ret = OB_SUCCESS;
    if (IS_INIT) {
        ret = OB_INIT_TWICE;
        LOG_WARN("init twice", K(ret));
    } else {
        is_inited_ = true;
    }
    return ret;
}

int ObTenantPumpTableStandaloneService::start() {
    int ret = OB_SUCCESS;
    if (IS_NOT_INIT) {
        ret = OB_NOT_INIT;
        LOG_WARN("ObTenantPumpTableStandaloneService not init", KR(ret), KP(this));
    }
    return ret;
}

int ObTenantPumpTableStandaloneService::do_work(const obrpc::ObExpdpImpdpTableArg& arg)
{
    int ret = OB_SUCCESS;
    if (arg.data_pump_mode_ == obrpc::ObExpdpImpdpTableArg::PumpMode::PUMP_MODE_EXPDP &&
        OB_FAIL(expdp_table_work(arg))) {
        LOG_WARN("do expdp table work failed", K(ret), K(arg));
    } else if (arg.data_pump_mode_ == obrpc::ObExpdpImpdpTableArg::PumpMode::PUMP_MDOE_IMPDP &&
               OB_FAIL(impdp_table_work(arg))) {
        LOG_WARN("do impdp table work failed", K(ret), K(arg));
    } else {
        ret = OB_INVALID_ARGUMENT;
        LOG_WARN("invalid argument", K(ret), K(arg));
    }
    return ret;
}

int ObTenantPumpTableStandaloneService::expdp_table_work(const obrpc::ObExpdpImpdpTableArg& arg)
{
    int ret = OB_SUCCESS;
    uint64_t tenant_id = arg.tenant_id_;
    const ObTableSchema* table_schema = nullptr;
    ObSchemaGetterGuard schema_guard;

    if (OB_UNLIKELY(arg.data_pump_mode_ != obrpc::ObExpdpImpdpTableArg::PumpMode::PUMP_MODE_EXPDP)) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("invalid data pump mode", K(ret), K(arg.data_pump_mode_));
    } else if (OB_UNLIKELY(!arg.is_valid())) {
        ret = OB_INVALID_ARGUMENT;
        LOG_WARN("invalid arg", K(ret), K(arg));
    } else if (OB_FAIL(ObMultiVersionSchemaService::get_instance().get_tenant_schema_guard(tenant_id, schema_guard))) {
        LOG_WARN("get tenant schema guard failed", K(ret), K(tenant_id));
    }

    for (int64_t i = 0; OB_SUCC(ret) && i < arg.tables_.count(); ++i) {
        const obrpc::ObTableItem& table_item = arg.tables_.at(i);
        table_schema = nullptr;
        ObArray<ObTabletID> tablet_ids;
        ObArray<uint64_t> part_ids;

        if (OB_FAIL(schema_guard.get_table_schema(tenant_id, table_item.database_name_, table_item.table_name_, 
                                        false, table_schema))) {
            LOG_WARN("get table schema failed", K(ret), K(table_item.database_name_), K(table_item.table_name_));
        } else if (OB_ISNULL(table_schema)) {
            ret = OB_ERR_UNEXPECTED;
            LOG_WARN("cannot find table schema", K(ret));
        } else if (OB_FAIL(table_schema->get_all_tablet_and_object_ids(tablet_ids, part_ids))) {
            LOG_WARN("get tablet & object ids failed", K(ret));
        }

        // TODO: metadata export
        uint64_t table_id = table_schema->get_table_id();
        for (int64_t j = 0; OB_SUCC(ret) && j < tablet_ids.count(); ++j) {
            const ObTabletID& tablet_id = tablet_ids.at(j);
            ObLSLocation location;
            ObLSID ls_id;
            ObAddr leader_addr;
            storage::ObDirectExport direct_exp;

            if (OB_FAIL(fetch_ls_location(tenant_id, tablet_id, location, ls_id))) {
                LOG_WARN("fetch ls location failed", K(ret), K(tenant_id), K(tablet_id));
            } else if (OB_FAIL(location.get_leader(leader_addr))) {
                LOG_WARN("get leader failed", K(ret), K(location));
            } else if (OB_UNLIKELY(leader_addr != MYADDR)) {
                ret = OB_NOT_SUPPORTED;
                LOG_WARN("distributed mode is not support", K(ret));
            } else if (OB_FAIL(direct_exp.init(ls_id, tenant_id, table_id, tablet_id, arg.dumpfile_path_))) {
                LOG_WARN("init direct export failed", K(ret), K(ls_id), K(tenant_id), K(table_id), K(tablet_id), K(arg.dumpfile_path_));
            } else if (OB_FAIL(direct_exp.dump_minor_sst())) {
                LOG_WARN("dump sst failed", K(ret));
            }
        }

    }

    return ret;
}

int ObTenantPumpTableStandaloneService::fetch_ls_location(uint64_t tenant_id, const common::ObTabletID &tablet_id, 
                                                          share::ObLSLocation& ls_location, share::ObLSID& ls_id)
{
    int ret = OB_SUCCESS;
    share::ObLocationService &location_service = OBSERVER.get_location_service();
    const int64_t cluster_id = GCONF.cluster_id.get_value();
    MAKE_TENANT_SWITCH_SCOPE_GUARD(tenant_guard);
    bool is_cache_hit = false;
    if (OB_FAIL(tenant_guard.switch_to(OB_SYS_TENANT_ID))) {
        LOG_WARN("fail to switch tenant", KR(ret), K(OB_SYS_TENANT_ID));
    } else if (OB_FAIL(location_service.get(tenant_id, tablet_id, INT64_MAX,
                                            is_cache_hit, ls_id))) {
        LOG_WARN("fail to get ls id", KR(ret), K(tenant_id));
    } else if (OB_FAIL(location_service.get(cluster_id, tenant_id, ls_id,
                                            INT64_MAX, is_cache_hit,
                                            ls_location))) {
        LOG_WARN("fail to get location", KR(ret));
    }
    return ret;
}

int ObTenantPumpTableStandaloneService::impdp_table_work(const obrpc::ObExpdpImpdpTableArg& arg)
{
    int ret = OB_SUCCESS;

    return ret;
}

}
}