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

#ifndef OCEANBASE_STORAGE_OB_DIRECT_EXPORT_H_
#define OCEANBASE_STORAGE_OB_DIRECT_EXPORT_H_

#include "common/ob_tablet_id.h"
#include "share/ob_ls_id.h"
#include "storage/meta_mem/ob_tablet_handle.h"

namespace oceanbase 
{
namespace storage 
{
class ObDirectExport 
{
public:
    ObDirectExport() : is_inited_(false) {}
    ~ObDirectExport() {
        is_inited_ = false;
        allocator_.reset();
    }
    int init(share::ObLSID ls_id, uint64_t tenant_id, uint64_t table_id,
             ObTabletID tablet_id, ObString path);

    int dump_minor_sst();
    int dump_major_sst();
    int dump_autoinc_seq_value();
private:
    int dump_sst(ObITable *table, ObTabletHandle &tablet_handle, bool is_minor);
    const int PATH_LEN = 1000;
    bool is_inited_;
    common::ObArenaAllocator allocator_;
    share::ObLSID ls_id_;
    uint64_t tenant_id_;
    uint64_t table_id_;
    ObTabletID tablet_id_;
    ObString dump_dir_;
};

} // namespace storage
} // namespace oceanbase
#endif