/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PubL v2. You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PubL v2 for more details.
 */

#include "storage/data_pump/ob_direct_export.h"

namespace oceanbase
{
namespace storage
{

int ObDirectExport::init(share::ObLSID ls_id, uint64_t tenant_id, uint64_t table_id,
                         ObTabletID tablet_id, ObString path)
{
    int ret = OB_SUCCESS;

    return ret;
}

int ObDirectExport::dump_minor_sst()
{
    int ret = OB_SUCCESS;
    return ret;
}
int ObDirectExport::dump_major_sst()
{
    int ret = OB_SUCCESS;
    return ret;
}
int ObDirectExport::dump_autoinc_seq_value()
{
    int ret = OB_SUCCESS;
    return ret;
}

int ObDirectExport::dump_sst(ObITable *table, ObTabletHandle &tablet_handle, bool is_minor)
{
    int ret = OB_SUCCESS;
    return ret;
}

}
}